"""Validate recorded test/recovery evidence and hash untouched native captures."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]
evidence = root / 'Evidence/TASK-P5.1'
automation = evidence / 'automation'
load = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
summary = {'ue': [], 'protocol': {}}
for role, expected in [('command', 2), ('map', 2), ('video', 1)]:
    report = load(automation / f'ue-{role}/index.json')
    log = (root / f'Saved/P5.1/final-ue-{role}.log').read_text(encoding='utf-8-sig')
    completed = re.findall(r'Test Completed\. Result=\{Success\}', log)
    assert len(completed) == expected
    assert report['succeeded'] + report['succeededWithWarnings'] == expected
    assert report['failed'] == 0 and report['notRun'] == 0 and report['inProcess'] == 0
    summary['ue'].append({'role': role, 'completed_successfully': expected,
                          'device': report['devices'][0]['instanceName'],
                          'report_created': report['reportCreatedOn'], 'failed': 0})
for label, count in [('p4', 21), ('p5', 8), ('p51', 11)]:
    text = (automation / f'{label}-protocol-final.log').read_text(encoding='utf-8-sig')
    assert len(re.findall(r'^PASS ', text, re.M)) == count
    summary['protocol'][label] = count
backend = load(automation / 'backend-tests.json')
assert backend['tests'] == 64 and backend['failures'] == 2
failed = [f"{s['name']}.{t['name']}" for s in backend['testsuites'] for t in s['testsuite'] if t.get('failures')]
assert sorted(failed) == sorted(['SegmentDistanceTest.Crossing', 'AssemblyPlannerTest.ConflictHeightSeparation'])
summary['backend'] = {'discovered': 64, 'passed': 62, 'historical_failures': failed}
for name in ['backend-rehydrated', 'after-full-relaunch']:
    snap = load(evidence / f'recovery/{name}.json')
    assert all(snap['comparison'].values())
    assert all(any(c['client_role'] == role and c['state'] == 'ONLINE' and c.get('hydrated')
                   for c in snap['clients']) for role in ['Command', 'Map', 'Video'])
before = load(evidence / 'recovery/before-map-restart.json')
after = load(evidence / 'recovery/after-map-restart.json')
assert before['context'] == after['context']
for domain in ['plans', 'missions', 'paths', 'deployments']:
    assert before['plans'][domain] == after['plans'][domain], domain
summary['native_recovery'] = 'saved Map content equal; Backend/full relaunch domains equal and all roles hydrated'
for name in ['shutdown-owned-processes', 'final-shutdown-owned-processes']:
    rows = load(evidence / f'recovery/{name}.json')
    assert len(rows) == 4 and not any(r['live'] for r in rows)
before_refs = (evidence / 'closure/protected-refs-before.txt').read_text(encoding='utf-8-sig')
after_refs = (evidence / 'closure/protected-refs-after.txt').read_text(encoding='utf-8-sig')
def protected(text):
    return sorted(x for x in text.splitlines() if 'refs/heads/feat/triple-screen-p5-1-command-plan-ux' not in x)
assert protected(before_refs) == protected(after_refs)
summary['protected_refs_unchanged'] = True
(automation / 'verified-summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
manifest = [{'file': f.name, 'sha256': hashlib.sha256(f.read_bytes()).hexdigest(), 'bytes': f.stat().st_size}
            for f in sorted((evidence / 'visual').glob('*.jpg'))]
(evidence / 'visual/manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))
