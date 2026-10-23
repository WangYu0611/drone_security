"""Run the existing RHI deployment test with an actual owner heartbeat before forwarding deploy."""
import http.client
import json
from pathlib import Path
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(r'')
sys.path.insert(0, str(ROOT / 'Scripts/PlanConfiguration'))
import verify

out = ROOT / 'Evidence/EditReturnVideo/20261023-105421'
trace = out / ('deploy-heartbeat-trace-' + str(time.time_ns()) + '.jsonl')
injections = []
lock = threading.Lock()

def record(value):
    with lock, trace.open('a', encoding='utf-8') as log:
        log.write(json.dumps(value) + '\n')

def snapshot():
    conn = http.client.HTTPConnection('127.0.0.1', 22996, timeout=3)
    try:
        conn.request('GET', '/api/security-plans')
        return json.loads(conn.getresponse().read())
    finally:
        conn.close()

class Proxy(BaseHTTPRequestHandler):
    def forward(self):
        data = self.rfile.read(int(self.headers.get('Content-Length', '0')))
        body = json.loads(data) if data else {}
        injected = None
        try:
            if self.command == 'POST' and self.path == '/api/security-plans' and body.get('action') == 'deploy' and 'expected_plan_version' in body:
                before = snapshot()
                plan = body['plan_id']
                lease_seen = before['configuration_session']['last_seen_at']
                deadline = time.monotonic() + 8
                while True:
                    current = snapshot()
                    session = current['configuration_session']
                    if session and session['last_seen_at'] > lease_seen and current['version'] > body['expected_version']:
                        break
                    if time.monotonic() > deadline:
                        raise RuntimeError('No actual owner heartbeat observed; injection is not valid')
                    time.sleep(.05)
                assert current['plans'][plan]['content_revision'] == before['plans'][plan]['content_revision'] == body['expected_content_revision']
                assert current['plans'][plan]['version'] == before['plans'][plan]['version'] == body['expected_plan_version']
                assert session['acquired_version'] == body['configuration_acquired_version']
                injected = dict(action='deploy', expected_version=body['expected_version'], forwarded_at_version=current['version'],
                                plan_version=body['expected_plan_version'], revision=body['expected_content_revision'],
                                configuration_acquired_version=body['configuration_acquired_version'],
                                heartbeat_before=lease_seen, heartbeat_after=session['last_seen_at'])
            conn = http.client.HTTPConnection('127.0.0.1', 22996, timeout=15)
            try:
                conn.request(self.command, self.path, data or None, {'Content-Type': 'application/json'})
                response = conn.getresponse()
                payload = response.read()
                status = response.status
            finally:
                conn.close()
            if injected is not None:
                injected['status'] = status
                injected['response'] = json.loads(payload)
                injections.append(injected)
                record(injected)
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
        except Exception as error:
            record(dict(error=str(error), action=body.get('action')))
            payload = json.dumps(dict(code='QA_HEARTBEAT_INJECTION_FAILED', message=str(error))).encode()
            self.send_response(502)
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
    do_GET = do_POST = do_PATCH = forward
    def log_message(self, *args):
        pass

server = ThreadingHTTPServer(('127.0.0.1', 23000), Proxy)
threading.Thread(target=server.serve_forever, daemon=True).start()
original_config = verify.isolated_config
original_start = verify.start_owned_backend
def isolated_config(*args, **kwargs):
    config = original_config(*args, **kwargs)
    config['http_endpoint'] = 'http://127.0.0.1:23000'
    return config
def start_owned_backend(runtime, http_port, ws_port):
    runtime.config['http_endpoint'] = 'http://127.0.0.1:22996'
    try:
        return original_start(runtime, http_port, ws_port)
    finally:
        runtime.config['http_endpoint'] = 'http://127.0.0.1:23000'
verify.isolated_config = isolated_config
verify.start_owned_backend = start_owned_backend
try:
    code = verify.main(['DroneOps.PlanConfiguration.MissionActions', '--role', 'Command',
                        '--http-port', '22996', '--ws-port', '22997', '--timeout', '240'])
finally:
    server.shutdown()
    server.server_close()
assert injections and all(item['status'] == 200 for item in injections), 'Existing RHI test must complete an injected deployment'
print(json.dumps(dict(heartbeat_injections=len(injections), trace=str(trace), exit_code=code)))
sys.exit(code)
