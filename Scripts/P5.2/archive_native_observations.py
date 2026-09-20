"""Losslessly archive the stopped read-only observer, retaining the local source."""
import gzip,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
source=root/'Evidence/TASK-P5.2/native/observations.jsonl'
archive=source.with_suffix('.jsonl.gz')
retained=root/'Saved/P5.2-native-raw/observations.jsonl'
assert source.resolve().is_relative_to(root) and retained.resolve().is_relative_to(root)
assert not retained.exists(),'Raw evidence already archived'
raw=source.read_bytes();packed=gzip.compress(raw,compresslevel=9,mtime=0)
assert gzip.decompress(packed)==raw
archive.write_bytes(packed)
retained.parent.mkdir(parents=True,exist_ok=True);source.rename(retained)
proof=dict(uncompressed_bytes=len(raw),archive_bytes=len(packed),sha256=hashlib.sha256(raw).hexdigest(),lossless_verified=True,retained_original=str(retained))
(root/'Evidence/TASK-P5.2/closure/observation-archive.json').write_text(json.dumps(proof,indent=2),encoding='utf-8')
print(json.dumps(proof))
