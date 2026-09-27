import sys,json,time
from pathlib import Path
root=Path.cwd();sys.path.insert(0,str(root/'Launcher'))
from runtime import Runtime
cfg=json.loads((root/'Launcher/config.json').read_text());cfg.update(json.loads((root/'Launcher/config.local.json').read_text()));cfg.update(runtime_directory='Saved/CameraFixQA',http_endpoint='http://127.0.0.1:20380',ws_endpoint='ws://127.0.0.1:20381/ws')
r=Runtime(root,cfg);last=''
try:
 r._backend();r.start_role('Command');r.start_role('Map');print('READY',flush=True)
 while True:
  p=root/'Saved/CameraFixQA/command.json'
  if p.exists():
   s=p.read_text()
   if s!=last:
    last=s;q=json.loads(s)
    if q['action']=='stop':break
    if q['action']=='stop_role':r.stop_role(q['role'])
    if q['action']=='start':r.start_role(q['role'])
    print('DONE '+s,flush=True)
  time.sleep(.5)
finally:r.stop()


