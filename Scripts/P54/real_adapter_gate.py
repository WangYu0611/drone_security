"""Real-mode routing gates with an EMPTY fleet: no aircraft, UDP mapping or commands."""
import json,subprocess,time,socket,urllib.request,urllib.error
from pathlib import Path
root=Path(__file__).resolve().parents[2];run=str(time.time_ns());work=root/'Saved'/('P54-real-gate-'+run);out=root/'Evidence/TASK-P5.4'/('real-adapter-gate-'+run)
(work/'data').mkdir(parents=True);(work/'Logs').mkdir();out.mkdir(parents=True)
for port in (20380,20381):
 with socket.socket() as sock:
  if sock.connect_ex(('127.0.0.1',port))==0:raise RuntimeError('Test port occupied; preserving existing service')
(work/'data/drones.json').write_text('[]')
(work/'data/real_execution.json').write_text(json.dumps({'enabled':True,'default_speed_mps':5,'uavs':{}}))
(work/'backend.yaml').write_text('server:\n  http_port: 20380\n  ws_port: 20381\n  debug: true\nport_map: {}\nstorage:\n  path: "data/drones.json"\nlog:\n  level: "info"\n  file: "Logs/backend.log"\n')
def api(path,body=None):
 req=urllib.request.Request('http://127.0.0.1:20380'+path,data=None if body is None else json.dumps(body).encode(),headers={'Content-Type':'application/json'})
 try:
  with urllib.request.urlopen(req,timeout=3) as r:return r.status,json.load(r)
 except urllib.error.HTTPError as r:return r.code,json.load(r)
results=[]
with (out/'backend.log').open('w') as log:
 process=subprocess.Popen([str(root/'Backend/build-p54/Release/DroneBackend.exe'),str(work/'backend.yaml')],cwd=work,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
 try:
  for attempt in range(80):
   try:status,state=api('/api/security-plans');break
   except OSError:
    if process.poll() is not None:raise RuntimeError('Backend exited')
    time.sleep(.25)
  else:raise RuntimeError('Backend readiness timeout')
  assert status==200 and state['execution_adapter']=='Real' and state['executions']=={};results.append('Real adapter hydration; empty fleet; zero executions')
  for path,body in [('/api/arrays',{}),('/api/debug/cmd/d1/move',{}),('/api/mock-executions/pause-all',{'simulation':True,'request_id':'reject-mock'})]:
   status,reply=api(path,body);assert status==409,(path,status,reply);results.append({'path':path,'status':status,'code':reply['code']})
  results.append({'state_after':api('/api/security-plans')[1]['executions'],'flight_io':'No UAV records and empty port_map; no transport activated'})
 except Exception as error:
  (out/'result.json').write_text(json.dumps({'pass':False,'results':results,'failure':str(error)},indent=2));raise
 finally:
  if process.poll() is None:process.terminate()
  process.wait(timeout=15)
(out/'result.json').write_text(json.dumps({'pass':True,'pid':process.pid,'results':results},indent=2));print(out)
