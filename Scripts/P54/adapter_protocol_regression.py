"""Fresh four-aircraft regression and actual two-minute schedule/restart."""
import sys,json,subprocess,os,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'Launcher'));from runtime import Runtime
run=str(time.time_ns());out=root/'Evidence/TASK-P5.4'/('adapter-protocol-'+run);out.mkdir(parents=True)
cfg=json.loads((root/'Launcher/config.json').read_text());cfg.update(backend_executable='Backend/build-p54/Release/DroneBackend.exe',http_endpoint='http://127.0.0.1:20480',ws_endpoint='ws://127.0.0.1:20481/ws',runtime_directory='Saved/P54-adapter-protocol-'+run)
runtime=Runtime(root,cfg);env=os.environ.copy();env.update(P1_HTTP=cfg['http_endpoint'],P1_WS=cfg['ws_endpoint'],P54_EVIDENCE_DIR=str(out))
try:
 runtime._backend();first=runtime.owned['Backend'].process.pid
 with (out/'protocol.log').open('w',encoding='utf-8') as log:subprocess.run(['node',str(root/'Backend/tests/p54_mission_integration.mjs')],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True)
 runtime.stop_role('Backend');runtime._backend();second=runtime.owned['Backend'].process.pid
 assert first!=second
 (out/'restart.json').write_text(json.dumps({'before_pid':first,'after_pid':second,'graceful':True,'same_durable_store':str(runtime.directory)},indent=2))
 with (out/'resume.log').open('w',encoding='utf-8') as log:subprocess.run(['node',str(root/'Backend/tests/p54_mission_integration.mjs'),'--resume'],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True)
 print(str(out),flush=True)
finally:runtime.stop()
