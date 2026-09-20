"""Copy diagnostic logs with credential redaction; original Saved logs remain intact."""
import hashlib
import json
from pathlib import Path
import re

root=Path(__file__).resolve().parents[2]
dest=root/'Evidence/TASK-P5.1/logs'
dest.mkdir(parents=True,exist_ok=True)
sources=list((root/'Saved/Stage1/Logs').glob('*.log'))+list((root/'Saved/P5.1').glob('*.log'))
sources+=list((root/'Saved/Stage1/Logs').glob('*.jsonl'))
manifest=[]
for path in sources:
    raw=path.read_bytes()
    text=raw.decode('utf-8',errors='replace')
    text=re.sub(r'(?i)((?:access_token|api_key|apikey|token|password|authorization)\s*[=:]\s*["\x27]?)[^\s"\x27&<>]+',r'\1[REDACTED]',text)
    text=re.sub(r'eyJ[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+','[REDACTED_JWT]',text)
    text=re.sub(r'(https?://)[^\s/@]+:[^\s/@]+@',r'\1[REDACTED]@',text)
    text='\n'.join(line.rstrip() for line in text.splitlines()).rstrip()+'\n' if text else ''
    name=('stage1-' if 'Stage1' in path.parts else 'diagnostic-')+path.name
    (dest/name).write_text(text,encoding='utf-8',newline='\n')
    manifest.append({'source':str(path.relative_to(root)),'copy':name,'source_sha256':hashlib.sha256(raw).hexdigest(),'normalized_or_redacted':text!=raw.decode('utf-8',errors='replace')})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(f'Copied {len(manifest)} logs; originals retained in Saved')
