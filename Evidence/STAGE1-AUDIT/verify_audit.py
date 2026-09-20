"""Validate generated documentation and preserved sources without runtime activity."""
from pathlib import Path
import collections, hashlib, json, re, subprocess

R=Path(__file__).resolve().parents[2]
D=R/'Docs/Stage1'
E=R/'Evidence/STAGE1-AUDIT'
def git(*args):return subprocess.check_output(['git','-C',str(R),*args]).decode('utf8',errors='replace').strip()
def text(p):return p.read_text(encoding='utf-8-sig')
errors=[]
required=['Stage1-Feature-Audit.md','Stage1-Data-Interface-Catalog.md','Stage1-Architecture-and-Dataflow.md','Stage1-Real-UAV-Interface-Audit.md','Stage1-Presentation-Summary.md','Stage1-Demo-Runbook.md','Stage1-Schema-Fields.md']
for f in required:
    if not (D/f).is_file():errors.append('missing '+f)
docs=list(D.glob('*.md'))+[E/'README.md']
for p in docs:
    s=text(p)
    if s.count('```')%2:errors.append('unbalanced fences '+p.name)
    for target in re.findall(r'\]\(([^)]+)\)',s):
        if not target.startswith(('http:','https:','#')) and not (p.parent/target.split('#')[0]).exists():errors.append('broken link '+p.name+' '+target)
    for block in re.findall(r'```json\s*\n(.*?)\n```',s,re.S):
        try:json.loads(block)
        except Exception as ex:errors.append('invalid JSON example '+p.name+' '+str(ex))
    if re.search(r'\b192\.168\.\d+\.\d+\b',s):errors.append('unredacted private device IP '+p.name)
catalog=text(D/'Stage1-Data-Interface-Catalog.md')
listed={(m,p) for m,p in re.findall(r'^\| (GET|POST|PUT|PATCH|DELETE) `([^`]+)`',catalog,re.M)}
routes=json.loads(text(E/'http-routes.json'))
scanned={(x['method'],x['path']) for x in routes}
if listed!=scanned:errors.append('HTTP table mismatch '+str(listed^scanned))
feature=text(D/'Stage1-Feature-Audit.md')
rows=re.findall(r'^\| F\d+.*$',feature,re.M)
counts=collections.Counter()
for row in rows:
    m=re.search(r'\| (ACTIVE_VERIFIED|ACTIVE_PARTIAL|AVAILABLE_NOT_USED|LEGACY_PRESERVED|HISTORICAL_ONLY|HISTORICAL_VERIFIED|STAGE2_RESERVED|UNKNOWN)(?: /| \|)',row)
    if not m:errors.append('feature lacks status '+row[:40])
    else:counts[m[1]]+=1
if len(rows)!=62:errors.append('feature count mismatch')
for p,h in json.loads(text(E/'source-sha256.json')).items():
    if hashlib.sha256((R/p).read_bytes()).hexdigest()!=h:errors.append('source changed '+p)
if git('diff','--name-only') or git('diff','--cached','--name-only'):errors.append('tracked changes present')
if git('rev-parse','HEAD')!='e9ce39402f3bc6222e9d7b2f3c93ef65b5870941':errors.append('HEAD changed')
untracked=git('ls-files','--others','--exclude-standard').splitlines()
for p in untracked:
    if not p.startswith(('Docs/Stage1/','Evidence/STAGE1-AUDIT/')):errors.append('unexpected output '+p)
for p in E.glob('*.json'):
    try:json.loads(text(p))
    except Exception as ex:errors.append('invalid evidence JSON '+p.name)
summary={'audit_result':'CONDITIONAL PASS','document_validation':'PASS' if not errors else 'FAIL','errors':errors,'required_documents':len(required),'http_routes':len(scanned),'http_catalog_exact_match':listed==scanned,'feature_groups':len(rows),'feature_classification':dict(counts),'source_hashes_verified':len(json.loads(text(E/'source-sha256.json'))),'tracked_changes':git('diff','--name-only'),'staged_changes':git('diff','--cached','--name-only'),'head':git('rev-parse','HEAD'),'new_runtime_tests':0,'hardware_connections':0,'saved_native_visuals_reviewed':['Evidence/TASK-P5.2/native/12-completed.jpg','Evidence/TASK-P5.2/native/06-executing-map.jpg'],'limits':['Blueprint graphs not exported','External PX4/ROS/media runtime not revalidated','Existing historical test failures retained','No native rerun this audit']}
(E/'validation.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps(summary,ensure_ascii=False))
raise SystemExit(bool(errors))
