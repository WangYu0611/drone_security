"""Isolated synthetic regression; never counts as native user acceptance."""
import json, os, socket, subprocess, time, urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SUITE=ROOT/'Evidence/TASK-P5.3'/('run-'+str(time.time_ns()));OUT=SUITE/'protocol';OUT.mkdir(parents=True,exist_ok=True)
print('Evidence:',SUITE,flush=True)
RUNTIME=ROOT/'Saved'/('P5.3-protocol-'+str(time.time_ns()));(RUNTIME/'data').mkdir(parents=True)
for port in (19780,19781):
    with socket.socket() as probe:probe.bind(('127.0.0.1',port))
(RUNTIME/'data/mock_execution.json').write_bytes((ROOT/'Backend/mock_execution.json').read_bytes())
registry=[dict(id=f'd{i}',slot=i,name=f'Mock UAV {i:02}',model='SYNTHETIC-QA',ip='127.0.0.1',port=29999,video_url='') for i in range(1,5)]
(RUNTIME/'data/drones.json').write_text(json.dumps(registry),encoding='utf-8')
(RUNTIME/'backend.yaml').write_text('server:\n  http_port: 19780\n  ws_port: 19781\n  debug: true\nport_map: {}\nstorage:\n  path: "data/drones.json"\nlog:\n  file: "backend.log"\n',encoding='utf-8')
env=os.environ|{'P1_HTTP':'http://127.0.0.1:19780','P1_WS':'ws://127.0.0.1:19781/ws','P4_EVIDENCE_DIR':str(OUT/'p4'),'P5_EVIDENCE_DIR':str(OUT/'p5'),'P51_EVIDENCE_DIR':str(OUT/'p51'),'P53_EVIDENCE_DIR':str(OUT/'p53'),'P52_EVIDENCE_DIR':str(OUT/'p52')}
def get():
    with urllib.request.urlopen(env['P1_HTTP']+'/api/security-plans',timeout=2) as r:return json.load(r)
log=(OUT/'backend.log').open('ab')
def launch():
    p=subprocess.Popen([str(ROOT/'Backend/build-p53/Release/DroneBackend.exe'),str(RUNTIME/'backend.yaml')],cwd=RUNTIME,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    for _ in range(100):
        try:get();return p
        except OSError:time.sleep(.1)
    p.terminate();raise RuntimeError('Backend did not become ready')
p=launch()
try:
    for suite in ['p4_workflow_integration','p5_presence_integration','p51_workflow_integration','p52_execution_integration','p53_geometry_integration']:
        with (OUT/(suite+'.log')).open('w',encoding='utf-8') as output:
            subprocess.run(['node',str(ROOT/'Backend/tests'/f'{suite}.mjs')],cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT,check=True)
    before=get();p.terminate();p.wait(timeout=10);p=launch();after=get()
    assert before['executions']==after['executions'];assert before['deployments']==after['deployments']
    (OUT/'restart.json').write_text(json.dumps({'synthetic':True,'paused_restart_preserved':True,'execution_count':len(after['executions'])},indent=2),encoding='utf-8')
    if any(a in __import__('sys').argv for a in ('--ue','--ue-map','--ue-map-isolated','--ue-all-isolated')):
        (SUITE/'ue').mkdir(parents=True,exist_ok=True)
        fixture=(ROOT/'Scripts/P5.1/route_fixture.mjs').read_text().replace("'http://127.0.0.1:19580'","'http://127.0.0.1:19780'").replace('ws://127.0.0.1:19581/ws','ws://127.0.0.1:19781/ws').replace('Evidence/TASK-P5.1/automation/route-fixture.json',(SUITE/'ue/route-fixture.json').as_posix())
        route_fixture=RUNTIME/'route_fixture.mjs';route_fixture.write_text(fixture)
        subprocess.run(['node',str(route_fixture)],cwd=ROOT,check=True)
        ue_failures=[]
        for role,tests,count in [
            ('Command','DroneOps.P4.Workflow.CommandLiveCRUD+DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo+DroneOps.P4.P52.CommandExecutionControls',3),
            ('Map','DroneOps.P4.Workflow.MapLiveDraftGuard+DroneOps.P4.Localization.FTextAndDraftIsolation+DroneOps.P4.P52.MapExecutionMonitor+DroneOps.P4.P53.GeometryUIAndCesiumRoundtrip',4),
            ('Video','DroneOps.P4.VideoTarget.ExplicitSelectionAndBrowserIsolation',1)]:
            isolated=any(a in __import__('sys').argv for a in ('--ue-map-isolated','--ue-all-isolated'))
            if any(a in __import__('sys').argv for a in ('--ue-map','--ue-map-isolated')) and role!='Map':continue
            # Cesium's Gaussian-splat subsystem can retain an invalid object across
            # repeated PIE worlds. Isolated mode retains each unmodified test and
            # its original export, while starting a fresh editor for every world.
            groups=[(t,1,Path(role+'-isolated')/t.rsplit('.',1)[-1]) for t in tests.split('+')] if isolated else [(tests,count,Path(role))]
            for selected,expected,folder in groups:
                report=SUITE/'ue'/folder;report.mkdir(parents=True,exist_ok=True)
                output=report/'run.log';started=time.time()
                args=[r'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(ROOT/'UE5DroneControl.uproject'),'/Engine/Maps/Entry','-ClientRole='+role,'-P1Http='+env['P1_HTTP'],'-P1Ws='+env['P1_WS'],'-unattended','-nosplash','-ExecCmds=Automation RunTests '+selected,'-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(report),'-abslog='+str(output)]
                result=subprocess.run(args,cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
                if result.returncode!=0:
                    ue_failures.append({'role':role,'test':selected,'process_exit':result.returncode,'report':str(report)});continue
                assert (report/'index.json').stat().st_mtime>=started
                summary=json.loads((report/'index.json').read_text(encoding='utf-8-sig'))
                if summary['failed']!=0 or summary['succeeded']+summary.get('succeededWithWarnings',0)!=expected:
                    ue_failures.append({'role':role,'report':str(report),'failed':summary['failed']})
                else:
                    assert output.read_text(encoding='utf-8-sig').count('Test Completed. Result={Success}')==expected
        assert not ue_failures,ue_failures
finally:p.terminate();p.wait(timeout=10);log.close()
