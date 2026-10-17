"""Isolated Mock/Editor acceptance; preserve every run, including failures."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Launcher'))
from environment import load_config
from runtime import ROLES, Runtime, SingleInstance

DEFAULT_TESTS = {
    'DroneOps.PlanConfiguration.Workspace': 'Command',
    'DroneOps.PlanConfiguration.MapControls': 'Map',
}


def write_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')
    temporary.replace(path)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def isolated_config(http_port, ws_port, directory):
    if not (1024 <= http_port <= 65535 and 1024 <= ws_port <= 65535 and http_port != ws_port):
        raise ValueError('HTTP/WS must be different unprivileged ports')
    config = load_config(ROOT)
    canonical = (ROOT / 'Backend/build/Release/DroneBackend.exe').resolve()
    resolved = (ROOT / config['backend_executable']).resolve()
    if resolved != canonical or not canonical.is_file():
        raise RuntimeError('Build/select the normal Backend/build/Release/DroneBackend.exe; '
                           'phase-specific and legacy fallback binaries are not accepted')
    if not Path(config['ue_executable']).is_file():
        raise RuntimeError('The normal Launcher configuration does not resolve an Unreal Editor')
    config.update(http_endpoint=f'http://127.0.0.1:{http_port}',
                  ws_endpoint=f'ws://127.0.0.1:{ws_port}/ws',
                  runtime_directory=directory, mock_fixture='Backend/mock_execution.json')
    return config


def require_unused_ports(http_port, ws_port):
    # Never let Runtime._backend() reuse another user's compatible server.
    sockets = []
    try:
        for port in (http_port, ws_port):
            listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sockets.append(listener)
            if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            listener.bind(('127.0.0.1', port))
            listener.listen(1)
    except OSError as exc:
        raise RuntimeError('QA ports are occupied; choose unused --http-port and --ws-port') from exc
    finally:
        for listener in sockets:
            listener.close()


def start_owned_backend(runtime, http_port, ws_port):
    require_unused_ports(http_port, ws_port)
    runtime._backend()
    if runtime.existing_backend or 'Backend' not in runtime.owned:
        raise RuntimeError('Backend was not created by this isolated runtime; refusing test writes')


def cleanup_owned(runtime):
    errors = []
    try:
        runtime.stop()
    except Exception as exc:
        errors.append(str(exc))
        # This runtime has fresh, isolated Mock data. Record failed graceful
        # shutdown, then reclaim only retained owned handles, never external PIDs.
        for role in tuple(runtime.owned):
            try:
                runtime.stop_role(role)
            except Exception as stop_error:
                errors.append(f'{role}: {stop_error}')
    return errors


def summarize(report, test, expected, exit_code):
    report_file = report / 'index.json'
    index = json.loads(report_file.read_text(encoding='utf-8-sig')) if report_file.exists() else {}
    log_path = report / 'editor.log'
    log = log_path.read_text(encoding='utf-8-sig', errors='replace') if log_path.exists() else ''
    discoveries = [int(n) for n in re.findall(r'Found (\d+) automation tests based on', log)]
    completions = re.findall(r'Test Completed\. Result=\{([^}]+)\} Name=\{[^}]*\} Path=\{([^}]+)\}', log)
    report_tests = index.get('tests', [])
    paths = [entry.get('fullTestPath', '') for entry in report_tests]
    result = {
        'test': test, 'expected': expected, 'exit': exit_code,
        'discovered': len(report_tests), 'discovery_log_counts': discoveries,
        'completed': len(completions), 'completed_tests': [path for _, path in completions],
        'passed': index.get('succeeded', 0) + index.get('succeededWithWarnings', 0),
        'warnings': index.get('succeededWithWarnings', 0), 'failed': index.get('failed', 0),
        'not_run': index.get('notRun', 0), 'in_process': index.get('inProcess', 0),
        'report_tests': paths,
    }
    result['pass'] = (exit_code == 0 and report_file.exists()
                      and result['discovered'] == result['completed'] == result['passed'] == expected
                      and discoveries == [expected] and len(set(paths)) == expected
                      and sorted(paths) == sorted(result['completed_tests'])
                      and all(path == test or path.startswith(test + '.') for path in paths)
                      and all(state == 'Success' for state, _ in completions)
                      and result['failed'] == result['not_run'] == result['in_process'] == 0)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tests', nargs='*', help='Exact test names or test-path prefixes')
    parser.add_argument('--role', choices=ROLES, help='Override role for all selected tests')
    parser.add_argument('--expected', action='append', default=[], metavar='TEST=COUNT',
                        help='Expected discovered/completed count; default 1 per selected test')
    parser.add_argument('--http-port', type=int, default=22582)
    parser.add_argument('--ws-port', type=int, default=22583)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--null-rhi', action='store_true', help='Explicit component-only run; never native evidence')
    args = parser.parse_args(argv)
    if args.timeout < 1:
        parser.error('--timeout must be positive')
    tests = args.tests or list(DEFAULT_TESTS)
    expected = {}
    for item in args.expected:
        try:
            name, count = item.rsplit('=', 1)
            count = int(count)
            if name not in tests or count < 1:
                raise ValueError()
            expected[name] = count
        except ValueError:
            parser.error('--expected must name a selected test and a positive count')
    for test in tests:
        if not re.fullmatch(r'DroneOps(?:\.[A-Za-z0-9_]+)+', test):
            parser.error('Test names must be DroneOps dot-separated identifiers')
        if test not in DEFAULT_TESTS and not args.role:
            parser.error('Additional regression tests require an explicit --role')

    run = str(time.time_ns())
    out = ROOT / 'Evidence/PlanConfiguration' / ('verification-' + run)
    out.mkdir(parents=True)
    results = []
    binary = ROOT / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll'
    metadata = {'run': run, 'rendering': 'NullRHI component only' if args.null_rhi else 'RHI automation',
                'ue_module_sha256': sha256(binary) if binary.is_file() else None}
    write_json(out / 'run.json', metadata)
    guard = SingleInstance(f'http://127.0.0.1:{args.http_port}')
    try:
        for number, test in enumerate(tests):
            role = args.role or DEFAULT_TESTS[test]
            report = out / f'{number:02}-{test}'
            report.mkdir()
            runtime = None
            result = {'test': test, 'role': role, 'pass': False}
            try:
                config = isolated_config(args.http_port, args.ws_port,
                                         f'Saved/PlanConfiguration-verify-{run}-{number}')
                metadata['backend'] = str((ROOT / config['backend_executable']).resolve())
                metadata['backend_sha256'] = sha256(metadata['backend'])
                write_json(out / 'run.json', metadata)
                runtime = Runtime(ROOT, config)
                start_owned_backend(runtime, args.http_port, args.ws_port)
                fixture_script = {
                    'DroneOps.P4.Workflow.MapLiveDraftGuard': 'p4_ue_route_fixture.mjs',
                    'DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo': 'p4_workflow_integration.mjs',
                }.get(test)
                if fixture_script:
                    # Existing regressions declare a named route task or a
                    # deployed plan. Prepare only that case's isolated store.
                    fixture_env = dict(os.environ, P1_HTTP=config['http_endpoint'],
                                       P1_WS=config['ws_endpoint'],
                                       P54_ROUTE_FIXTURE=str(report / 'route-fixture.json'),
                                       P4_EVIDENCE_DIR=str(report / 'p4-fixture'))
                    with (report / 'fixture.log').open('w', encoding='utf-8') as log:
                        subprocess.run(['node', str(ROOT / 'Backend/tests' / fixture_script)],
                                       cwd=ROOT, env=fixture_env, stdout=log, stderr=subprocess.STDOUT,
                                       timeout=180, check=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                command = [config['ue_executable'], str(ROOT / config['project']), '/Game/Tests/P55OfflineMap',
                           '-P55OfflineVisualQA', '-ClientRole=' + role, '-P5Instance=' + str(uuid.uuid4()),
                           '-P1Http=' + config['http_endpoint'], '-P1Ws=' + config['ws_endpoint'],
                           '-unattended', '-nosplash', '-ExecCmds=Automation RunTests ' + test,
                           '-TestExit=Automation Test Queue Empty', '-ReportExportPath=' + str(report),
                           '-abslog=' + str(report / 'editor.log')]
                if args.null_rhi:
                    command.append('-NullRHI')
                write_json(report / 'invocation.json', {'role': role, 'arguments': command,
                                                       'runtime_directory': str(runtime.directory)})
                runtime._spawn(role, command, ROOT)
                process = runtime.owned[role].process
                try:
                    code = process.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    code = 'TIMEOUT'
                if code == 'TIMEOUT':
                    runtime.stop_role(role)
                result.update(summarize(report, test, expected.get(test, 1), code))
            except Exception as exc:
                result['error'] = f'{type(exc).__name__}: {exc}'
            finally:
                if runtime:
                    result['runtime_directory'] = str(runtime.directory)
                    result['cleanup_errors'] = cleanup_owned(runtime)
                    result['ownership_after_stop'] = [p.record() for p in runtime.owned.values()]
                    if result['cleanup_errors'] or result['ownership_after_stop']:
                        result['pass'] = False
                write_json(report / 'result.json', result)
                results.append(result)
                write_json(out / 'summary.json', results)
                print(json.dumps(result, ensure_ascii=False), flush=True)
    finally:
        guard.close()
        metadata['ue_module_sha256_after'] = sha256(binary) if binary.is_file() else None
        metadata['binary_unchanged'] = metadata['ue_module_sha256'] == metadata['ue_module_sha256_after']
        metadata['pass'] = bool(results) and all(r['pass'] for r in results) and metadata['binary_unchanged']
        write_json(out / 'run.json', metadata)
        print(out, flush=True)
    return 0 if metadata['pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
