"""Repeat unchanged P53 suite with idle aircraft outside its ingress corridor."""
import sys,json,subprocess,os,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'Launcher'));from runtime import Runtime
run=str(time.time_ns());out=root/'Evidence/TASK-P5.4'/('p53-safe-'+run);out.mkdir(parents=True)
fixture=json.loads((root/'Backend/mock_execution.json').read_text())
for uid,u in fixture['uavs'].items():
 if uid!='UAV-03':u['home_position']['latitude']-=.01
(out/'mock_fixture.json').write_text(json.dumps(fixture,indent=2))
cfg=json.loads((root/'Launcher/config.json').read_text());cfg.update(backend_executable='Backend/build-p54/Release/DroneBackend.exe',http_endpoint='http://127.0.0.1:20280',ws_endpoint='ws://127.0.0.1:20281/ws',runtime_directory='Saved/P54-p53-safe-'+run,mock_fixture=str(out/'mock_fixture.json'))
r=Runtime(root,cfg)
try:
 r._backend();env=os.environ.copy();env.update(P1_HTTP=cfg['http_endpoint'],P1_WS=cfg['ws_endpoint'],P53_EVIDENCE_DIR=str(out/'protocol'))
 with (out/'p53.log').open('w',encoding='utf-8') as log:result=subprocess.run(['node',str(root/'Backend/tests/p53_geometry_integration.mjs')],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 print(str(out),result.returncode,flush=True)
finally:r.stop()

sys.exit(result.returncode)
