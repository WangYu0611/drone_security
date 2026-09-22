"""Own the native QA processes; record Backend evidence without driving UI."""
import json
from pathlib import Path
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Launcher'))
from runtime import Runtime, compatible_snapshot

out=ROOT/'Evidence/TASK-P5.3/native';out.mkdir(parents=True,exist_ok=True)
config=json.loads((ROOT/'Launcher/config.json').read_text(encoding='utf-8'))
config['runtime_directory']='Saved/P5.3-native'
runtime=Runtime(ROOT,config)
control=ROOT/'Saved/P5.3-native/control.txt'
previous=''
runtime.start()
try:
    while True:
        runtime.poll()
        if control.exists():
            command=control.read_text().strip();control.unlink()
            if command=='stop':break
            if command.startswith('restart '):runtime.restart(command.split(' ',1)[1])
        current=compatible_snapshot(config['http_endpoint'])
        text=json.dumps(current,ensure_ascii=False,sort_keys=True)
        # Presence timestamps change frequently: preserve business snapshots only.
        business=json.dumps({k:current[k] for k in ('plans','context','preferences','video')},ensure_ascii=False,sort_keys=True)
        if business!=previous:
            (out/f'state-{time.time_ns()}.json').write_text(text,encoding='utf-8');previous=business
        (out/'latest.json').write_text(text,encoding='utf-8')
        time.sleep(.5)
finally:
    runtime.stop()
