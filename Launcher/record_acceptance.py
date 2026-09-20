"""Read-only oracle for native lifecycle checkpoints (does not drive clients)."""
import argparse
import json
from pathlib import Path
from runtime import compatible_snapshot

p = argparse.ArgumentParser()
p.add_argument('name')
p.add_argument('--compare')
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
config = json.loads((root/'Launcher/config.json').read_text())
out = root/'Evidence/TASK-P5/recovery'
out.mkdir(parents=True, exist_ok=True)
snapshot = compatible_snapshot(config['http_endpoint'])
snapshot['ownership'] = json.loads((root/config['runtime_directory']/'ownership.json').read_text())
if args.compare:
    before = json.loads((out/(args.compare+'.json')).read_text(encoding='utf-8-sig'))
    for canonical, endpoint in [('preferences','ui-preferences'),('video','video-view'),('plans','security-plans')]:
        if canonical not in before: before[canonical] = before[endpoint]
    snapshot['comparison'] = {}
    for domain in ('context', 'preferences', 'video', 'plans'):
        same = before[domain] == snapshot[domain]
        snapshot['comparison'][domain] = same
        if not same:
            raise AssertionError(f'{domain} changed during recovery')
    print('PASS authoritative context, preferences, video and plans unchanged')
(out/(args.name+'.json')).write_text(json.dumps(snapshot, indent=2), encoding='utf-8')
print([(c['client_role'], c['state'], c.get('hydrated')) for c in snapshot['clients']])
