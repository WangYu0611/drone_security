"""Preserve raw task evidence locally; redact JWTs in the versioned review copy."""
from pathlib import Path
import hashlib,json,re,shutil
root=Path(__file__).resolve().parents[2];evidence=root/'Evidence/TASK-P5.4';raw=root/'Saved/P54-raw-evidence'
pattern=re.compile(rb'eyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+')
changes=[]
for file in evidence.rglob('*'):
 if not file.is_file() or file.suffix.lower() not in {'.log','.json','.txt','.xml','.html'} or file.name=='redaction-manifest.json':continue
 data=file.read_bytes();count=len(pattern.findall(data))
 if not count:continue
 rel=file.relative_to(evidence);backup=raw/rel;backup.parent.mkdir(parents=True,exist_ok=True)
 if backup.exists():raise RuntimeError('Refusing to overwrite original evidence '+str(backup))
 shutil.copy2(file,backup);clean=pattern.sub(b'[REDACTED_JWT]',data);file.write_bytes(clean)
 changes.append(dict(path=rel.as_posix(),jwt_count=count,original_sha256=hashlib.sha256(data).hexdigest(),review_sha256=hashlib.sha256(clean).hexdigest(),raw_local=str(backup)))
manifest=evidence/'redaction-manifest.json'
if manifest.exists():changes=json.loads(manifest.read_text())+changes
manifest.write_text(json.dumps(changes,indent=2));print('Redacted evidence files:',len(changes))
