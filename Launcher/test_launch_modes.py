"""Mode isolation and real-location startup checks; no aircraft or UE processes."""
import copy
import itertools
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from launch_modes import (check_backend_mode, geoid_separation, mode_config,
                          received_origin, valid_origin)
from runtime import ROLES, Runtime, role_arguments


SINGAPORE = {'latitude': 1.279445, 'longitude': 103.862188, 'altitude': 15.0}
NOW = 1000.0


def real_drone(slot=1, **changes):
    result = {'slot': slot, 'simulation': False, 'telemetry_source': 'received',
              'status': 'online', 'gps_fix': True, 'telemetry_received_at': NOW - 1,
              'gps_lat': 31.2304, 'gps_lon': 121.4737, 'gps_alt': 72.5}
    result.update(changes)
    return result


def real_snapshot():
    return {'backend': {'capabilities': ['telemetry_receive_time']},
            'context': {'active_uav_id': None}, 'clients': [],
            'plans': {'plans': {}, 'mock_execution': {'uavs': {}}},
            'preferences': {'language': 'en'}}


class ModeFixture(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix='drone mode tests ')
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        (self.root / 'Backend').mkdir()
        fixture = Path(__file__).resolve().parents[1] / 'Backend/mock_execution.json'
        (self.root / 'Backend/mock_execution.json').write_bytes(fixture.read_bytes())
        self.backend_config = self.root / 'Backend/operator.yaml'
        self.backend_config.write_text('server:\n  http_port: 8080\noperator_policy: preserve\n',
                                       encoding='utf-8')
        self.base = {'backend_executable': 'Backend/fake.exe', 'ue_executable': 'fake-editor.exe',
                     'project': 'UE5DroneControl.uproject', 'map': '/Game/Level/CesiumWorld',
                     'http_endpoint': 'http://127.0.0.1:19880',
                     'ws_endpoint': 'ws://127.0.0.1:19881/ws',
                     'runtime_directory': 'Saved/Stage1',
                     'mock_fixture': 'historical-qa-fixture.json', 'mock_reset_on_start': True,
                     'realtime_backend_config': 'Backend/operator.yaml',
                     'realtime_gps_timeout': 1, 'startup_timeout': 30, 'max_fps': 20,
                     'nested': {'operator': ['preserve']}}

    def realtime_runtime(self):
        runtime = Runtime(self.root, mode_config(self.root, self.base, 'realtime'))
        runtime.launch_mode = 'realtime'
        return runtime


