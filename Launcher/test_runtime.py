import json
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from runtime import (Runtime, OwnedProcess, compatible_snapshot, role_arguments, SingleInstance,
                     BackendCompatibilityError, REQUIRED_CAPABILITIES)
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
                self.send_response(200); self.end_headers()
                self.wfile.write(json.dumps({'capabilities': sorted(REQUIRED_CAPABILITIES)}
                                            if self.path == '/' else {}).encode())
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True); worker.start()
        try:
            with self.assertRaises(ValueError): compatible_snapshot(f'http://127.0.0.1:{server.server_port}')
        finally: server.shutdown(); server.server_close()
    def test_FleetCompatibilityRejectsHealthyLegacyBackendWithoutTakingOwnership(self):
        requests = []
        payloads = {
            '/': {'status': 'ok'},
            '/api/context': {'context_version': 1, 'active_uav_id': None},
            '/api/context/clients': [],
            '/api/ui-preferences': {'language': 'en'},
            '/api/video-view': {'video_view_version': 1},
            '/api/security-plans': {'version': 1, 'plans': {}, 'executions': {}},
        }
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *a): pass
            def do_GET(self):
                requests.append(('GET', self.path))
                self.send_response(200); self.end_headers()
                self.wfile.write(json.dumps(payloads[self.path]).encode())
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f'http://127.0.0.1:{server.server_port}'
        try:
            with tempfile.TemporaryDirectory() as folder:
                runtime = Runtime(folder, {'http_endpoint': base, 'runtime_directory': 'Saved/test'})
                for capabilities in (None, [], ['fleet_move_3d'], ['fleet_move_3d', 'map_fleet_controls'],
                                     'fleet_move_3d', {'map_fleet_controls': True}):
                    with self.subTest(capabilities=capabilities):
                        payloads['/']['capabilities'] = capabilities
                        with self.assertRaisesRegex(BackendCompatibilityError, 'SetupDroneSecurity.cmd'):
                            runtime.start()
                        self.assertEqual(runtime.state, 'FAILED')
                        self.assertEqual(runtime.owned, {})
                        self.assertFalse((runtime.directory / 'data').exists())
                payloads['/']['capabilities'] = sorted(REQUIRED_CAPABILITIES)
                runtime._backend()
                self.assertTrue(runtime.existing_backend)
                self.assertEqual(runtime.owned, {})
                self.assertEqual(runtime.snapshot['plans'], payloads['/api/security-plans'])
                runtime.state = 'READY'
                runtime.components = {role: 'ONLINE' for role in runtime.components}
                payloads['/']['capabilities'] = []
                with self.assertRaises(BackendCompatibilityError):
                    runtime.poll()
                self.assertEqual(runtime.state, 'DEGRADED')
                self.assertEqual(runtime.components['Backend'], 'OFFLINE')
                self.assertEqual(runtime.owned, {})
                self.assertTrue(all(method == 'GET' for method, _ in requests))
        finally:
            server.shutdown(); server.server_close()
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
        for state in ('CREATED','PREFLIGHT','STARTING','EXECUTING','PAUSED','RETURNING','COMPLETED','CANCELLED','ABORTED','FAILED'):
            with patch('runtime.api',return_value={'executions':{'e':{'state':state}}}):
                self.assertEqual(runtime.has_active_execution(),state not in ('COMPLETED','CANCELLED','ABORTED','FAILED'))
    def test_NewOwnedBackendResetsOnceAndPreservesExistingFixture(self):
        formation = {'origin': {'latitude':39.9807,'longitude':116.34703,'altitude':30},
                     'spacing_m':20, 'direction':'east'}
        for enabled in (False, True):
            with self.subTest(enabled=enabled), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                (root/'fixture.json').write_text(json.dumps({'startup_formation':formation,'uavs':{}}))
                runtime = Runtime(root, {'http_endpoint':'http://127.0.0.1:19880',
                    'ws_endpoint':'ws://127.0.0.1:19881/ws', 'runtime_directory':'runtime',
                    'mock_fixture':'fixture.json','backend_executable':'fake.exe',
                    'mock_reset_on_start':enabled})
                data = runtime.directory/'data'; data.mkdir()
                original = '{"uavs":{"UAV-01":{"home_position":{"latitude":40}}}}'
                (data/'mock_execution.json').write_text(original)
                snapshot = {'backend':{'capabilities':['mock_startup_reset']},'plans':{}}
                requests = []
                def spawn(*args):
                    runtime.owned['Backend'] = Mock()
                    runtime.owned['Backend'].process.poll.return_value = None
                def post(request, **kwargs):
                    self.assertEqual(request.full_url, 'http://127.0.0.1:19880/api/mock-executions/reset')
                    body=json.loads(request.data);requests.append(body)
                    self.assertEqual(body['startup_formation'],formation)
                    return io.BytesIO(json.dumps({'reset_epoch':1,'state':{'mock_position_baseline':
                        {'request_id':body['request_id'],'epoch':1,'positions':{}}}}).encode())
                with patch('runtime.port_open',return_value=False), patch('runtime.compatible_snapshot',return_value=snapshot), \
                     patch.object(runtime,'_spawn',side_effect=spawn), patch('runtime.urllib.request.urlopen',side_effect=post):
                    runtime._backend()
                self.assertEqual(len(requests), int(enabled))
                self.assertEqual((data/'mock_execution.json').read_text(),original)
                with patch('runtime.port_open',return_value=True), patch('runtime.compatible_snapshot',return_value=snapshot), \
                     patch('runtime.urllib.request.urlopen') as post_existing:
                    runtime._backend()
                    post_existing.assert_not_called()
    def test_StartupResetLostReplyReusesIdAndRequiresAck(self):
        with tempfile.TemporaryDirectory() as folder:
            runtime=Runtime(folder, {'http_endpoint':'http://test','runtime_directory':'runtime'})
            runtime.snapshot={'backend':{'capabilities':['mock_startup_reset']}}
            calls=[]
            def response(request, **kwargs):
                body=json.loads(request.data);calls.append(body)
                if len(calls)==1:raise OSError('reply lost')
                return io.BytesIO(json.dumps({'reset_epoch':3,'state':{'mock_position_baseline':
                    {'request_id':body['request_id'],'epoch':3,'positions':{}}}}).encode())
            with patch('runtime.urllib.request.urlopen',side_effect=response), patch.object(runtime.stopping,'wait',return_value=False):
                runtime.reset_mock_for_startup({'startup_formation':{}})
            self.assertEqual(calls[0],calls[1])
            with patch('runtime.urllib.request.urlopen',return_value=io.BytesIO(b'{}')):
                with self.assertRaisesRegex(RuntimeError,'acknowledge'):
                    runtime.reset_mock_for_startup({'startup_formation':{}})
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
