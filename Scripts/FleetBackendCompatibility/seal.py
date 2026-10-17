"""Seal this fix's stopped diagnostics and live Mock native acceptance separately."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import urllib.request
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from environment import load_config
from runtime import compatible_snapshot

out = root / 'Evidence/FleetBackendCompatibility'
manifest = out / 'manifest.json'
if manifest.exists():
    raise RuntimeError('Evidence already sealed; use a new directory')
config = load_config(root)
snapshot = compatible_snapshot(config['http_endpoint'])
assert snapshot['plans']['execution_adapter'] == 'Mock'
executions = snapshot['plans']['executions']
assert len(executions) == 4 and all(e['state'] == 'COMPLETED' for e in executions.values())
assert all(e['control_kind'] == 'GROUP_MOVE' and abs(e['position']['altitude'] - 40) < 1e-8
           for e in executions.values())
assert {c['client_role'] for c in snapshot['clients'] if c['state'] == 'ONLINE' and c['hydrated']} == {
    'Map', 'Command', 'Video'}
for folder in (root / 'Saved').glob('FleetBackendCompatibility-*'):
    ownership = folder / 'ownership.json'
    assert ownership.is_file() and json.loads(ownership.read_text()) == [], folder
before = json.loads((out / 'native/before-move.json').read_text())
after = json.loads((out / 'native/after-move.json').read_text())
assert before['video'] == after['video'] == snapshot['video']
logs = out / 'logs'
logs.mkdir(exist_ok=True)
for name in ('canonical-build.log', 'launcher-tests.log', 'backend-tests.log'):
    shutil.copy2(root / 'Saved/FleetBackendCompatibility' / name, logs / name)
unit = ET.parse(out / 'backend-tests.xml').getroot()
baseline = ET.parse(out / 'baseline-existing-failures.xml').getroot()
assert (int(unit.attrib['tests']), int(unit.attrib['failures'])) == (137, 2)
assert (int(baseline.attrib['tests']), int(baseline.attrib['failures'])) == (2, 2)
failed = lambda doc: sorted(c.attrib['classname'] + '.' + c.attrib['name']
                            for c in doc.iter('testcase') if c.find('failure') is not None)
assert failed(unit) == failed(baseline)
patterns = [rb'eyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\.', rb'gh[pousr]_[A-Za-z0-9]{25,}',
            rb'github_pat_[A-Za-z0-9_]{20,}']
files = sorted(p for p in out.rglob('*') if p.is_file())
flagged = [str(p.relative_to(out)) for p in files if any(re.search(x, p.read_bytes()) for x in patterns)]
assert not flagged, flagged
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
binaries = [root / config['backend_executable'], root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll',
            root / 'Backend/build/Release/DroneBackend_tests.exe',
            root / 'Backend/build-p54/Release/DroneBackend_tests.exe']
data = {
    'sealed_at_utc': datetime.now(timezone.utc).isoformat(),
    'baseline': '77a515932157785368fdf8b881d0e9b4cb12c4eb',
    'scope': 'Mock only; real native current Launcher Map movement; isolated HTTP/WS diagnostics',
    'isolated_processes_stopped': True,
    'user_launcher_kept_running': True,
    'current_clients': [{k: c[k] for k in ('client_role', 'state', 'hydrated')} for c in snapshot['clients']],
    'current_root': snapshot['backend'],
    'unit_tests': {'total': 137, 'passed': 135, 'existing_failures': failed(unit)},
    'credential_pattern_scan_flagged_files': flagged,
    'binaries': {str(p.relative_to(root)).replace('\\', '/'): digest(p) for p in binaries},
    'files': {str(p.relative_to(out)).replace('\\', '/'): digest(p) for p in files},
}
manifest.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
print('SEALED', len(files), 'files;', 'unit tests 135/137 with 2 existing failures')
