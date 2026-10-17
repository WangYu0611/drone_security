"""Online Cesium / isolated Mock host. Input is tested with native mouse/keyboard."""
import json
from pathlib import Path
import sys
import time

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from environment import load_config
import runtime as launcher_runtime

config = load_config(root)
folder = root / 'Saved/FleetTerrainHeight-QA'
folder.mkdir(parents=True, exist_ok=True)
source = json.loads((root / 'Evidence/FleetTerrainHeight/surface-input.json').read_text())
fixture = folder / 'fixture.json'
fixture.write_text(json.dumps({'simulation': True, 'default_speed_mps': 8,
    'uavs': {f'UAV-{i:02}': {'home_position': p} for i, p in enumerate(source, 1)}}, indent=2))
config.update(http_endpoint='http://127.0.0.1:22180', ws_endpoint='ws://127.0.0.1:22181/ws',
              runtime_directory='Saved/FleetTerrainHeight-QA', map='/Game/Level/CesiumWorld',
              mock_fixture=str(fixture), max_fps=30)
original_arguments = launcher_runtime.role_arguments
def arguments(*args):
    result = original_arguments(*args)
    return [a + ',DroneOps.SurfaceProbe' if a.startswith('-ExecCmds=') else a for a in result]
launcher_runtime.role_arguments = arguments
runtime = launcher_runtime.Runtime(root, config)
commands = folder / 'command.json'
last = ''
try:
    runtime._backend()
    print('MOCK_BACKEND_READY', flush=True)
    while True:
        if commands.exists():
            current = commands.read_text()
            if current != last:
                last = current; request = json.loads(current)
                if request['action'] == 'stop': break
                if request['action'] == 'start': runtime.start_role(request['role'])
                if request['action'] == 'stop_role': runtime.stop_role(request['role'])
                print('COMMAND_DONE ' + current, flush=True)
        time.sleep(.5)
finally:
    runtime.stop()
    print('OWNED_PROCESSES_STOPPED', flush=True)
