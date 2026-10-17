"""Fresh Mock store and Editor for each terrain/height/camera regression."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from environment import load_config
from runtime import Runtime

run = str(time.time_ns())
out = root / 'Evidence/FleetTerrainHeight' / ('verification-' + run)
out.mkdir(parents=True)
tests = sys.argv[1:] or ['DroneOps.Fleet.SurfaceHeight', 'DroneOps.Fleet.TerrainWorkflow',
    'DroneOps.Fleet.ThreeDimensionalWorkflow', 'DroneOps.Fleet.Workflow',
    'DroneOps.P54.RouteEditCameraLock', 'DroneOps.P4.P54.CameraDampingAnd3DRoute',
    'DroneOps.P4.Localization.FTextAndDraftIsolation']
results = []
for index, name in enumerate(tests):
    config = load_config(root)
    config.update(http_endpoint='http://127.0.0.1:22380', ws_endpoint='ws://127.0.0.1:22381/ws',
                  runtime_directory=f'Saved/FleetTerrainHeight-verify-{run}-{index}')
    runtime = Runtime(root, config)
    report = out / name
    report.mkdir()
    try:
        runtime._backend()
        command = [config['ue_executable'], str(root / config['project']), '/Game/Tests/P55OfflineMap',
            '-P55OfflineVisualQA', '-ClientRole=Map', '-P1Http=' + config['http_endpoint'],
            '-P1Ws=' + config['ws_endpoint'], '-unattended', '-nosplash',
            '-ExecCmds=Automation RunTests ' + name, '-TestExit=Automation Test Queue Empty',
            '-ReportExportPath=' + str(report), '-abslog=' + str(report / 'editor.log')]
        if name not in ['DroneOps.Fleet.TerrainWorkflow', 'DroneOps.Fleet.ThreeDimensionalWorkflow',
                        'DroneOps.Fleet.Workflow', 'DroneOps.P4.P54.CameraDampingAnd3DRoute']:
            command.append('-NullRHI')
        with (report / 'stdio.log').open('w', encoding='utf-8') as stream:
            process = subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                code = process.wait(timeout=240)
            except subprocess.TimeoutExpired:
                process.terminate(); process.wait(timeout=20); code = 'TIMEOUT'
        summary = json.loads((report / 'index.json').read_text(encoding='utf-8-sig')) if (report / 'index.json').exists() else {}
        log = (report / 'editor.log').read_text(encoding='utf-8-sig', errors='replace')
        expected = 2 if name == 'DroneOps.P54.RouteEditCameraLock' else 1
        result = dict(test=name, exit=code, discovered=len(summary.get('tests', [])),
                      completed=log.count('Test Completed. Result='), failed=summary.get('failed', 0),
                      passed=summary.get('succeeded', 0) + summary.get('succeededWithWarnings', 0))
        result['pass'] = code == 0 and result['discovered'] == result['completed'] == result['passed'] == expected and result['failed'] == 0
        results.append(result)
        (out / 'summary.json').write_text(json.dumps(results, indent=2))
        print(json.dumps(result), flush=True)
    finally:
        runtime.stop()
binary = root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll'
(out / 'binary.json').write_text(json.dumps({'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}, indent=2))
print(out, flush=True)
sys.exit(0 if results and all(r['pass'] for r in results) else 1)