class ModeConfigTests(ModeFixture):
    def test_demo_uses_shipped_singapore_not_historical_fixture(self):
        before = copy.deepcopy(self.base)
        fixture_before = (self.root / 'Backend/mock_execution.json').read_bytes()
        result = mode_config(self.root, self.base, 'demo')
        self.assertEqual(result['startup_origin'], SINGAPORE)
        self.assertEqual(result['mock_fixture'], 'Backend/mock_execution.json')
        self.assertEqual(result['launch_mode'], 'demo')
        self.assertEqual(result['http_endpoint'], self.base['http_endpoint'])
        result['nested']['operator'].append('result-only')
        self.assertEqual(self.base, before)
        self.assertEqual((self.root / 'Backend/mock_execution.json').read_bytes(), fixture_before)

    def test_realtime_separates_endpoints_data_and_clears_demo_origin(self):
        self.base['startup_origin'] = dict(SINGAPORE)
        before = copy.deepcopy(self.base)
        result = mode_config(self.root, self.base, 'realtime')
        self.assertEqual(result['http_endpoint'], 'http://127.0.0.1:8080')
        self.assertEqual(result['ws_endpoint'], 'ws://127.0.0.1:8081/ws')
        self.assertEqual(result['runtime_directory'], 'Saved/Realtime')
        self.assertEqual(result['backend_config'], 'Backend/operator.yaml')
        self.assertFalse(result['mock_reset_on_start'])
        self.assertNotIn('startup_origin', result)
        result['nested']['operator'].clear()
        self.assertEqual(self.base, before)

    def test_unknown_modes_and_invalid_demo_origin_are_rejected(self):
        for mode in (None, '', 'live', 'DEMO'):
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                mode_config(self.root, self.base, mode)
        fixture = self.root / 'Backend/mock_execution.json'
        fixture.write_text(json.dumps({'startup_formation': {'origin': {
            'longitude': 181, 'latitude': 1, 'altitude': 15}}}), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'DEMO'):
            mode_config(self.root, self.base, 'demo')

    def test_realtime_rejects_reused_directory_or_either_endpoint(self):
        for key, value in [('realtime_runtime_directory', 'Saved/Stage1'),
                           ('realtime_http_endpoint', self.base['http_endpoint']),
                           ('realtime_ws_endpoint', self.base['ws_endpoint'])]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                mode_config(self.root, {**self.base, key: value}, 'realtime')

    def test_realtime_rejects_absolute_alias_of_demo_runtime_directory(self):
        config = {**self.base, 'realtime_runtime_directory': str(self.root / 'Saved/Stage1')}
        with self.assertRaisesRegex(ValueError, 'directories'):
            mode_config(self.root, config, 'realtime')

    def test_realtime_rejects_localhost_alias_of_demo_http_port(self):
        config = {**self.base, 'realtime_http_endpoint': 'http://localhost:19880'}
        with self.assertRaisesRegex(ValueError, 'ports'):
            mode_config(self.root, config, 'realtime')

    def test_each_role_receives_the_same_selected_origin_and_mode(self):
        for mode in ('demo', 'realtime'):
            config = mode_config(self.root, self.base, mode)
            if mode == 'realtime':
                config['startup_origin'] = {'longitude': 121.4737, 'latitude': 31.2304,
                                            'altitude': 104.0, 'uav_id': 'UAV-02'}
            for role in ROLES:
                with self.subTest(mode=mode, role=role):
                    args = role_arguments(config, self.root, role, 'mode-test', self.root / 'role.log')
                    self.assertIn('-MapStartupMode=' + mode.title(), args)
                    for field, argument in [('longitude', 'Longitude'), ('latitude', 'Latitude'),
                                            ('altitude', 'Height')]:
                        self.assertIn('-MapStartup' + argument + '=' + str(config['startup_origin'][field]), args)
                    self.assertIn('-ClientRole=' + role, args)


