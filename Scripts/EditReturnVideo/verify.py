"""Run edit-return/redeploy and explicit Map-to-Video protocol checks in an owned Mock host."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Launcher'))
sys.path.insert(0, str(ROOT / 'Scripts/ManualTakeover'))
from runtime import Runtime, SingleInstance
from verify import sha256, unused_ports, write_json

EXPECTED_CHECKS = 13
PROTECTED_PORTS = {19880, 19881, 22992, 22993}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--http-port', type=int, default=22994)
    parser.add_argument('--ws-port', type=int, default=22995)
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--node', default='node')
    args = parser.parse_args(argv)
    ports = (args.http_port, args.ws_port)
    if (any(not 1024 <= port <= 65535 or port in PROTECTED_PORTS for port in ports)
            or args.http_port == args.ws_port or args.timeout < 1):
        parser.error('Use distinct unprivileged QA ports, excluding production/native ports, and a positive timeout')
    binary = ROOT / 'Backend/build/Release/DroneBackend.exe'
    if not binary.is_file():
        parser.error('Build the normal Backend/build/Release/DroneBackend.exe first')
    run = str(time.time_ns())
    out = ROOT / 'Evidence/EditReturnVideo' / ('protocol-' + run)
    out.mkdir(parents=True)
    fixture = ROOT / 'Scripts/ManualTakeover/mock_fixture.json'
    config = {'backend_executable': str(binary),
              'http_endpoint': f'http://127.0.0.1:{args.http_port}',
              'ws_endpoint': f'ws://127.0.0.1:{args.ws_port}/ws',
              'runtime_directory': 'Saved/EditReturnVideo-' + run,
              'mock_fixture': str(fixture), 'mock_reset_on_start': False}
    result = {'run': run, 'pass': False, 'synthetic': True,
              'native_verified': False, 'real_hardware_verified': False,
              'backend': str(binary), 'backend_sha256': sha256(binary),
              'fixture_sha256': sha256(fixture), 'expected_checks': EXPECTED_CHECKS,
              'http_endpoint': config['http_endpoint'], 'ws_endpoint': config['ws_endpoint']}
    runtime = None
    guard = None
    try:
        guard = SingleInstance(config['http_endpoint'])
        unused_ports(*ports)
        runtime = Runtime(ROOT, config)
        runtime._backend()
        if runtime.existing_backend or 'Backend' not in runtime.owned:
            raise RuntimeError('This runner does not own the Backend; refusing test writes')
        result['owned_backend'] = runtime.owned['Backend'].record()
        result['runtime_directory'] = str(runtime.directory)
        environment = dict(os.environ, P1_HTTP=config['http_endpoint'], P1_WS=config['ws_endpoint'],
                           EDIT_RETURN_VIDEO_EVIDENCE=str(out))
        with (out / 'protocol.log').open('w', encoding='utf-8') as log:
            process = subprocess.run([args.node, str(ROOT / 'Scripts/EditReturnVideo/protocol.mjs')],
                                     cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                     timeout=args.timeout, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        result['exit_code'] = process.returncode
        protocol = json.loads((out / 'result.json').read_text(encoding='utf-8'))
        result['completed_checks'] = protocol.get('passed_count', 0)
        result['pass'] = (process.returncode == 0 and protocol.get('pass') is True
                          and result['completed_checks'] == EXPECTED_CHECKS)
    except Exception as error:
        result['error'] = f'{type(error).__name__}: {error}'
    finally:
        errors = []
        if runtime:
            try:
                runtime.stop()
            except Exception as error:
                errors.append(str(error))
                for role in tuple(runtime.owned):
                    try:
                        runtime.stop_role(role)
                    except Exception as stop_error:
                        errors.append(f'{role}: {stop_error}')
            result['ownership_after_stop'] = [item.record() for item in runtime.owned.values()]
            if result['ownership_after_stop']:
                result['pass'] = False
        if guard:
            guard.close()
        result['cleanup_errors'] = errors
        result['backend_sha256_after'] = sha256(binary)
        result['binary_unchanged'] = result['backend_sha256'] == result['backend_sha256_after']
        if errors or not result['binary_unchanged']:
            result['pass'] = False
        write_json(out / 'run.json', result)
        print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
        print(out, flush=True)
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
