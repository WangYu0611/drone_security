"""LAN lifecycle contract tests; no sockets, aircraft or UE processes are opened."""
import copy
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

from runtime import Runtime, ROLES, REQUIRED_CAPABILITIES


class StubOwnedProcess:
    def __init__(self, role, args, cwd, log_dir):
        self.role, self.args, self.cwd = role, args, Path(cwd)
        self.exit_code = None
        self.process = Mock()
        self.process.poll.side_effect = lambda: self.exit_code
        self.stop = Mock(side_effect=self._stop)

    def _stop(self):
        self.exit_code = 0

    def record(self):
        return {'role': self.role, 'pid': 100, 'start_time': 0,
                'executable': self.args[0], 'owned': True, 'exit_code': self.exit_code}


class DeploymentRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        (self.root / 'Backend').mkdir()
        (self.root / 'Backend/config.yaml').write_text('server: {}', encoding='utf-8')
        self.hosts = {'Backend': '192.168.20.10', 'Command': '192.168.20.11',
                      'Map': '192.168.20.12', 'Video': '192.168.20.13'}
        self.snapshot = {
            'backend': {'capabilities': sorted(REQUIRED_CAPABILITIES | {'telemetry_receive_time'})},
            'context': {'context_version': 1, 'active_uav_id': None}, 'clients': [],
            'preferences': {'language': 'en'}, 'video': {'video_view_version': 1},
            'plans': {'plans': {}, 'executions': {}, 'execution_adapter': 'Real'},
        }
        self.created = []

        def spawn(*args):
            process = StubOwnedProcess(*args)
            self.created.append(process)
            return process

        self.process_factory = self.patch('runtime.OwnedProcess', side_effect=spawn)
        self.port = self.patch('runtime.port_open', return_value=True)
        self.read = self.patch('runtime.compatible_snapshot', side_effect=lambda _: copy.deepcopy(self.snapshot))
        self.validate = self.patch('runtime.validate_local_role')
        self.patch('runtime.api', side_effect=AssertionError('Unexpected live API access'))
        self.patch('runtime.urllib.request.urlopen', side_effect=AssertionError('Unexpected live HTTP access'))
        self.patch('runtime.socket.create_connection', side_effect=AssertionError('Unexpected live socket access'))
        self.patch('runtime.subprocess.Popen', side_effect=AssertionError('Unexpected real process spawn'))
        self.origin = self.patch_object(Runtime, '_resolve_realtime_origin')
        self.pause = self.patch_object(Runtime, 'pause_for_shutdown')
        self.sequence = 0

    def patch(self, target, **kwargs):
        patcher = patch(target, **kwargs)
        self.addCleanup(patcher.stop)
        return patcher.start()

    def patch_object(self, target, name, **kwargs):
        patcher = patch.object(target, name, **kwargs)
        self.addCleanup(patcher.stop)
        return patcher.start()

    def runtime(self, role, **overrides):
        self.sequence += 1
        config = {
            'deployment_role': role, 'deployment_hosts': self.hosts,
            'deployment_http_port': 28080, 'deployment_ws_port': 28081,
            'http_endpoint': 'http://192.168.20.10:28080',
            'ws_endpoint': 'ws://192.168.20.10:28081/ws',
            'launch_mode': 'realtime', 'runtime_directory': f'Saved/LAN-{self.sequence}-{role}',
            'backend_config': 'Backend/config.yaml', 'backend_executable': 'DroneBackend.exe',
            'ue_executable': 'UnrealEditor.exe', 'project': 'UE5DroneControl.uproject',
            'map': '/Game/Level/CesiumWorld', 'max_fps': 20, 'startup_timeout': 30,
            'mock_reset_on_start': False,
        }
        config.update(overrides)
        return Runtime(self.root, config)

    def all_runtime(self, **overrides):
        return self.runtime('All', **{
            'deployment_hosts': {role: '127.0.0.1' for role in ('Backend',) + ROLES},
            'http_endpoint': 'http://127.0.0.1:28080',
            'ws_endpoint': 'ws://127.0.0.1:28081/ws',
            **overrides,
        })

    @staticmethod
    def client(role, instance='remote-instance', **overrides):
        record = {'client_role': role, 'instance_id': instance, 'state': 'ONLINE',
                  'hydrated': True, 'last_seen': time.time()}
        record.update(overrides)
        return record

    def test_each_workstation_manages_exactly_its_selected_role(self):
        for role in ('Backend',) + ROLES:
            with self.subTest(role=role):
                runtime = self.runtime(role)
                self.assertEqual(tuple(runtime.local_roles), (role,))
                self.assertEqual(tuple(runtime.local_client_roles), () if role == 'Backend' else (role,))
                for candidate in ('Backend',) + ROLES:
                    self.assertEqual(runtime.manages_role(candidate), candidate == role)
                self.assertFalse(runtime.manages_role('unknown'))

    def test_clients_launch_only_local_role_and_only_map_resolves_location(self):
        for role in ROLES:
            with self.subTest(role=role):
                self.created.clear()
                self.origin.reset_mock()
                runtime = self.runtime(role)
                runtime.start()
                self.assertEqual(set(runtime.owned), {role})
                self.assertEqual([process.role for process in self.created], [role])
                self.assertEqual(self.origin.call_count, int(role == 'Map'))
                self.assertTrue(runtime.existing_backend)
                args = runtime.owned[role].args
                self.assertIn('-ClientRole=' + role, args)
                self.assertIn('-P1Http=' + runtime.config['http_endpoint'], args)
                self.assertIn('-P1Ws=' + runtime.config['ws_endpoint'], args)
                self.validate.assert_called_with(runtime.config)

    def test_backend_workstation_starts_only_backend_with_shared_custom_ports(self):
        self.port.side_effect = lambda endpoint: bool(self.created)
        runtime = self.runtime('Backend')
        runtime.start()
        self.assertEqual(set(runtime.owned), {'Backend'})
        self.assertEqual([process.role for process in self.created], ['Backend'])
        process = runtime.owned['Backend']
        self.assertEqual(process.args, [str(self.root / 'DroneBackend.exe'),
                                      str(self.root / 'Backend/config.yaml'),
                                      '--http-port', '28080', '--ws-port', '28081'])
        self.assertEqual(process.cwd, self.root)
        self.assertFalse(runtime.existing_backend)
        self.origin.assert_not_called()
        self.assertFalse((runtime.directory / 'data').exists())

    def test_shared_all_launches_backend_and_three_clients_with_same_loopback_endpoints(self):
        self.port.side_effect = lambda endpoint: bool(self.created)
        runtime = self.all_runtime()
        runtime.start()
        self.assertEqual(runtime.local_roles, ('Backend',) + ROLES)
        self.assertEqual(runtime.local_client_roles, ROLES)
        self.assertEqual([process.role for process in self.created], ['Backend', *ROLES])
        self.assertEqual(set(runtime.owned), {'Backend', *ROLES})
        self.assertTrue(all(runtime.manages_role(role) for role in ('Backend',) + ROLES))
        self.assertFalse(runtime.manages_role('unknown'))
        self.validate.assert_called_once_with(runtime.config)
        self.origin.assert_called_once()
        self.assertEqual(runtime.owned['Backend'].args[-4:],
                         ['--http-port', '28080', '--ws-port', '28081'])
        for role in ROLES:
            args = runtime.owned[role].args
            self.assertIn('-ClientRole=' + role, args)
            self.assertIn('-P1Http=http://127.0.0.1:28080', args)
            self.assertIn('-P1Ws=ws://127.0.0.1:28081/ws', args)
        runtime.stop()
        for process in self.created:
            process.stop.assert_called_once()
        self.assertEqual(runtime.state, 'STOPPED')
        self.assertEqual(runtime.owned, {})

    def test_shared_all_demo_generates_backend_config_without_requesting_real_location(self):
        self.port.side_effect = lambda endpoint: bool(self.created)
        self.snapshot['plans'].update(execution_adapter='Mock', mock_execution={'uavs': ['d1']})
        (self.root / 'mock.json').write_text('{}', encoding='utf-8')
        runtime = self.all_runtime(launch_mode='demo', mock_fixture='mock.json')
        runtime.start()
        self.assertEqual(set(runtime.owned), {'Backend', *ROLES})
        self.origin.assert_not_called()
        generated = (runtime.directory / 'backend.yaml').read_text(encoding='utf-8')
        self.assertIn('http_port: 28080', generated)
        self.assertIn('ws_port: 28081', generated)
        for role in ROLES:
            self.assertIn('-MapStartupMode=Demo', runtime.owned[role].args)

    def test_shared_all_requires_every_owned_component_for_ready(self):
        self.port.side_effect = lambda endpoint: bool(self.created)
        runtime = self.all_runtime()
        runtime.start()
        runtime.deadline = 0
        self.snapshot['clients'] = [self.client(role, runtime.instances[role]) for role in ROLES]
        runtime.poll()
        self.assertEqual(runtime.state, 'READY')
        for role in ROLES:
            with self.subTest(role=role):
                local = next(client for client in self.snapshot['clients']
                             if client['instance_id'] == runtime.instances[role])
                local['hydrated'] = False
                # Another instance of the same role cannot satisfy local readiness.
                self.snapshot['clients'].append(self.client(role, 'unowned-' + role))
                runtime.poll()
                self.assertEqual(runtime.state, 'DEGRADED')
                self.assertEqual(runtime.components[role], 'OFFLINE')
                self.snapshot['clients'].pop()
                local['hydrated'] = True
                runtime.poll()
                self.assertEqual(runtime.state, 'READY')
        runtime.owned['Backend'].exit_code = 1
        runtime.poll()
        self.assertEqual(runtime.state, 'DEGRADED')
        self.assertEqual(runtime.components['Backend'], 'OFFLINE')

    def test_shared_all_failure_reclaims_every_new_local_process_but_not_external_backend(self):
        for own_backend in (True, False):
            with self.subTest(own_backend=own_backend):
                self.created.clear()
                self.port.side_effect = lambda endpoint: bool(self.created) or not own_backend
                runtime = self.all_runtime()
                original_start_role = runtime.start_role

                def fail_after_map(role):
                    original_start_role(role)
                    if role == 'Map':
                        raise RuntimeError('Map startup rejected')

                with patch.object(runtime, 'start_role', side_effect=fail_after_map):
                    with self.assertRaisesRegex(RuntimeError, 'Map startup rejected'):
                        runtime.start()
                self.assertEqual([process.role for process in self.created],
                                 (['Backend'] if own_backend else []) + ['Command', 'Map'])
                for process in self.created:
                    process.stop.assert_called_once()
                self.assertEqual(runtime.state, 'FAILED')
                self.assertEqual(runtime.owned, {})
        self.pause.assert_not_called()

    def test_clients_never_start_backend_when_remote_host_is_down(self):
        self.port.return_value = False
        for role in ROLES:
            with self.subTest(role=role):
                runtime = self.runtime(role)
                with self.assertRaisesRegex(RuntimeError, 'Backend'):
                    runtime.start()
                self.assertEqual(runtime.state, 'FAILED')
                self.assertEqual(runtime.owned, {})
        self.process_factory.assert_not_called()
        self.origin.assert_not_called()
        self.pause.assert_not_called()

    def test_http_without_websocket_rejects_all_roles_before_spawning(self):
        self.port.side_effect = lambda endpoint: endpoint.startswith('http://')
        for role in ('All', 'Backend') + ROLES:
            with self.subTest(role=role):
                runtime = self.runtime(role)
                with self.assertRaisesRegex(RuntimeError, 'WebSocket'):
                    runtime.start()
                self.assertEqual(runtime.state, 'FAILED')
                self.assertEqual(runtime.owned, {})
        self.process_factory.assert_not_called()
        self.origin.assert_not_called()

    def test_unrecognized_local_address_rejects_before_network_or_process_work(self):
        self.validate.side_effect = ValueError('Configured role IP is not a local address')
        for role in ('All', 'Backend') + ROLES:
            with self.subTest(role=role):
                runtime = self.runtime(role)
                with self.assertRaisesRegex(ValueError, 'not a local address'):
                    runtime.start()
                self.assertEqual(runtime.owned, {})
        self.port.assert_not_called()
        self.read.assert_not_called()
        self.process_factory.assert_not_called()

    def test_other_online_roles_do_not_block_start_but_duplicate_local_role_does(self):
        for role in ROLES:
            with self.subTest(role=role):
                self.snapshot['clients'] = [self.client(other) for other in ROLES if other != role]
                runtime = self.runtime(role)
                runtime.start()
                self.assertEqual(set(runtime.owned), {role})
                before = len(self.created)
                self.snapshot['clients'].append(self.client(role))
                duplicate = self.runtime(role)
                with self.assertRaisesRegex(RuntimeError, 'already ONLINE'):
                    duplicate.start()
                self.assertEqual(len(self.created), before)
                self.assertIsNone(runtime.owned[role].process.poll())
                self.assertEqual(duplicate.owned, {})

    def test_ready_depends_only_on_backend_and_local_role(self):
        for role in ('Backend',) + ROLES:
            with self.subTest(role=role):
                self.snapshot['clients'] = []
                runtime = self.runtime(role)
                runtime.start()
                if role != 'Backend':
                    self.snapshot['clients'] = [self.client(role, runtime.instances[role])]
                runtime.poll()
                self.assertEqual(runtime.state, 'READY')
                self.assertEqual(runtime.components['Backend'], 'ONLINE')
                for remote in ROLES:
                    self.assertEqual(runtime.components[remote], 'ONLINE' if remote == role else 'OFFLINE')

    def test_remote_presence_is_visible_but_remote_expiry_does_not_degrade_local_ready(self):
        runtime = self.runtime('Command')
        runtime.start()
        local = self.client('Command', runtime.instances['Command'])
        self.snapshot['clients'] = [local, self.client('Map'), self.client('Video')]
        runtime.poll()
        self.assertEqual(runtime.state, 'READY')
        self.assertEqual(runtime.components['Map'], 'ONLINE')
        self.assertEqual(runtime.components['Video'], 'ONLINE')
        self.snapshot['clients'][1]['last_seen'] = time.time() - 20
        self.snapshot['clients'][2]['hydrated'] = False
        runtime.poll()
        self.assertEqual(runtime.state, 'READY')
        self.assertEqual(runtime.components['Map'], 'OFFLINE')
        self.assertEqual(runtime.components['Video'], 'OFFLINE')

    def test_unhydrated_local_client_cannot_be_replaced_by_remote_same_role_presence(self):
        runtime = self.runtime('Command')
        runtime.start()
        runtime.deadline = 0
        self.snapshot['clients'] = [self.client('Command', runtime.instances['Command'], hydrated=False),
                                    self.client('Command', 'another-instance')]
        runtime.poll()
        self.assertEqual(runtime.state, 'DEGRADED')
        self.assertEqual(runtime.components['Command'], 'OFFLINE')

    def test_remote_restart_and_start_role_are_rejected_without_affecting_local_process(self):
        runtime = self.runtime('Command')
        runtime.start()
        local = runtime.owned['Command']
        for role in ('Backend', 'Map', 'Video'):
            with self.subTest(role=role), self.assertRaises(RuntimeError):
                runtime.restart(role)
        for role in ('Map', 'Video'):
            with self.subTest(role=role), self.assertRaises(RuntimeError):
                runtime.start_role(role)
        local.stop.assert_not_called()
        self.assertEqual(set(runtime.owned), {'Command'})

    def test_client_shutdown_stops_only_owned_local_process_and_never_pauses_remote(self):
        runtime = self.runtime('Video')
        runtime.start()
        local = runtime.owned['Video']
        self.snapshot['clients'] = [self.client(role) for role in ROLES]
        self.snapshot['plans']['executions'] = {'live': {'state': 'EXECUTING'}}
        runtime.stop()
        local.stop.assert_called_once()
        self.pause.assert_not_called()
        self.assertEqual(runtime.owned, {})
        self.assertEqual(runtime.state, 'STOPPED')
        self.assertEqual(self.snapshot['plans']['executions']['live']['state'], 'EXECUTING')

    def test_failure_after_local_spawn_reclaims_only_local_process(self):
        runtime = self.runtime('Command')
        original = runtime.start_role

        def spawn_then_fail(role):
            original(role)
            raise RuntimeError('Local startup validation failed')

        with patch.object(runtime, 'start_role', side_effect=spawn_then_fail):
            with self.assertRaisesRegex(RuntimeError, 'Local startup validation failed'):
                runtime.start()
        self.assertEqual([process.role for process in self.created], ['Command'])
        self.created[0].stop.assert_called_once()
        self.assertEqual(runtime.owned, {})
        self.assertEqual(runtime.state, 'FAILED')
        self.pause.assert_not_called()

    def test_backend_start_failure_reclaims_backend_without_starting_clients(self):
        self.port.return_value = False
        runtime = self.runtime('Backend')
        with patch.object(runtime, '_wait_backend_ready', side_effect=RuntimeError('Backend failed readiness')):
            with self.assertRaisesRegex(RuntimeError, 'Backend failed readiness'):
                runtime.start()
        self.assertEqual([process.role for process in self.created], ['Backend'])
        self.created[0].stop.assert_called_once()
        self.assertEqual(runtime.owned, {})
        self.assertEqual(runtime.state, 'FAILED')
        self.origin.assert_not_called()

    def test_existing_backend_is_not_stopped_or_restarted_by_backend_launcher(self):
        runtime = self.runtime('Backend')
        runtime.start()
        self.assertTrue(runtime.existing_backend)
        self.assertEqual(runtime.owned, {})
        with self.assertRaisesRegex(RuntimeError, 'not owned'):
            runtime.restart('Backend')
        runtime.stop()
        self.process_factory.assert_not_called()
        self.pause.assert_not_called()

    def test_exited_owned_backend_is_not_ready_even_if_another_service_answers_http(self):
        self.port.side_effect = lambda endpoint: bool(self.created)
        runtime = self.runtime('Backend')
        runtime.start()
        runtime.poll()
        self.assertEqual(runtime.state, 'READY')
        runtime.owned['Backend'].exit_code = 1
        self.port.return_value = True
        runtime.poll()
        self.assertEqual(runtime.state, 'DEGRADED')
        self.assertEqual(runtime.components['Backend'], 'OFFLINE')

    def test_websocket_loss_degrades_ready_lan_workstation_even_when_http_stays_healthy(self):
        runtime = self.runtime('Command')
        runtime.start()
        self.snapshot['clients'] = [self.client('Command', runtime.instances['Command'])]
        runtime.poll()
        self.assertEqual(runtime.state, 'READY')
        self.port.return_value = False
        runtime.poll()
        self.assertEqual(runtime.state, 'DEGRADED')
        self.assertEqual(runtime.components['Backend'], 'OFFLINE')

    def test_shared_mode_cannot_be_changed_by_local_start_argument(self):
        runtime = self.runtime('Command')
        with self.assertRaisesRegex(RuntimeError, 'shared configuration'):
            runtime.start('demo')
        self.assertEqual(runtime.launch_mode, 'realtime')
        self.port.assert_not_called()
        self.process_factory.assert_not_called()


if __name__ == '__main__':
    unittest.main(verbosity=2)
