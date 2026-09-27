"""Run inherited HTTP/WS suites against fresh, isolated synthetic Backend stores."""
import sys,json,subprocess,os,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'Launcher'))
from runtime import Runtime
run=str(time.time_ns());out=root/'Evidence/TASK-P5.4'/('regression-'+run);out.mkdir(parents=True)
results=[]
for name,script,variable in [('p4','p4_workflow_integration.mjs','P4_EVIDENCE_DIR'),('p5','p5_presence_integration.mjs','P5_EVIDENCE_DIR'),('p51','p51_workflow_integration.mjs','P51_EVIDENCE_DIR'),('p52','p52_execution_integration.mjs','P52_EVIDENCE_DIR'),('p53','p53_geometry_integration.mjs','P53_EVIDENCE_DIR')]:
    config=json.loads((root/'Launcher/config.json').read_text());config.update(backend_executable='Backend/build-p54/Release/DroneBackend.exe',http_endpoint='http://127.0.0.1:20080',ws_endpoint='ws://127.0.0.1:20081/ws',runtime_directory=f'Saved/P54-regression-{run}/{name}')
    runtime=Runtime(root,config)
    try:
        runtime._backend();env=os.environ.copy();env.update(P1_HTTP=config['http_endpoint'],P1_WS=config['ws_endpoint']);env[variable]=str(out/name)
        with (out/(name+'.log')).open('w',encoding='utf-8') as log:
            result=subprocess.run(['node',str(root/'Backend/tests'/script)],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        results.append({'suite':name,'exit':result.returncode});print(name,result.returncode,flush=True)
    except Exception as error:results.append({'suite':name,'error':str(error)});print(name,str(error),flush=True)
    finally:runtime.stop()
(out/'summary.json').write_text(json.dumps(results,indent=2));print(str(out),flush=True)

sys.exit(0 if all(item.get("exit")==0 for item in results) else 1)
