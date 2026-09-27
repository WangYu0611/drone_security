"""Camera FIX-02 final-source regression. Each test owns a fresh Editor process."""
import sys,json,subprocess,os,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'Launcher'))
from runtime import Runtime
run=str(time.time_ns());out=root/'Evidence/TASK-P5.4/camera-fix'/('ue-'+run);out.mkdir(parents=True)
cfg=json.loads((root/'Launcher/config.json').read_text());cfg.update(backend_executable='Backend/build-p54/Release/DroneBackend.exe',http_endpoint='http://127.0.0.1:20180',ws_endpoint='ws://127.0.0.1:20181/ws',runtime_directory='Saved/P54-ue-'+run)
runtime=Runtime(root,cfg);env=os.environ.copy();env.update(P1_HTTP=cfg['http_endpoint'],P1_WS=cfg['ws_endpoint'],P4_EVIDENCE_DIR=str(out/'p4-fixture'),P52_EVIDENCE_DIR=str(out/'p52-fixture'),P54_ROUTE_FIXTURE=str(out/'route-fixture.json'))
cases=[('Command','Workflow.CommandLiveCRUD'),('Command','Workflow.CommandReviewReadonlyCopyAndCombo'),('Command','P52.CommandExecutionControls'),('Map','Workflow.MapLiveDraftGuard'),('Map','Localization.FTextAndDraftIsolation'),('Map','P52.MapExecutionMonitor'),('Map','P53.GeometryUIAndCesiumRoundtrip'),('Video','VideoTarget.ExplicitSelectionAndBrowserIsolation')]
cases.append(('Map','P54.CameraDampingAnd3DRoute'))
cases.append(('Map','DroneOps.Command.A2MapMode'))
if len(sys.argv)>1:
 roles={name:role for role,name in cases}
 cases=[(roles.get(name,'Map'),name) for name in sys.argv[1:]]
results=[]
try:
 runtime._backend()
 for f in ['p4_workflow_integration.mjs','p52_execution_integration.mjs','p4_ue_route_fixture.mjs']:
  with (out/(f+'.log')).open('w',encoding='utf-8') as log:subprocess.run(['node',str(root/'Backend/tests'/f)],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True)
 for role,test in cases:
  report=out/test;report.mkdir();log=report/'editor.log'
  cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'UE5DroneControl.uproject'),'/Game/Level/Entry','-ClientRole='+role,'-P1Http='+cfg['http_endpoint'],'-P1Ws='+cfg['ws_endpoint'],'-unattended','-nosplash','-ExecCmds=Automation RunTests '+(test if test.startswith('DroneOps.') else 'DroneOps.P4.'+test),'-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(report),'-abslog='+str(log)]
  with (report/'stdio.log').open('w',encoding='utf-8') as stream:
   process=subprocess.Popen(cmd,cwd=root,stdout=stream,stderr=subprocess.STDOUT,creationflags=0x08000000)
   try:code=process.wait(timeout=300)
   except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=20);code='TIMEOUT'
  summary=json.loads((report/'index.json').read_text(encoding='utf-8-sig')) if (report/'index.json').exists() else {}
  result=dict(test=test,role=role,pid=process.pid,exit=code,succeeded=summary.get('succeeded',0),warnings=summary.get('succeededWithWarnings',0),failed=summary.get('failed',0))
  result['discovered']=len(summary.get('tests',[]))
  result['completed']='Test Completed. Result=' in log.read_text(encoding='utf-8-sig',errors='replace') if log.exists() else False
  result['pass']=result['discovered']==1 and result['completed'] and result['succeeded']+result['warnings']==1 and result['failed']==0 and code==0
  results.append(result);print(json.dumps(result),flush=True);(out/'summary.json').write_text(json.dumps(results,indent=2))
finally:runtime.stop()
print(str(out),flush=True)

sys.exit(0 if results and all(item["pass"] for item in results) else 1)