class ReceivedLocationTests(unittest.TestCase):
    def origin(self, drones, context=None, **settings):
        return received_origin(drones, context or {}, now=NOW, **settings)

    def test_only_received_online_real_aircraft_with_an_explicit_fix_qualify(self):
        self.assertEqual(self.origin([real_drone()])['uav_id'], 'UAV-01')
        for changes in ({'simulation': True}, {'simulation': None}, {'simulation': 0},
                        {'telemetry_source': 'mock_execution'}, {'telemetry_source': ''},
                        {'status': 'offline'}, {'status': 'ONLINE'},
                        {'gps_fix': False}, {'gps_fix': None}, {'gps_fix': 1}):
            with self.subTest(changes=changes):
                self.assertIsNone(self.origin([real_drone(**changes)]))

    def test_timestamp_is_packet_time_fresh_and_never_manufactured_by_polling(self):
        for timestamp in (None, NOW - 5.001, NOW + .001, float('nan'), float('inf'), True, '999'):
            drone = real_drone(telemetry_received_at=timestamp)
            with self.subTest(timestamp=timestamp):
                self.assertIsNone(self.origin([drone]))
                self.assertIs(drone['telemetry_received_at'], timestamp)
        missing = real_drone()
        del missing['telemetry_received_at']
        for _ in range(3):
            self.assertIsNone(self.origin([missing]))
        self.assertNotIn('telemetry_received_at', missing)
        for timestamp in (NOW, NOW - 5):
            self.assertIsNotNone(self.origin([real_drone(telemetry_received_at=timestamp)]))

    def test_invalid_coordinates_slots_and_zero_fix_do_not_fall_back(self):
        for changes in ({'gps_lat': 86}, {'gps_lat': -86}, {'gps_lon': 180.1},
                        {'gps_lon': -180.1}, {'gps_alt': -1001}, {'gps_alt': 100001},
                        {'gps_lat': float('nan')}, {'gps_lon': float('inf')},
                        {'gps_alt': '72.5'}, {'gps_lat': True}, {'gps_lat': 0, 'gps_lon': 0},
                        {'slot': 0}, {'slot': -1}, {'slot': True}, {'slot': '1'}):
            with self.subTest(changes=changes):
                self.assertIsNone(self.origin([real_drone(**changes)]))
        self.assertIsNone(self.origin([None, {}, 'invalid']))
        self.assertIsNone(self.origin([real_drone(simulation=True)], {'active_uav_id': 'UAV-01'}))

    def test_selected_valid_aircraft_wins_then_lowest_valid_slot(self):
        drones = [real_drone(4), real_drone(2), real_drone(1, status='offline')]
        before = copy.deepcopy(drones)
        self.assertEqual(self.origin(drones, {'active_uav_id': 'UAV-04'})['uav_id'], 'UAV-04')
        self.assertEqual(self.origin(drones, {'active_uav_id': 'UAV-01'})['uav_id'], 'UAV-02')
        self.assertEqual(self.origin(list(reversed(drones)))['uav_id'], 'UAV-02')
        self.assertEqual(drones, before)

    def test_amsl_is_converted_with_geoid_before_range_validation(self):
        origin = self.origin([real_drone()], geoid=31.5)
        self.assertAlmostEqual(origin['altitude'], 104.0)
        self.assertAlmostEqual(origin['longitude'], 121.4737)
        self.assertAlmostEqual(origin['latitude'], 31.2304)
        self.assertIsNone(self.origin([real_drone(gps_alt=99990)], geoid=20))
        for setting in ({'max_age': 0}, {'max_age': -1}, {'max_age': True},
                        {'max_age': float('inf')}, {'geoid': float('nan')}):
            with self.subTest(setting=setting), self.assertRaises(ValueError):
                self.origin([real_drone()], **setting)

    def test_origin_validation_rejects_non_numeric_and_unusable_projection_ranges(self):
        for origin in (None, {}, {'longitude': True, 'latitude': 1, 'altitude': 2},
                       {'longitude': 1, 'latitude': 90, 'altitude': 2},
                       {'longitude': 1, 'latitude': 1, 'altitude': float('inf')}):
            with self.subTest(origin=origin):
                self.assertFalse(valid_origin(origin))
        self.assertTrue(valid_origin(SINGAPORE))

    def test_geoid_uses_local_override_and_rejects_invalid_height(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.assertEqual(geoid_separation(root), 0)
            default = root / 'Config/DefaultGame.ini'
            default.parent.mkdir()
            default.write_text('[/Script/UE5DroneControl.DroneNetworkManager]\n'
                               'GeoidSeparationMeters=10.5\n', encoding='utf-8')
            self.assertEqual(geoid_separation(root), 10.5)
            local = root / 'Saved/Config/WindowsEditor/Game.ini'
            local.parent.mkdir(parents=True)
            local.write_text('[/Script/UE5DroneControl.DroneNetworkManager]\n'
                             'GeoidSeparationMeters=-4.25\n', encoding='utf-8')
            self.assertEqual(geoid_separation(root), -4.25)
            local.write_text('[/Script/UE5DroneControl.DroneNetworkManager]\n'
                             'GeoidSeparationMeters=nan\n', encoding='utf-8')
            with self.assertRaises(ValueError):
                geoid_separation(root)


class BackendModeTests(unittest.TestCase):
    def test_realtime_rejects_legacy_capabilities_and_both_mock_position_sources(self):
        check_backend_mode(real_snapshot(), 'realtime')
        old = real_snapshot()
        old['backend']['capabilities'] = ['fleet_move_3d', 'map_fleet_controls', 'plan_configuration']
        with self.assertRaisesRegex(RuntimeError, 'SetupDroneSecurity.cmd'):
            check_backend_mode(old, 'realtime')
        for field in ('mock_execution', 'mock_position_baseline'):
            snapshot = real_snapshot()
            snapshot['plans'][field] = ({'uavs': {'UAV-01': {}}} if field == 'mock_execution'
                                         else {'positions': {'UAV-01': dict(SINGAPORE)}})
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, 'Mock Backend'):
                check_backend_mode(snapshot, 'realtime')

    def test_demo_rejects_real_or_empty_mock_authority(self):
        with self.assertRaisesRegex(RuntimeError, 'Mock Backend'):
            check_backend_mode(real_snapshot(), 'demo')
        snapshot = real_snapshot()
        snapshot['plans']['mock_execution']['uavs'] = {'UAV-01': {}}
        check_backend_mode(snapshot, 'demo')
        snapshot['plans']['execution_adapter'] = 'Real'
        with self.assertRaisesRegex(RuntimeError, 'Mock Backend'):
            check_backend_mode(snapshot, 'demo')

    def test_real_adapter_can_keep_legacy_named_aircraft_and_position_configuration(self):
        snapshot = real_snapshot()
        snapshot['plans'].update({
            'execution_adapter': 'Real',
            'mock_execution': {'uavs': {'UAV-01': {'home_position': {
                'latitude': 31.2304, 'longitude': 121.4737, 'altitude': 104.0}}}},
            'mock_position_baseline': {'positions': {'UAV-01': dict(SINGAPORE)}}})
        before = copy.deepcopy(snapshot)
        check_backend_mode(snapshot, 'realtime')
        self.assertEqual(snapshot, before)
        snapshot['backend']['capabilities'] = []
        with self.assertRaisesRegex(RuntimeError, 'SetupDroneSecurity.cmd'):
            check_backend_mode(snapshot, 'realtime')


