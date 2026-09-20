"""Stage 1 lifecycle. Existing Backend HTTP/WS remains the only shared authority."""
from __future__ import annotations
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import threading
import time
import urllib.request
import urllib.parse
import uuid

ROLES = ('Command', 'Map', 'Video')

def api(base, path):
    with urllib.request.urlopen(base.rstrip('/') + path, timeout=2) as response:
        return json.load(response)

def compatible_snapshot(base):
    context = api(base, '/api/context')
    clients = api(base, '/api/context/clients')
    prefs = api(base, '/api/ui-preferences')
    video = api(base, '/api/video-view')
    plans = api(base, '/api/security-plans')
    if not (isinstance(context.get('context_version'), int)
            and 'active_uav_id' in context and isinstance(clients, list)
            and prefs.get('language') in ('en', 'zh-Hans')
            and 'video_view_version' in video and 'plans' in plans):
        raise ValueError('Incompatible Backend snapshot')
    return {'context': context, 'clients': clients, 'preferences': prefs,
            'video': video, 'plans': plans}

def port_open(endpoint):
    uri = urllib.parse.urlparse(endpoint)
    try:
        with socket.create_connection((uri.hostname, uri.port), timeout=.3):
            return True
    except OSError:
        return False

class SingleInstance:
    def __init__(self, endpoint):
        self.handle = None
        if os.name == 'nt':
            kernel = ctypes.WinDLL('kernel32', use_last_error=True)
            kernel.CreateMutexW.restype = wintypes.HANDLE
            kernel.CreateMutexW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.LPCWSTR]
            name = 'Local\\DroneSecurity-' + hashlib.sha256(endpoint.encode()).hexdigest()[:24]
            self.handle = kernel.CreateMutexW(None, False, name)
            if not self.handle or ctypes.get_last_error() == 183:
                if self.handle:
                    kernel.CloseHandle(wintypes.HANDLE(self.handle))
                self.handle = None
                raise RuntimeError('Launcher already running for this Backend')
    def close(self):
        if self.handle:
            ctypes.windll.kernel32.CloseHandle(wintypes.HANDLE(self.handle))
            self.handle = None

class OwnedProcess:
    """Popen retains a process handle: termination cannot target a recycled PID."""
    def __init__(self, role, args, cwd, log_dir):
        self.role, self.args = role, args
        self.started_at = time.time()
        self.output = open(log_dir / f'{role.lower()}-stdio-{time.time_ns()}.log', 'ab')
        flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
        self.process = subprocess.Popen(args, cwd=cwd, stdout=self.output,
                                        stderr=subprocess.STDOUT, creationflags=flags)
        self.job = None
        if os.name == 'nt':
            self._own_job()
    def _own_job(self):
        # Kill-on-close contains browser/helper descendants as well as the root UE.
        class Basic(ctypes.Structure):
            _fields_ = [('ProcessTime', ctypes.c_longlong), ('JobTime', ctypes.c_longlong),
                        ('Flags', wintypes.DWORD), ('Min', ctypes.c_size_t), ('Max', ctypes.c_size_t),
                        ('Count', wintypes.DWORD), ('Affinity', ctypes.c_size_t),
                        ('Priority', wintypes.DWORD), ('Scheduling', wintypes.DWORD)]
        class Io(ctypes.Structure):
            _fields_ = [(n, ctypes.c_ulonglong) for n in ('ReadOp','WriteOp','OtherOp','Read','Write','Other')]
        class Extended(ctypes.Structure):
            _fields_ = [('Basic', Basic), ('Io', Io), ('ProcessMemory', ctypes.c_size_t),
                        ('JobMemory', ctypes.c_size_t), ('PeakProcess', ctypes.c_size_t), ('PeakJob', ctypes.c_size_t)]
        k = ctypes.WinDLL('kernel32', use_last_error=True)
        k.CreateJobObjectW.restype = wintypes.HANDLE
        job = k.CreateJobObjectW(None, None)
        limits = Extended(); limits.Basic.Flags = 0x2000
        if not job or not k.SetInformationJobObject(wintypes.HANDLE(job), 9, ctypes.byref(limits), ctypes.sizeof(limits)) or not k.AssignProcessToJobObject(wintypes.HANDLE(job), wintypes.HANDLE(int(self.process._handle))):
            if job: k.CloseHandle(wintypes.HANDLE(job))
            self.process.terminate(); self.process.wait(timeout=10)
            raise OSError('Cannot establish process-tree ownership')
        self.job = job
    def record(self):
        return {'role': self.role, 'pid': self.process.pid, 'start_time': self.started_at,
                'executable': str(Path(self.args[0]).resolve()), 'owned': True,
                'exit_code': self.process.poll()}
    def stop(self):
        if self.process.poll() is None:
            if os.name == 'nt':
                user = ctypes.windll.user32
                callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
                @callback
                def close_window(hwnd, _):
                    pid = wintypes.DWORD()
                    user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                    if pid.value == self.process.pid: user.PostMessageW(hwnd, 0x10, 0, 0)
                    return True
                user.EnumWindows(close_window, 0)
            else:
                self.process.terminate()
            try: self.process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.process.terminate(); self.process.wait(timeout=10)
        if self.job:
            ctypes.windll.kernel32.CloseHandle(wintypes.HANDLE(self.job)); self.job = None
        self.output.close()

