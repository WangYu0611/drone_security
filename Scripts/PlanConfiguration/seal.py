"""Seal configuration acceptance after sanitization; preserve earlier failures."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Evidence/PlanConfiguration'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    target = OUT / 'manifest.json'
    if target.exists():
        raise RuntimeError('Evidence is already sealed; use a new revision directory')
    native = read(OUT / 'native/acceptance.json')
    assert native['owned_processes_stopped'] and native['final_visual_recheck_passed']
    latest = {}
    runs = []
    for folder in sorted(OUT.glob('verification-*')):
        metadata = read(folder / 'run.json')
        results = read(folder / 'summary.json')
        runs.append({'directory': folder.name, 'pass': metadata['pass'],
                     'ue_module_sha256': metadata['ue_module_sha256'],
                     'backend_sha256': metadata['backend_sha256']})
        for result in results:
            assert not result['cleanup_errors'] and not result['ownership_after_stop']
            latest[result['test']] = dict(result, directory=folder.name)
    expected = {'DroneOps.PlanConfiguration.Workspace', 'DroneOps.PlanConfiguration.MapControls',
                'DroneOps.P54.RouteEditCameraLock', 'DroneOps.P4.Workflow.MapLiveDraftGuard',
                'DroneOps.P4.Workflow.CommandLiveCRUD',
                'DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo',
                'DroneOps.P4.Localization.FTextAndDraftIsolation'}
    assert expected <= latest.keys(), expected - latest.keys()
    assert all(latest[name]['pass'] for name in expected)
    # Every failure remains inspectable; only the latest matching case is final.
    assert any(not run['pass'] for run in runs)
    logs = OUT / 'logs'
    logs.mkdir(exist_ok=True)
    for number in (1, 2, 3):
        source = ROOT / f'Saved/PlanConfiguration/build-{number}.log'
        text = source.read_text(encoding='utf-8-sig', errors='replace')
        text = re.sub(r'C:[\\/]Users[\\/][^\\/\s]+', '<USER>', text)
        (logs / source.name).write_text(text, encoding='utf-8')
    assert 'Result: Succeeded' in (logs / 'build-3.log').read_text(encoding='utf-8')
    shutil.copy2(ROOT / 'Saved/PlanConfiguration/launcher-tests.log', logs / 'launcher-tests.log')
    gather_results = []
    for suffix in ('', '-2', '-3'):
        source = ROOT / f'Saved/PlanConfiguration/localization{suffix}.log'
        text = source.read_text(encoding='utf-8-sig', errors='replace')
        lines = [line for line in text.splitlines() if 'GatherText completed with exit code' in line
                 or ('Error:' in line and ('GameFeatureData' in line or 'config' in line.lower()))]
        (logs / f'localization{suffix}-diagnostics.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        gather_results.append({'attempt': suffix or '-1', 'gather_completed': 'GatherText completed with exit code 0' in text})
    assert gather_results[-1]['gather_completed']
    for culture in ('en', 'zh-Hans'):
        data = (ROOT / f'Content/Localization/DroneOps/{culture}/DroneOps.locres').read_bytes()
        assert b'Configuration.MapRouteContext' in data and b'Configuration.DeployedReadOnly' in data
    protected = 'Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset'
    changed = subprocess.check_output(['git', 'diff', 'HEAD', '--name-only'], cwd=ROOT, text=True).splitlines()
    untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    paths = sorted(set(changed + untracked) - {protected})
    sources = {name: {'sha256_raw': digest(ROOT / name), 'git_blob': subprocess.check_output(
        ['git', 'hash-object', '--path=' + name, name], cwd=ROOT, text=True).strip()}
        for name in paths if not name.startswith(('Evidence/', 'Docs/')) and (ROOT / name).is_file()}
    patterns = [rb'eyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\.', rb'gh[pousr]_[A-Za-z0-9]{25,}',
                rb'github_pat_[A-Za-z0-9_]{20,}', rb'AIza[0-9A-Za-z_-]{30,}', rb'sk-(?:proj-)?[A-Za-z0-9_-]{30,}']
    files = sorted(path for path in OUT.rglob('*') if path.is_file())
    flagged = [str(path.relative_to(OUT)) for path in files if any(re.search(p, path.read_bytes()) for p in patterns)]
    assert not flagged, 'Potential credential files (values suppressed): ' + ', '.join(flagged)
    value = {'sealed_at_utc': datetime.now(timezone.utc).isoformat(),
             'baseline': 'ea80c1e9f2b2afc7b9726c16a826980de189aada',
             'scope': 'Isolated Mock data only. Native mouse workflows and RHI automation reported separately.',
             'runs': runs, 'latest_tests': [latest[name] for name in sorted(expected)],
             'failed_runs_retained': True, 'native': native,
             'localization': {'attempts': gather_results, 'final_internal_exit': 0,
                              'final_process_exit': int((ROOT / 'Saved/PlanConfiguration/localization-3-exit.txt').read_text(encoding='utf-8-sig'))},
             'backend_final_manifest': 'backend/revision-lease-release/manifest.json',
             'binaries': {name: digest(ROOT / name) for name in ('Backend/build/Release/DroneBackend.exe', 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll')},
             'sources': sources, 'credential_pattern_scan_flagged_files': flagged,
             'files': {path.relative_to(OUT).as_posix(): digest(path) for path in files}}
    target.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')
    print('SEALED', len(files), 'files;', sum(latest[name]['passed'] for name in expected), 'latest test cases passed')


if __name__ == '__main__':
    main()
