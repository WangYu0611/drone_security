"""Isolated Mock host; commands start/stop only processes owned by this Runtime."""
import json
import sys
import time
from pathlib import Path

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from runtime import Runtime
from environment import load_config

config = load_config(root)
config.update(mock_fixture='Backend/tests/fixtures/mock_execution_beijing.json', mock_reset_on_start=False, ue_executable='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',
              http_endpoint='http://127.0.0.1:20780', ws_endpoint='ws://127.0.0.1:20781/ws',
              runtime_directory='Saved/FleetControl-QA', max_fps=30,
              map='/Game/Tests/P55OfflineMap')
runtime = Runtime(root, config)
commands = runtime.directory / 'command.json'
last = ''
try:
    runtime._backend()
    print('MOCK_BACKEND_READY', flush=True)
    while True:
        if commands.exists():
            current = commands.read_text()
            if current != last:
                last = current
                request = json.loads(current)
                if request['action'] == 'stop':
                    break
                if request['action'] == 'start':
                    runtime.start_role(request['role'])
                if request['action'] == 'stop_role':
                    runtime.stop_role(request['role'])
                print('COMMAND_DONE ' + current, flush=True)
        time.sleep(.5)
finally:
    runtime.stop()
    print('OWNED_PROCESSES_STOPPED', flush=True)