def role_arguments(config, root, role, instance, log):
    return [str(root / config['ue_executable']), str(root / config['project']), config['map'],
            '-game', '-windowed', '-ResX=1280', '-ResY=800', '-nosplash', '-unattended',
            '-ClientRole=' + role, '-P5Instance=' + instance,
            '-P1Http=' + config['http_endpoint'], '-P1Ws=' + config['ws_endpoint'],
            '-ExecCmds=t.MaxFPS ' + str(config['max_fps']), '-abslog=' + str(log)]

class Runtime:
    def __init__(self, root, config):
        self.root, self.config = Path(root), config
        self.directory = self.root / config['runtime_directory']
        self.directory.mkdir(parents=True, exist_ok=True)
        self.logs = self.directory / 'Logs'; self.logs.mkdir(exist_ok=True)
        self.lock = threading.RLock()
        self.owned = {}
        self.instances = {}
        self.state = 'STOPPED'
        self.components = {r: 'OFFLINE' for r in ('Backend',) + ROLES}
        self.snapshot = None
        self.existing_backend = False
        self.error = ''
        self.deadline = 0
        self.stopping = threading.Event()
        self.transition('STOPPED')
    def log(self, event, **data):
        with self.lock:
            with (self.logs / 'launcher.jsonl').open('a', encoding='utf-8') as out:
                out.write(json.dumps({'at': time.time(), 'event': event, **data}, ensure_ascii=False)+'\n')
    def transition(self, state):
        if self.state != state: self.log('system', previous=self.state, state=state)
        self.state = state
    def _persist_ownership(self):
        records = [p.record() for p in self.owned.values()]
        temp = self.directory / 'ownership.tmp'
        temp.write_text(json.dumps(records, indent=2), encoding='utf-8')
        temp.replace(self.directory / 'ownership.json')
    def _spawn(self, role, args, cwd):
        if role in self.owned and self.owned[role].process.poll() is None:
            raise RuntimeError(role + ' already running')
        old = self.owned.pop(role, None)
        if old: old.stop()
        process = OwnedProcess(role, args, cwd, self.logs)
        self.owned[role] = process
        self.components[role] = 'STARTING'
        self.log('launch', **process.record())
        self._persist_ownership()
    def _backend(self):
        base = self.config['http_endpoint']
        if port_open(base):
            self.snapshot = compatible_snapshot(base)
            self.existing_backend = 'Backend' not in self.owned
            self.log('backend_existing' if self.existing_backend else 'backend_ready')
            return
        if port_open(self.config['ws_endpoint']):
            raise RuntimeError('WebSocket port occupied without compatible Backend')
        uri, ws = urllib.parse.urlparse(base), urllib.parse.urlparse(self.config['ws_endpoint'])
        if uri.hostname not in ('127.0.0.1', 'localhost') or ws.hostname not in ('127.0.0.1', 'localhost'):
            raise RuntimeError('Stage 1 requires local Backend')
        self.existing_backend = False
        # Fresh simulation data only; existing confirmed business data is never replaced.
        data = self.directory / 'data'; data.mkdir(exist_ok=True)
        fixture = json.loads((self.root / self.config['mock_fixture']).read_text(encoding='utf-8'))
        for name,value in [('mock_execution.json',fixture),('drones.json',[
                {'id':f'd{i}','slot':i,'name':f'Mock UAV {i:02}','model':'Stage 1 Simulation',
                 'ip':'127.0.0.1','port':29999,'video_url':''} for i in range(1,5)])]:
            target=data/name
            if not target.exists():target.write_text(json.dumps(value,indent=2),encoding='utf-8')
        config_path = self.directory / 'backend.yaml'
        config_path.write_text(f'''server:
  http_port: {uri.port}
  ws_port: {ws.port}
  debug: false
port_map: {{}}
storage:
  path: "data/drones.json"
  video_metadata_path: "metadata"
log:
  level: "info"
  file: "Logs/backend.log"
''', encoding='utf-8')
        self._spawn('Backend', [str(self.root / self.config['backend_executable']), str(config_path)], self.directory)
        end = time.monotonic() + 20
        while time.monotonic() < end and not self.stopping.is_set():
            if self.owned['Backend'].process.poll() is not None: raise RuntimeError('Backend exited before ready')
            try:
                self.snapshot = compatible_snapshot(base); return
            except (OSError, ValueError): self.stopping.wait(.3)
        raise RuntimeError('Backend readiness timed out')
    def start_role(self, role):
        with self.lock:
            if role not in ROLES: raise ValueError(role)
            snap = compatible_snapshot(self.config['http_endpoint'])
            if any(c.get('client_role') == role and c.get('state') == 'ONLINE' for c in snap['clients']):
                raise RuntimeError(role + ' already ONLINE; refusing duplicate')
            instance = str(uuid.uuid4()); self.instances[role] = instance
            log = self.logs / f'{role.lower()}-{time.time_ns()}.log'
            self._spawn(role, role_arguments(self.config, self.root, role, instance, log), self.root)
            self.deadline = time.monotonic() + self.config['startup_timeout']
    def start(self):
        with self.lock:
            if self.state not in ('STOPPED', 'FAILED'): return
            if any(p.process.poll() is None for p in self.owned.values()):
                raise RuntimeError('Stop the current session before relaunch')
            self.stopping.clear(); self.error = ''; self.transition('STARTING')
            try:
                self._backend()
                for role in ROLES:
                    if self.stopping.is_set(): break
                    self.start_role(role)
            except Exception as exc:
                self.error = str(exc); self.log('error', message=self.error)
                self.stop(); self.transition('FAILED')
                raise
    def poll(self):
        with self.lock:
            if self.state in ('STOPPED', 'STOPPING', 'FAILED'): return
            old = dict(self.components)
            try:
                snap = compatible_snapshot(self.config['http_endpoint'])
                self.snapshot = snap; self.components['Backend'] = 'ONLINE'
                for role in ROLES:
                    process = self.owned.get(role)
                    client = next((c for c in snap['clients'] if c.get('instance_id') == self.instances.get(role)), {})
                    self.components[role] = ('ONLINE' if process and process.process.poll() is None
                        and client.get('state') == 'ONLINE' and client.get('hydrated') is True
                        and time.time() - client.get('last_seen', 0) < 12 else
                        'STARTING' if process and process.process.poll() is None and time.monotonic() < self.deadline else 'OFFLINE')
            except Exception:
                self.components['Backend'] = 'OFFLINE'
                for role in ROLES:
                    p = self.owned.get(role)
                    self.components[role] = 'RECONNECTING' if p and p.process.poll() is None else 'OFFLINE'
            ready = all(s == 'ONLINE' for s in self.components.values())
            self.transition('READY' if ready else 'STARTING' if self.state == 'STARTING' and time.monotonic() < self.deadline else 'DEGRADED')
            for role, value in self.components.items():
                if value != old[role]: self.log('component', role=role, previous=old[role], state=value)
            self._persist_ownership()
    def stop_role(self, role):
        with self.lock:
            p = self.owned.pop(role, None)
            if p:
                p.stop(); self.log('exit', **p.record()); self.components[role] = 'OFFLINE'
            self._persist_ownership()
    def restart(self, role):
        with self.lock:
            if role == 'Backend' and self.existing_backend:
                raise RuntimeError('Existing Backend is not owned by Launcher')
            self.stop_role(role)
            if role == 'Backend': self._backend()
            else:
                # Wait for the old WS binding to close without manufacturing presence.
                end = time.monotonic() + 15
                while time.monotonic() < end:
                    snap = compatible_snapshot(self.config['http_endpoint'])
                    if not any(c.get('instance_id') == self.instances.get(role) and c.get('state') == 'ONLINE' for c in snap['clients']): break
                    time.sleep(.2)
                self.start_role(role)
    def has_active_execution(self):
        plans=api(self.config['http_endpoint'],'/api/security-plans')
        return any(e.get('state') not in ('COMPLETED','ABORTED','FAILED') for e in plans.get('executions',{}).values())
    def pause_for_shutdown(self):
        body=json.dumps({'request_id':str(uuid.uuid4()),'simulation':True}).encode()
        request=urllib.request.Request(self.config['http_endpoint']+'/api/mock-executions/pause-all',data=body,headers={'Content-Type':'application/json'},method='POST')
        with urllib.request.urlopen(request,timeout=5) as response:json.load(response)
    def stop(self):
        if 'Backend' in self.owned and self.owned['Backend'].process.poll() is None and port_open(self.config['http_endpoint']):
            self.pause_for_shutdown()
        self.stopping.set()
        with self.lock:
            self.transition('STOPPING')
            errors = []
            for role in ROLES + ('Backend',):
                try: self.stop_role(role)
                except Exception as exc: errors.append(role + ': ' + str(exc))
            if errors:
                self.error = '; '.join(errors); self.transition('FAILED'); raise RuntimeError(self.error)
            self.components = {r: 'OFFLINE' for r in ('Backend',) + ROLES}
            self.transition('STOPPED')
