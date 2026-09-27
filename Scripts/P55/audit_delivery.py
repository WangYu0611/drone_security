"""Snapshot reviewed sources/assets and scan new evidence without changing original logs."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess
root=Path(__file__).resolve().parents[2];out=root/'Evidence/TASK-P5.5'
base='414094076014dc076205195f33e656393cd1ee03'
def git(*args):return subprocess.check_output(['git',*args],cwd=root).decode('utf-8').strip()
assert not git('diff',base,'--','Backend'), 'P5.5 must not change Backend'
assert not git('diff',base,'--','Content/Command/Materials/M_CommandPath.uasset'), 'Old material changed'
native=out/'native-runtime';native.mkdir(exist_ok=True)
for file in (root/'Saved/P55-QA/Logs').glob('*'):
    if file.is_file():shutil.copy2(file,native/file.name)
shutil.copy2(root/'Saved/P55-QA/ownership.json',native/'ownership-after-stop.json')
pattern=re.compile(rb'eyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+')
redactions=[]
for file in out.rglob('*'):
    if not file.is_file() or file.suffix.lower() not in {'.log','.json','.txt','.html','.xml'}:continue
    data=file.read_bytes();count=len(pattern.findall(data))
    if count:
        raw=root/'Saved/P55RawEvidence'/file.relative_to(out)
        assert not raw.exists();raw.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,raw)
        clean=pattern.sub(b'[REDACTED_JWT]',data);file.write_bytes(clean)
        redactions.append(dict(path=str(file.relative_to(out)),count=count,raw_sha256=hashlib.sha256(data).hexdigest()))
paths=set(git('diff','--name-only',base).splitlines())|set(git('ls-files','--others','--exclude-standard').splitlines())
source=[]
for rel in sorted(paths):
    file=root/rel
    if not file.is_file() or rel.startswith('Evidence/'):continue
    data=file.read_bytes()
    if file.suffix in {'.uasset','.umap'}:
        assert data[:4]==bytes.fromhex('c1832a9e'), 'Expected real UE package: '+rel
        assert not pattern.search(data), 'Credential pattern in asset: '+rel
    source.append(dict(path=rel,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
(out/'delivery-manifest.json').write_text(json.dumps(dict(base=base,implementation_head=git('rev-parse','HEAD'),backend_unchanged=True,old_material_unchanged=True,jwt_redactions=redactions,files=source),indent=2),encoding='utf-8')
(out/'changed-files.txt').write_text('\n'.join(x['path'] for x in source)+'\n',encoding='utf-8')
print(json.dumps(dict(source_files=len(source),jwt_redactions=len(redactions),backend_unchanged=True,old_material_unchanged=True)))
