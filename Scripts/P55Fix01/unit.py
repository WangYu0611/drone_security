"""Offline Route V2 or camera-lock assertions in an empty UE editor world."""
import json, subprocess, time, sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=root/'Evidence/TASK-P5.5-FIX-01'/('offline-'+str(time.time_ns()))
out.mkdir(parents=True)
cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',
     str(root/'UE5DroneControl.uproject'),'/Engine/Maps/Entry','-unattended','-nosplash','-NullRHI',
     '-ExecCmds=Automation RunTests '+(sys.argv[1] if len(sys.argv)>1 else 'DroneOps.P55.RouteVisual'),
     '-TestExit=Automation Test Queue Empty','-ReportExportPath='+str(out),'-abslog='+str(out/'editor.log')]
with (out/'stdio.log').open('w',encoding='utf-8') as stream:
    process=subprocess.Popen(cmd,cwd=root,stdout=stream,stderr=subprocess.STDOUT,creationflags=0x08000000)
    try: code=process.wait(timeout=240)
    except subprocess.TimeoutExpired:
        process.terminate();process.wait(timeout=20);code='TIMEOUT'
report=json.loads((out/'index.json').read_text(encoding='utf-8-sig')) if (out/'index.json').exists() else {}
log=(out/'editor.log').read_text(encoding='utf-8-sig',errors='replace')
result=dict(exit=code,discovered=len(report.get('tests',[])),succeeded=report.get('succeeded',0),
            warnings=report.get('succeededWithWarnings',0),failed=report.get('failed',0),
            completed=log.count('Test Completed. Result='),evidence=str(out))
result['pass']=code==0 and result['discovered']==2 and result['completed']==2 and result['succeeded']+result['warnings']==2 and result['failed']==0
(out/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result),flush=True)
raise SystemExit(0 if result['pass'] else 1)
