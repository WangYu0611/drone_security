"""Summarize verified exports and protected refs; does not mutate runtime state."""
import json,subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[2]
out=root/'Evidence/TASK-P5.2'
counts=[]
for role,folders,expected in [
    ('Command',['Command'],3),
    ('Map',['Map-isolated/MapLiveDraftGuard','Map-isolated/FTextAndDraftIsolation','Map-isolated/MapExecutionMonitor'],3),
    ('Video',['Video'],1)]:
    item=dict(role=role,succeeded=0,warnings=0,failed=0,completed=0,reports=folders)
    for folder in folders:
        report=out/'ue'/folder
        data=json.loads((report/'index.json').read_text(encoding='utf-8-sig'))
        item['succeeded']+=data['succeeded'];item['warnings']+=data.get('succeededWithWarnings',0)
        item['failed']+=data['failed']
        item['completed']+=(report/'run.log').read_text(encoding='utf-8-sig').count('Test Completed. Result={Success}')
    assert item['failed']==0 and item['succeeded']+item['warnings']==expected and item['completed']==expected,item
    counts.append(item)
(out/'ue/final-counts.json').write_text(json.dumps(counts,indent=2),encoding='utf-8')
baseline=(out/'baseline/refs.txt').read_text(encoding='utf-8-sig').splitlines()
current=subprocess.check_output(['git','show-ref','--heads'],cwd=root,text=True).splitlines()
protected=lambda refs:sorted(r for r in refs if not r.endswith('refs/heads/feat/triple-screen-p5-2-mission-execution'))
assert protected(baseline)==protected(current),'Protected ref changed'
assert subprocess.check_output(['git','-C',str(root.parent/'UE5DroneControl-p5-1'),'status','--porcelain'],text=True)==''
print(json.dumps({'ue':counts,'protected_refs_unchanged':True,'p51_clean':True},indent=2))
