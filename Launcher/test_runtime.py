import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from runtime import Runtime, OwnedProcess, compatible_snapshot, role_arguments, SingleInstance
from windows import rectangles

class LauncherTests(unittest.TestCase):
    def test_SingleScreenMultiWindowLayout(self):
        for width,height in [(2048,1104),(1920,1040),(1366,728)]:
            result=rectangles((0,0,width,height))
            self.assertEqual(set(result), {'Command','Map','Video'})
            for x,y,w,h in result.values():
                self.assertGreater(w,0);self.assertGreater(h,0)
                self.assertLessEqual(x+w,width);self.assertLessEqual(y+h,height)
            cmd,m,v=(result[r] for r in ('Command','Map','Video'))
            self.assertLess(cmd[0]+cmd[2],m[0]);self.assertLess(m[1]+m[3],v[1])
    def test_BackendReadiness(self):
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *a): pass
            def do_GET(self):
                self.send_response(200); self.end_headers(); self.wfile.write(b'{}')
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True); worker.start()
        try:
            with self.assertRaises(ValueError): compatible_snapshot(f'http://127.0.0.1:{server.server_port}')
        finally: server.shutdown(); server.server_close()
    def test_RoleArguments(self):
        root = Path(__file__).resolve().parents[1]
        cfg = json.loads((root/'Launcher/config.json').read_text(encoding='utf-8-sig'))
        for role in ('Command','Map','Video'):
            args = role_arguments(cfg, root, role, 'owned-instance', root/'Saved/role.log')
            self.assertIn('-ClientRole='+role, args)
            self.assertIn('-P5Instance=owned-instance', args)
            self.assertIn('-P1Http='+cfg['http_endpoint'], args)
            self.assertIn('-P1Ws='+cfg['ws_endpoint'], args)
    def test_ProcessOwnershipAndSafeShutdown(self):
        with tempfile.TemporaryDirectory() as folder:
            unrelated = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'], creationflags=subprocess.CREATE_NO_WINDOW)
            owned = OwnedProcess('Test', [sys.executable, '-c', 'import time; time.sleep(60)'], folder, Path(folder))
            try:
                owned.stop()
                self.assertIsNotNone(owned.process.poll())
                self.assertIsNone(unrelated.poll())
                owned.stop()  # idempotent, does not target any new process by PID
            finally: unrelated.terminate(); unrelated.wait()
    def test_ActiveExecutionShutdownWarningPredicate(self):
        runtime=object.__new__(Runtime);runtime.config={'http_endpoint':'http://test'}
        for state in ('CREATED','PREFLIGHT','STARTING','EXECUTING','PAUSED','RETURNING','COMPLETED','ABORTED','FAILED'):
            with patch('runtime.api',return_value={'executions':{'e':{'state':state}}}):
                self.assertEqual(runtime.has_active_execution(),state not in ('COMPLETED','ABORTED','FAILED'))
    def test_ShutdownRequiresBackendPersistenceAck(self):
        runtime=object.__new__(Runtime);runtime.config={'http_endpoint':'http://test'}
        with patch('runtime.urllib.request.urlopen',side_effect=OSError('unavailable')):
            with self.assertRaises(OSError):runtime.pause_for_shutdown()
    def test_SingleInstance(self):
        lock = SingleInstance('test-endpoint-unique')
        try:
            with self.assertRaises(RuntimeError): SingleInstance('test-endpoint-unique')
        finally: lock.close()
        SingleInstance('test-endpoint-unique').close()

if __name__ == '__main__': unittest.main(verbosity=2)