class RealtimeRuntimeTests(ModeFixture):
    @staticmethod
    def owned(role):
        process = Mock()
        process.process.poll.return_value = None
        process.record.return_value = {'role': role, 'pid': 123, 'owned': True}
        return process

    def test_new_realtime_backend_preserves_operator_config_and_never_seeds_or_resets(self):
        runtime = self.realtime_runtime()
        before = self.backend_config.read_bytes()
        fixture_before = (self.root / 'Backend/mock_execution.json').read_bytes()
        def ready(_):
            runtime.snapshot = real_snapshot()
        with patch('runtime.port_open', return_value=False), \
             patch.object(runtime, '_spawn') as spawn, \
             patch.object(runtime, '_wait_backend_ready', side_effect=ready), \
             patch.object(runtime, 'reset_mock_for_startup') as reset:
            runtime._backend()
        spawn.assert_called_once_with('Backend', [str(self.root / 'Backend/fake.exe'),
                                                  str(self.backend_config)], self.root)
        reset.assert_not_called()
        self.assertEqual(self.backend_config.read_bytes(), before)
        self.assertEqual((self.root / 'Backend/mock_execution.json').read_bytes(), fixture_before)
        self.assertFalse((runtime.directory / 'data').exists())
        self.assertFalse((runtime.directory / 'backend.yaml').exists())
        self.assertFalse(runtime.existing_backend)

    def test_missing_realtime_backend_config_cannot_fall_through_to_mock_seed(self):
        runtime = self.realtime_runtime()
        self.backend_config.unlink()
        with patch('runtime.port_open', return_value=False), patch.object(runtime, '_spawn') as spawn:
            with self.assertRaisesRegex(RuntimeError, 'config missing'):
                runtime._backend()
        spawn.assert_not_called()
        self.assertFalse((runtime.directory / 'data').exists())

    def test_realtime_waits_for_received_fix_before_launching_all_roles(self):
        runtime = Runtime(self.root, self.base)
        snapshot = real_snapshot()
        events = []
        polls = iter([[], [real_drone(2)]])
        def drones(base, path):
            self.assertEqual(base, 'http://127.0.0.1:8080')
            self.assertEqual(path, '/api/drones')
            self.assertFalse(any(event.startswith('spawn:') for event in events))
            self.assertEqual(runtime.location_status, 'waiting')
            value = next(polls)
            events.append('received-fix' if value else 'no-fix')
            return value
        def spawn(role, args, cwd):
            self.assertEqual(runtime.location_status, 'ready')
            self.assertIn('-MapStartupMode=Realtime', args)
            self.assertIn('-MapStartupLongitude=121.4737', args)
            self.assertIn('-MapStartupLatitude=31.2304', args)
            self.assertIn('-MapStartupHeight=104.0', args)
            self.assertEqual(cwd, self.root)
            events.append('spawn:' + role)
        with patch('runtime.port_open', return_value=True), \
             patch('runtime.compatible_snapshot', return_value=snapshot), \
             patch('runtime.api', side_effect=drones), \
             patch('launch_modes.time.time', return_value=NOW), \
             patch('runtime.time.monotonic', side_effect=itertools.count(0, .2)), \
             patch('runtime.geoid_separation', return_value=31.5), \
             patch('runtime.computer_origin') as computer, \
             patch.object(runtime.stopping, 'wait', return_value=False), \
             patch.object(runtime, '_spawn', side_effect=spawn), \
             patch.object(runtime, 'reset_mock_for_startup') as reset, \
             patch.object(runtime, 'pause_for_shutdown') as pause:
            runtime.start('realtime')
        self.assertEqual(events, ['no-fix', 'received-fix'] + ['spawn:' + role for role in ROLES])
        self.assertEqual(runtime.config['startup_origin']['uav_id'], 'UAV-02')
        self.assertEqual(runtime.location_status, 'ready')
        self.assertTrue(runtime.existing_backend)
        self.assertEqual(runtime.owned, {})
        self.assertFalse((runtime.directory / 'data').exists())
        reset.assert_not_called()
        pause.assert_not_called()
        computer.assert_not_called()

    def test_no_real_fix_does_not_launch_clients_reset_or_stop_external_backend(self):
        runtime = Runtime(self.root, self.base)
        with patch('runtime.port_open', return_value=True), \
             patch('runtime.compatible_snapshot', return_value=real_snapshot()), \
             patch('runtime.api', return_value=[real_drone(simulation=True)]), \
             patch('launch_modes.time.time', return_value=NOW), \
             patch('runtime.time.monotonic', side_effect=itertools.count(0, .2)), \
             patch.object(runtime.stopping, 'wait', return_value=False), \
             patch.object(runtime, '_spawn') as spawn, \
             patch.object(runtime, 'reset_mock_for_startup') as reset, \
             patch.object(runtime, 'pause_for_shutdown') as pause, \
             patch('runtime.computer_origin') as computer, \
             patch('runtime.OwnedProcess') as process, \
             patch('runtime.urllib.request.urlopen') as request:
            with self.assertRaisesRegex(RuntimeError, 'No fresh real GPS'):
                runtime.start('realtime')
        self.assertEqual(runtime.state, 'FAILED')
        # Timeout has ended the wait; the stopped UI must not claim that a
        # background GPS acquisition is still running. The in-flight waiting
        # status is checked in test_realtime_waits_for_received_fix_before_launching_all_roles.
        self.assertEqual(runtime.location_status, '')
        self.assertNotIn('startup_origin', runtime.config)
        self.assertEqual(runtime.owned, {})
        self.assertTrue(runtime.existing_backend)
        self.assertEqual(json.loads((runtime.directory / 'ownership.json').read_text()), [])
        spawn.assert_not_called()
        process.assert_not_called()
        request.assert_not_called()
        reset.assert_not_called()
        pause.assert_not_called()
        computer.assert_not_called()

    def test_no_connected_aircraft_uses_computer_location_after_rechecking_gps(self):
        runtime = self.realtime_runtime()
        runtime.config['realtime_device_wait'] = 0
        computer_fix = {'longitude': 2.3522, 'latitude': 48.8566, 'altitude': 0.0,
                        'source': 'computer', 'accuracy_m': 25, 'height_estimated': True}
        with patch('runtime.compatible_snapshot', return_value=real_snapshot()), \
             patch('runtime.api', side_effect=[[], []]) as read, \
             patch('runtime.time.monotonic', side_effect=itertools.count(0, .1)), \
             patch('runtime.geoid_separation', return_value=31.5), \
             patch('runtime.computer_origin', return_value=computer_fix) as computer:
            runtime._resolve_realtime_origin()
        computer.assert_called_once_with(timeout=15.0, geoid=31.5, cancelled=runtime.stopping)
        self.assertEqual(read.call_count, 2, 'GPS authority must be checked again after querying the computer')
        self.assertEqual(runtime.config['startup_origin'], computer_fix)
        self.assertEqual(runtime.location_source, 'computer')
        self.assertEqual(runtime.location_status, 'estimated')
        self.assertNotEqual(runtime.config['startup_origin']['longitude'], SINGAPORE['longitude'])

    def test_gps_arriving_during_computer_query_takes_priority_over_computer_result(self):
        runtime = self.realtime_runtime()
        runtime.config['realtime_device_wait'] = 0
        computer_fix = {'longitude': 2.3522, 'latitude': 48.8566, 'altitude': 0,
                        'source': 'computer', 'height_estimated': False}
        with patch('runtime.compatible_snapshot', return_value=real_snapshot()), \
             patch('runtime.api', side_effect=[[], [real_drone(3)]]), \
             patch('runtime.time.monotonic', side_effect=itertools.count(0, .1)), \
             patch('launch_modes.time.time', return_value=NOW), \
             patch('runtime.geoid_separation', return_value=0), \
             patch('runtime.computer_origin', return_value=computer_fix) as computer:
            runtime._resolve_realtime_origin()
        computer.assert_called_once()
        self.assertEqual(runtime.config['startup_origin']['uav_id'], 'UAV-03')
        self.assertAlmostEqual(runtime.config['startup_origin']['longitude'], 121.4737)
        self.assertEqual(runtime.location_source, 'uav')
        self.assertEqual(runtime.location_status, 'ready')

    def test_connected_real_aircraft_without_fix_never_uses_computer_fallback(self):
        config = {**self.base, 'realtime_device_wait': 0}
        runtime = Runtime(self.root, config)
        with patch('runtime.port_open', return_value=True), \
             patch('runtime.compatible_snapshot', return_value=real_snapshot()), \
             patch('runtime.api', return_value=[real_drone(gps_fix=False)]), \
             patch('runtime.time.monotonic', side_effect=itertools.count(0, .1)), \
             patch('launch_modes.time.time', return_value=NOW), \
             patch.object(runtime.stopping, 'wait', return_value=False), \
             patch('runtime.computer_origin') as computer, \
             patch.object(runtime, '_spawn') as spawn, \
             patch.object(runtime, 'pause_for_shutdown') as pause:
            with self.assertRaisesRegex(RuntimeError, 'No fresh real GPS'):
                runtime.start('realtime')
        computer.assert_not_called()
        spawn.assert_not_called()
        pause.assert_not_called()
        self.assertEqual(runtime.state, 'FAILED')
        self.assertNotIn('startup_origin', runtime.config)
        self.assertTrue(runtime.existing_backend)
        self.assertEqual(runtime.owned, {})

    def test_unavailable_computer_location_has_no_demo_fallback_or_external_backend_shutdown(self):
        config = {**self.base, 'realtime_device_wait': 0}
        runtime = Runtime(self.root, config)
        with patch('runtime.port_open', return_value=True), \
             patch('runtime.compatible_snapshot', return_value=real_snapshot()), \
             patch('runtime.api', return_value=[]), \
             patch('runtime.time.monotonic', side_effect=itertools.count(0, .1)), \
             patch('runtime.computer_origin', side_effect=RuntimeError('Windows location unavailable')) as computer, \
             patch.object(runtime, '_spawn') as spawn, \
             patch.object(runtime, 'reset_mock_for_startup') as reset, \
             patch.object(runtime, 'pause_for_shutdown') as pause, \
             patch('runtime.urllib.request.urlopen') as mutation:
            with self.assertRaisesRegex(RuntimeError, '^Windows location unavailable$'):
                runtime.start('realtime')
        computer.assert_called_once()
        spawn.assert_not_called()
        reset.assert_not_called()
        pause.assert_not_called()
        mutation.assert_not_called()
        self.assertEqual(runtime.launch_mode, 'realtime')
        self.assertEqual(runtime.state, 'FAILED')
        self.assertNotIn('startup_origin', runtime.config)
        self.assertEqual(runtime.owned, {})
        self.assertTrue(runtime.existing_backend)

    def test_realtime_shutdown_closes_owned_process_without_mock_pause(self):
        runtime = self.realtime_runtime()
        owned = Mock()
        owned.process.poll.return_value = None
        owned.record.return_value = {'role': 'Backend', 'pid': 123, 'owned': True}
        runtime.owned['Backend'] = owned
        with patch.object(runtime, 'pause_for_shutdown') as pause, patch('runtime.port_open') as port:
            runtime.stop()
        owned.stop.assert_called_once_with()
        pause.assert_not_called()
        port.assert_not_called()
        self.assertEqual(runtime.state, 'STOPPED')
        self.assertEqual(runtime.owned, {})

    def test_running_session_cannot_switch_mode_or_mutate_its_configuration(self):
        runtime = Runtime(self.root, self.base)
        runtime.config = mode_config(self.root, self.base, 'demo')
        runtime.launch_mode = 'demo'
        before = copy.deepcopy(runtime.config)
        directory = runtime.directory
        with patch.object(runtime, '_backend') as backend:
            for state in ('STARTING', 'READY', 'DEGRADED', 'STOPPING'):
                runtime.state = state
                with self.subTest(state=state), self.assertRaisesRegex(RuntimeError, 'switching modes'):
                    runtime.start('realtime')
                self.assertEqual(runtime.config, before)
                self.assertEqual(runtime.directory, directory)
                self.assertEqual(runtime.launch_mode, 'demo')
            backend.assert_not_called()

    def test_live_owned_process_blocks_relaunch_even_after_failure(self):
        runtime = Runtime(self.root, self.base)
        runtime.state = 'FAILED'
        process = Mock()
        process.process.poll.return_value = None
        runtime.owned['Map'] = process
        with patch.object(runtime, '_backend') as backend:
            with self.assertRaisesRegex(RuntimeError, 'current session'):
                runtime.start('realtime')
        backend.assert_not_called()
        self.assertIsNone(runtime.launch_mode)
        process.stop.assert_not_called()

    def test_normal_demo_shutdown_requires_pause_ack_before_closing_owned_processes(self):
        runtime = Runtime(self.root, self.base)
        runtime.launch_mode = 'demo'
        runtime.state = 'READY'
        backend = self.owned('Backend')
        runtime.owned['Backend'] = backend
        with patch('runtime.port_open', return_value=True), \
             patch.object(runtime, 'pause_for_shutdown', side_effect=OSError('pause acknowledgement unavailable')):
            with self.assertRaisesRegex(OSError, 'pause acknowledgement'):
                runtime.stop()
        self.assertEqual(runtime.state, 'READY')
        self.assertIs(runtime.owned['Backend'], backend)
        self.assertFalse(runtime.stopping.is_set())
        backend.stop.assert_not_called()

    def test_failed_demo_start_reclaims_new_processes_when_pause_ack_is_unavailable(self):
        runtime = Runtime(self.root, self.base)
        backend, command = self.owned('Backend'), self.owned('Command')
        def attach_backend():
            runtime.owned['Backend'] = backend
        def fail_client(role):
            self.assertEqual(role, 'Command')
            runtime.owned[role] = command
            raise ValueError('original client startup failure')
        with patch.object(runtime, '_backend', side_effect=attach_backend), \
             patch.object(runtime, 'start_role', side_effect=fail_client), \
             patch('runtime.port_open', return_value=True), \
             patch.object(runtime, 'pause_for_shutdown', side_effect=OSError('pause acknowledgement unavailable')):
            with self.assertRaisesRegex(ValueError, '^original client startup failure$'):
                runtime.start('demo')
        backend.stop.assert_called_once_with()
        command.stop.assert_called_once_with()
        self.assertEqual(runtime.state, 'FAILED')
        self.assertEqual(runtime.error, 'original client startup failure')
        self.assertEqual(runtime.owned, {})
        self.assertEqual(json.loads((runtime.directory / 'ownership.json').read_text()), [])

    def test_failed_backend_restart_cleans_only_replacement_backend(self):
        runtime = self.realtime_runtime()
        runtime.state = 'READY'
        old_backend, new_backend = self.owned('Backend'), self.owned('Backend')
        clients = {role: self.owned(role) for role in ROLES}
        runtime.owned = {'Backend': old_backend, **clients}
        def failed_replacement():
            runtime.owned['Backend'] = new_backend
            raise RuntimeError('replacement backend incompatible')
        with patch.object(runtime, '_backend', side_effect=failed_replacement):
            with self.assertRaisesRegex(RuntimeError, 'replacement backend incompatible'):
                runtime.restart('Backend')
        old_backend.stop.assert_called_once_with()
        new_backend.stop.assert_called_once_with()
        self.assertEqual(runtime.state, 'DEGRADED')
        self.assertEqual(runtime.owned, clients)
        for process in clients.values():
            process.stop.assert_not_called()


if __name__ == '__main__':
    unittest.main(verbosity=2)
