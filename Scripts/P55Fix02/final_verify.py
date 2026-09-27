"""FIX-02: route ownership/materials, camera locks, real widget lifecycle and reservation UI."""
import json, os, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
commands=[
 [sys.executable,'Scripts/P55Fix02/unit.py'],
 [sys.executable,'Scripts/P55Fix02/unit.py','DroneOps.P54.RouteEditCameraLock'],
 [sys.executable,'Scripts/P55Fix02/regression.py','DroneOps.P55.SecurityPlanRouteVisual','Workflow.MapLiveDraftGuard','P54.CameraDampingAnd3DRoute','P52.MapExecutionMonitor','Localization.FTextAndDraftIsolation','DroneOps.P55.ReservationWidgets','P52.CommandExecutionControls'],
]
env=os.environ.copy();env['P55_OFFLINE_QA']='1';results=[]
for command in commands:
 r=subprocess.run(command,cwd=root,env=env,text=True,capture_output=True)
 print(r.stdout,flush=True)
 if r.stderr:print(r.stderr,flush=True)
 results.append(dict(command=command,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
 (root/'Evidence/TASK-P5.5-FIX-02/final-verification.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
raise SystemExit(0 if all(r['exit']==0 for r in results) else 1)
