"""Seal completed, task-owned Mock QA evidence without changing runtime data."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
from datetime import datetime, timezone

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from environment import load_config
out = root / 'Evidence/FleetControl3D'
runtime = root / 'Saved/FleetControl3D-QA'
manifest = out / 'manifest.json'
if manifest.exists():
    raise RuntimeError('Evidence already sealed; use a new evidence directory for further runs')
if json.loads((runtime / 'ownership.json').read_text(encoding='utf-8-sig')):
    raise RuntimeError('Stop task-owned processes before sealing logs')
logs = out / 'runtime-logs'
logs.mkdir(exist_ok=True)
for source in (runtime / 'Logs').iterdir():
    if source.is_file():
        shutil.copy2(source, logs / source.name)

patterns = [
    rb'eyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\.',
    rb'gh[pousr]_[A-Za-z0-9]{25,}',
    rb'github_pat_[A-Za-z0-9_]{20,}',
    rb'-----BEGIN .*PRIVATE KEY-----',
]
files = sorted(p for p in out.rglob('*') if p.is_file())
flagged = [str(p.relative_to(out)) for p in files
           if any(re.search(pattern, p.read_bytes()) for pattern in patterns)]
if flagged:
    raise RuntimeError('Review possible credentials before publication: ' + ', '.join(flagged))

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

binaries = [root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll',
            root / load_config(root)['backend_executable']]
data = {
    'sealed_at_utc': datetime.now(timezone.utc).isoformat(),
    'baseline': '1e31715ceb7d3f35f1819c9fe6dfdc503d15c2cf',
    'scope': 'offline Mock; native mouse/keyboard and automation evidence separated',
    'native_processes_stopped': True,
    'credential_pattern_scan_flagged_files': flagged,
    'binaries': {str(p.relative_to(root)).replace('\\', '/'): digest(p) for p in binaries},
    'files': {str(p.relative_to(out)).replace('\\', '/'): digest(p) for p in files},
}
manifest.write_text(json.dumps(data, indent=2), encoding='utf-8')
print(json.dumps({'sealed_files': len(files), 'manifest': str(manifest)}))
