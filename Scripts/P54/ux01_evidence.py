"""Preserve raw UX-01 evidence locally; redact JWT values in review copies only."""
from pathlib import Path
import hashlib
import json
import re
import shutil

root = Path(__file__).resolve().parents[2]
base = root / 'Evidence/TASK-P5.4/ux-01'
raw = root / 'Saved/UX01RawEvidence'
pattern = re.compile(rb'eyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+')
manifest = base / 'redaction-manifest.json'
changes = json.loads(manifest.read_text()) if manifest.exists() else []
for file in sorted(base.rglob('*')):
    if not file.is_file() or file.suffix.lower() not in {'.log', '.json', '.jsonl', '.txt', '.xml', '.html'} or file == manifest:
        continue
    data = file.read_bytes()
    count = len(pattern.findall(data))
    if not count:
        continue
    rel = file.relative_to(base)
    backup = raw / rel
    if backup.exists():
        raise RuntimeError('Refusing to overwrite raw evidence: ' + str(backup))
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(file, backup)
    clean = pattern.sub(b'[REDACTED_JWT]', data)
    file.write_bytes(clean)
    changes.append(dict(path=rel.as_posix(), jwt_count=count,
                        original_sha256=hashlib.sha256(data).hexdigest(),
                        review_sha256=hashlib.sha256(clean).hexdigest(), raw_local=str(backup)))
manifest.write_text(json.dumps(changes, indent=2), encoding='utf-8')
print('UX-01 redaction records:', len(changes))
