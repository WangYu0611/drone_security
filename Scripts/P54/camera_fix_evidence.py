"""Preserve immutable raw logs and redact JWTs only in FIX-02 review evidence."""
from pathlib import Path
import hashlib,json,re,shutil
root=Path(__file__).resolve().parents[2]
base=root/'Evidence/TASK-P5.4'
folders=['camera-fix','ue-regression-1790421811387014600','regression-1790422334717021900','p53-safe-1790422444689247600']
raw=root/'Saved/CameraFixRawEvidence'
pattern=re.compile(rb'eyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+')
manifest=base/'camera-fix/redaction-manifest.json'
changes=json.loads(manifest.read_text()) if manifest.exists() else []
for name in folders:
 for file in (base/name).rglob('*'):
  if not file.is_file() or file.suffix.lower() not in {'.log','.json','.jsonl','.txt','.xml','.html'} or file==manifest:continue
  data=file.read_bytes();count=len(pattern.findall(data))
  if not count:continue
  rel=file.relative_to(base);backup=raw/rel
  if backup.exists():raise RuntimeError('Refusing to overwrite raw evidence: '+str(backup))
  backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,backup)
  clean=pattern.sub(b'[REDACTED_JWT]',data);file.write_bytes(clean)
  changes.append(dict(path=rel.as_posix(),jwt_count=count,original_sha256=hashlib.sha256(data).hexdigest(),review_sha256=hashlib.sha256(clean).hexdigest(),raw_local=str(backup)))
manifest.write_text(json.dumps(changes,indent=2))
print('FIX-02 redaction records:',len(changes))
