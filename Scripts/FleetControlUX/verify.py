"""Fresh Editor process per test; no test passes without discovery and completion."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'Launcher'))
from runtime import Runtime
run=str(time.time_ns())
out=root/'Evidence/FleetControlUX'/('verification-'+run)
out.mkdir(parents=True)
config=json.loads((root/'Launcher/config.json').read_text())
config.update(backend_executable='Backend/build-p54/Release/DroneBackend.exe',
              http_endpoint='http://127.0.0.1:21780',ws_endpoint='ws://127.0.0.1:21781/ws',
              runtime_directory='Saved/FleetControlUX-verify-'+run)
runtime=Runtime(root,config)
results=[]
regression_ready=False
try:
    runtime._backend()
    tests=sys.argv[1:] or ['DroneOps.Fleet.Selection','DroneOps.Fleet.TargetProjection','DroneOps.Fleet.Presentation','DroneOps.Fleet.ThreeDimensionalWorkflow','DroneOps.Fleet.Workflow','DroneOps.P54.RouteEditCameraLock','DroneOps.P55.SecurityPlanRouteVisual','DroneOps.P4.P52.MapExecutionMonitor','DroneOps.P4.Localization.FTextAndDraftIsolation']
    for test in tests:
        if test in ('DroneOps.P55.SecurityPlanRouteVisual','DroneOps.P4.P52.MapExecutionMonitor') and not regression_ready:
            # These existing suites require saved three-Mission geometry and a
            # paused Security Plan execution. Keep that reserved UAV out of the
            # preceding free-fleet workflows by using a separate Mock runtime.
            runtime.stop()
            config['runtime_directory']+='-regression'
            runtime=Runtime(root,config);runtime._backend()
            fixture_env=os.environ.copy()
            fixture_env.update(P1_HTTP=config['http_endpoint'],P1_WS=config['ws_endpoint'],
                               P4_EVIDENCE_DIR=str(out/'p4-fixture'),P52_EVIDENCE_DIR=str(out/'p52-fixture'),
                               P52_UAV='UAV-04',P52_LONGITUDE='116.34712',P54_ROUTE_FIXTURE=str(out/'route-fixture.json'))
            fixtures=['Backend/tests/p4_workflow_integration.mjs','Backend/tests/p4_ue_route_fixture.mjs',
                      'Scripts/P55Fix02/fixture.mjs','Backend/tests/p52_execution_integration.mjs']
            for index,fixture in enumerate(fixtures):
                with (out/f'fixture-{index}.log').open('w',encoding='utf-8') as stream:
                    subprocess.run(['node',str(root/fixture)],cwd=root,env=fixture_env,stdout=stream,stderr=subprocess.STDOUT,timeout=180,check=True)
            regression_ready=True
        report=out/test
        report.mkdir()
        command=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',
                 str(root/'UE5DroneControl.uproject'),'/Game/Tests/P55OfflineMap',
                 '-P55OfflineVisualQA','-ClientRole=Map','-P1Http='+config['http_endpoint'],
                 '-P1Ws='+config['ws_endpoint'],'-unattended','-nosplash',
                 '-ExecCmds=Automation RunTests '+test,'-TestExit=Automation Test Queue Empty',
                 '-ReportExportPath='+str(report),'-abslog='+str(report/'editor.log')]
        if test not in ('DroneOps.Fleet.Workflow','DroneOps.Fleet.ThreeDimensionalWorkflow','DroneOps.P55.SecurityPlanRouteVisual'):command.append('-NullRHI')
        with (report/'stdio.log').open('w',encoding='utf-8') as stream:
            process=subprocess.Popen(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT,creationflags=0x08000000)
            try:code=process.wait(timeout=240)
            except subprocess.TimeoutExpired:
                process.terminate();process.wait(timeout=20);code='TIMEOUT'
        summary=json.loads((report/'index.json').read_text(encoding='utf-8-sig')) if (report/'index.json').exists() else {}
        log=(report/'editor.log').read_text(encoding='utf-8-sig',errors='replace')
        expected=2 if test=='DroneOps.P54.RouteEditCameraLock' else 1
        result=dict(test=test,exit=code,discovered=len(summary.get('tests',[])),passed=summary.get('succeeded',0)+summary.get('succeededWithWarnings',0),warnings=summary.get('succeededWithWarnings',0),failed=summary.get('failed',0),completed=log.count('Test Completed. Result='))
        result['pass']=code==0 and result['discovered']==expected and result['completed']==expected and result['passed']==expected and result['failed']==0
        results.append(result);print(json.dumps(result),flush=True)
        (out/'summary.json').write_text(json.dumps(results,indent=2))
    runtime.stop()
    config['runtime_directory']+='-protocol'
    runtime=Runtime(root,config)
    runtime._backend()
    env=os.environ.copy()
    env.update(P1_HTTP=config['http_endpoint'],P1_WS=config['ws_endpoint'],FLEET_EVIDENCE=str(out/'protocol'),FLEET_ALTITUDE_DELTA='50')
    with (out/'protocol.log').open('w',encoding='utf-8') as stream:
        code=subprocess.run(['node',str(root/'Scripts/FleetControl/protocol.mjs')],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=120).returncode
    results.append({'protocol_exit':code,'pass':code==0});(out/'summary.json').write_text(json.dumps(results,indent=2))
finally:
    runtime.stop()
print(str(out),flush=True)
sys.exit(0 if results and all(r['pass'] for r in results) else 1)
