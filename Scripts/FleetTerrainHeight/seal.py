"""Seal terrain acceptance; preserve failed runs and separate native/probe evidence."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from environment import load_config
from runtime import compatible_snapshot

out = root / 'Evidence/FleetTerrainHeight'
manifest = out / 'manifest.json'
if manifest.exists():
    raise RuntimeError('Evidence already sealed; use a new directory')
config = load_config(root)
snapshot = compatible_snapshot(config['http_endpoint'])
assert snapshot['plans']['execution_adapter'] == 'Mock'
online = [c for c in snapshot['clients'] if c['state'] == 'ONLINE' and c['hydrated']]
assert {c['client_role'] for c in online} == {'Command', 'Map', 'Video'}
assert len(online) == 3
assert all(e['state'] in ('COMPLETED', 'ABORTED', 'FAILED', 'CANCELLED')
           for e in snapshot['plans']['executions'].values())
before = json.loads((out / 'native/user-before.json').read_text())
after = json.loads((out / 'native/user-after.json').read_text())
assert before['count'] == 8 and after['count'] == len(snapshot['plans']['executions']) == 12
assert before['video'] == after['video'] == snapshot['video']
assert len(after['latest_executions']) == 4 and all(e['state'] == 'COMPLETED' for e in after['latest_executions'])
for folder in (root / 'Saved').glob('FleetTerrainHeight-*'):
    ownership = folder / 'ownership.json'
    if ownership.exists():
        assert json.loads(ownership.read_text()) == [], folder

latest = {}
for folder in sorted(out.glob('verification-*')):
    for result in json.loads((folder / 'summary.json').read_text()):
        latest[result['test']] = (folder, result)
tests = []
for name, (folder, result) in latest.items():
    assert result['pass'], (name, result)
    index = json.loads((folder / name / 'index.json').read_text(encoding='utf-8-sig'))
    tests.append(dict(result, warnings=index.get('succeededWithWarnings', 0),
                      report=str(folder.relative_to(out)).replace('\\', '/') + '/' + name))
assert sum(t['discovered'] for t in tests) == sum(t['completed'] for t in tests) == sum(t['passed'] for t in tests) == 8
assert sum(t['warnings'] for t in tests) == 6

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

dll = root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll'
final_run = max(out.glob('verification-*'))
assert json.loads((final_run / 'binary.json').read_text())['sha256'] == digest(dll)
recovered = json.loads((out / 'native/recovered-probe.json').read_text())
move = json.loads((out / 'native/agl-40m-completed-probe.json').read_text())
camera = json.loads((out / 'native/camera-near-surface-probe.json').read_text())
assert all(p['surface_loaded'] and p['agl'] >= 29.99 and p['model_error_m'] < .001 for p in recovered['uavs'])
assert all(p['surface_loaded'] and p['agl'] > 39 and p['model_error_m'] < .001 for p in move['uavs'])
assert abs(move['uavs'][0]['agl'] - 40) < .001 and camera['camera_agl'] >= 4.99

logs = out / 'logs'
logs.mkdir(exist_ok=True)
for i in range(1, 6):
    shutil.copy2(root / f'Saved/FleetTerrainHeight/build-{i}.log', logs / f'build-{i}.log')
# Raw commandlet logs remain in Saved; retain the relevant original diagnostics
# without publishing unrelated account/session or network data.
gather = (root / 'Saved/FleetTerrainHeight/localization-2.log').read_text(encoding='utf-8-sig', errors='replace')
gather_lines = [line for line in gather.splitlines()
                if 'GatherText completed with exit code' in line or ('GameFeatureData' in line and 'Error:' in line)]
(logs / 'localization-diagnostics.txt').write_text('\n'.join(gather_lines) + '\n', encoding='utf-8')
assert 'GatherText completed with exit code 0' in gather
for lang in ('en', 'zh-Hans'):
    assert (root / f'Content/Localization/DroneOps/{lang}/DroneOps.locres').read_bytes().count(b'FleetTerrain.') == 12

patterns = [rb'eyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\.', rb'gh[pousr]_[A-Za-z0-9]{25,}',
            rb'github_pat_[A-Za-z0-9_]{20,}', rb'AIza[0-9A-Za-z_-]{30,}', rb'sk-(?:proj-)?[A-Za-z0-9_-]{30,}']
files = sorted(p for p in out.rglob('*') if p.is_file())
flagged = [str(p.relative_to(out)) for p in files if any(re.search(pattern, p.read_bytes()) for pattern in patterns)]
assert not flagged, 'Review potential credential files (values suppressed): ' + ', '.join(flagged)
source_paths = ['Source/UE5DroneControl/Map/MapSurfaceHeight.h', 'Source/UE5DroneControl/Map/MapSurfaceHeight.cpp',
    'Source/UE5DroneControl/Command/CommandMapInteractionService.cpp',
    'Source/UE5DroneControl/Map/MapFleetInteractionWidget.h', 'Source/UE5DroneControl/Map/MapFleetInteractionWidget.cpp',
    'Source/UE5DroneControl/Tests/FleetTerrainTest.cpp', 'Source/UE5DroneControl/Tests/FleetControlTest.cpp',
    'Source/UE5DroneControl/Shared/ProductText.cpp', 'Scripts/P4/localization.json']
sources = {p: {'sha256_raw': digest(root / p),
               'git_blob': subprocess.check_output(['git', 'hash-object', '--path=' + p, p], cwd=root, text=True).strip()}
           for p in source_paths}
data = {
    'sealed_at_utc': datetime.now(timezone.utc).isoformat(),
    'baseline': '237ccfbddef9231b7aed967cd94af8a2ef3e010b',
    'scope': 'online CesiumWorld, Mock only; native mouse/keyboard separated from numerical probes and automation',
    'final_tests': tests, 'unique_discovered': 8, 'unique_completed': 8, 'unique_passed': 8,
    'tests_with_existing_environment_warnings': 6,
    'failed_initial_runs_retained': True,
    'localization_internal_exit': 0, 'localization_process_exit': 1,
    'isolated_processes_stopped': True, 'user_normal_launcher_kept_running': True,
    'current_clients': [{k: c[k] for k in ('client_role', 'state', 'hydrated')} for c in online],
    'credential_pattern_scan_flagged_files': flagged,
    'binaries': {'Binaries/Win64/UnrealEditor-UE5DroneControl.dll': digest(dll),
                 config['backend_executable']: digest(root / config['backend_executable'])},
    'sources': sources,
    'files': {str(p.relative_to(out)).replace('\\', '/'): digest(p) for p in files},
}
manifest.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
print('SEALED', len(files), 'files; 8 unique tests passed; 6 with existing warnings; user runtime ready')
