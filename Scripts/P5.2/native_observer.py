"""Read-only native acceptance oracle. Never sends a command or mutates backend."""
import json,time,urllib.request
from pathlib import Path
out=Path(__file__).resolve().parents[2]/'Evidence/TASK-P5.2/native'
with (out/'observations.jsonl').open('a',encoding='utf-8') as f:
    for _ in range(7200):
        try:
            with urllib.request.urlopen('http://127.0.0.1:19780/api/security-plans',timeout=2) as r: state=json.load(r)
            row={'observed_at':time.time(),'executions':state.get('executions',[]),'events':state.get('events',[])}
            f.write(json.dumps(row,separators=(',',':'))+'\n');f.flush()
        except OSError: pass
        time.sleep(1)
