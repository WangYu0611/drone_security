"""Task-owned QA host. Existing launcher owns every process and preserves user data."""
import sys,json,time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'Launcher'))
from runtime import Runtime
config=json.loads((root/'Launcher/config.json').read_text())
config.update(backend_executable='Backend/build-p54/Release/DroneBackend.exe',ue_executable='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',http_endpoint='http://127.0.0.1:20280',ws_endpoint='ws://127.0.0.1:20281/ws',runtime_directory='Saved/P54-UX01-QA',max_fps=15)
runtime=Runtime(root,config)
commands=runtime.directory/'command.json'
last=''
try:
    runtime._backend()
    print('P54_BACKEND_READY',flush=True)
    while True:
        if commands.exists():
            text=commands.read_text()
            if text!=last:
                last=text;request=json.loads(text)
                if request['action']=='stop':break
                if request['action']=='start':runtime.start_role(request['role'])
                if request['action']=='stop_role':runtime.stop_role(request['role'])
                if request['action']=='restart_backend':runtime.stop_role('Backend');runtime._backend()
                print('COMMAND_DONE '+text,flush=True)
        time.sleep(.5)
finally:
    runtime.stop()
    print('P54_OWNED_PROCESSES_STOPPED',flush=True)
