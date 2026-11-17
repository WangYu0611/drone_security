"""Shared LAN profile checks; sockets are mocked, no Backend/UE is started."""
import copy
import json
from pathlib import Path
import socket
import tempfile
import unittest
from unittest.mock import call, patch

from deployment import (DEPLOYMENT_ROLES, DEPLOYMENT_SELECTIONS, SELECTION_PATH, deployment_config,
                        load_deployment_selection, save_deployment_selection,
                        validate_local_role)
from launch_modes import mode_config


class DeploymentFixture(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='drone deployment tests ')
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        (self.root / 'Backend').mkdir()
        self.origin = {'longitude': 103.862188, 'latitude': 1.279445, 'altitude': 15.0}
        self.fixture = self.root / 'Backend/mock_execution.json'
        self.fixture.write_text(json.dumps({'startup_formation': {'origin': self.origin}}), encoding='utf-8')
        self.backend_config = self.root / 'Backend/operator.yaml'
        self.backend_config.write_text('operator_flight_policy: preserve\n', encoding='utf-8')
        self.base = {
            'http_endpoint': 'http://127.0.0.1:19880',
            'ws_endpoint': 'ws://127.0.0.1:19881/ws',
            'runtime_directory': 'Saved/Stage1',
            'realtime_backend_config': 'Backend/operator.yaml',
            'mock_fixture': 'historical-fixture.json',
            'mock_reset_on_start': True,
            'operator': {'preserve': ['nested']},
        }
        self.profile = {
            'schema_version': 1, 'mode': 'realtime',
            'hosts': dict(zip(DEPLOYMENT_ROLES, ('192.168.10.10', '192.168.10.11',
                                               '192.168.10.12', '192.168.10.13'))),
            'http_port': 8080, 'ws_port': 8081,
        }
        self.path = self.root / 'shared deployment.json'
        self.write_profile()

    def write_profile(self):
        self.path.write_text(json.dumps(self.profile), encoding='utf-8')

    def derive(self, role='Map'):
        return deployment_config(self.root, self.base, self.path, role)


class DeploymentConfigTests(DeploymentFixture):
    def test_all_roles_derive_same_backend_and_isolated_runtime(self):
        before = copy.deepcopy(self.base)
        files_before = {p: p.read_bytes() for p in (self.path, self.fixture, self.backend_config)}
        with patch('deployment.socket.socket') as socket_factory:
            results = [self.derive(role) for role in DEPLOYMENT_SELECTIONS]
        socket_factory.assert_not_called()
        for role, result in zip(DEPLOYMENT_SELECTIONS, results):
            with self.subTest(role=role):
                self.assertEqual(result['http_endpoint'], 'http://192.168.10.10:8080')
                self.assertEqual(result['ws_endpoint'], 'ws://192.168.10.10:8081/ws')
                self.assertEqual(result['runtime_directory'], f'Saved/Deployment/realtime/{role}')
                self.assertEqual(result['deployment_role'], role)
                self.assertEqual(result['deployment_path'], str(self.path.resolve()))
                self.assertEqual(result['deployment_hosts'], self.profile['hosts'])
                self.assertEqual(result['deployment_http_port'], 8080)
                self.assertEqual(result['deployment_ws_port'], 8081)
                self.assertEqual(result['backend_config'], 'Backend/operator.yaml')
                self.assertEqual(result['launch_mode'], 'realtime')
                self.assertFalse(result['mock_reset_on_start'])
        results[0]['operator']['preserve'].append('only-derived')
        results[0]['deployment_hosts']['Map'] = '10.0.0.99'
        self.assertEqual(results[1]['deployment_hosts'], self.profile['hosts'])
        self.assertEqual(self.base, before)
        self.assertEqual({p: p.read_bytes() for p in files_before}, files_before)
        self.assertFalse((self.root / 'Saved').exists())

    def test_demo_keeps_shipped_fixture_without_rewriting_fleet_or_policy(self):
        self.profile['mode'] = 'demo'
        self.write_profile()
        before = self.fixture.read_bytes()
        result = self.derive('Backend')
        self.assertEqual(result['mock_fixture'], 'Backend/mock_execution.json')
        self.assertEqual(result['startup_origin'], self.origin)
        self.assertEqual(result['launch_mode'], 'demo')
        self.assertEqual(result['runtime_directory'], 'Saved/Deployment/demo/Backend')
        self.assertEqual(self.fixture.read_bytes(), before)
        self.assertFalse((self.root / 'Saved').exists())

    def test_relative_profile_and_utf8_bom_are_supported(self):
        self.path.write_text(json.dumps(self.profile), encoding='utf-8-sig')
        result = deployment_config(self.root, self.base, self.path.name, 'Video')
        self.assertEqual(result['deployment_path'], str(self.path.resolve()))

    def test_shared_profile_overrides_conflicting_invalid_or_missing_legacy_network(self):
        legacy_values = (
            {'http_endpoint': 'http://127.0.0.1:8080', 'ws_endpoint': 'ws://127.0.0.1:8081/ws',
             'runtime_directory': 'Saved/conflict', 'realtime_runtime_directory': 'Saved/conflict',
             'realtime_http_endpoint': 'http://127.0.0.1:8080',
             'realtime_ws_endpoint': 'ws://127.0.0.1:8081/ws'},
            {'http_endpoint': None, 'ws_endpoint': 'not-a-url', 'runtime_directory': 123,
             'realtime_runtime_directory': {}, 'realtime_http_endpoint': 'http://bad:999999',
             'realtime_ws_endpoint': None},
            {},
        )
        for mode in ('demo', 'realtime'):
            self.profile['mode'] = mode
            self.write_profile()
            for old in legacy_values:
                base = {k: v for k, v in self.base.items() if k not in (
                    'http_endpoint', 'ws_endpoint', 'runtime_directory',
                    'realtime_http_endpoint', 'realtime_ws_endpoint', 'realtime_runtime_directory')}
                base.update(old)
                before = copy.deepcopy(base)
                for role in DEPLOYMENT_SELECTIONS:
                    with self.subTest(mode=mode, old=old, role=role):
                        config = deployment_config(self.root, base, self.path, role)
                        self.assertEqual(config['http_endpoint'], 'http://192.168.10.10:8080')
                        self.assertEqual(config['ws_endpoint'], 'ws://192.168.10.10:8081/ws')
                        self.assertEqual(config['runtime_directory'], f'Saved/Deployment/{mode}/{role}')
                self.assertEqual(base, before)

    def test_single_machine_mode_isolation_stays_enabled_by_default(self):
        for change, message in (
                ({'realtime_http_endpoint': self.base['http_endpoint']}, 'ports'),
                ({'realtime_runtime_directory': self.base['runtime_directory']}, 'directories')):
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, message):
                mode_config(self.root, {**self.base, **change}, 'realtime')

    def test_missing_or_malformed_file_is_actionable(self):
        for contents in ('{', '[1]', 'null'):
            with self.subTest(contents=contents):
                self.path.write_text(contents, encoding='utf-8')
                with self.assertRaises(ValueError):
                    self.derive()
        self.path.unlink()
        with self.assertRaisesRegex(ValueError, 'Cannot read'):
            self.derive()

    def test_exact_schema_and_hosts_reject_missing_unknown_or_duplicate_fields(self):
        for field in self.profile:
            with self.subTest(missing=field):
                value = copy.deepcopy(self.profile)
                del value[field]
                self.path.write_text(json.dumps(value), encoding='utf-8')
                with self.assertRaisesRegex(ValueError, 'missing'):
                    self.derive()
        value = {**self.profile, 'http_endpoint': 'http://10.0.0.1:8080'}
        self.path.write_text(json.dumps(value), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'unknown'):
            self.derive()
        for hosts in ([], None, {**self.profile['hosts'], 'map': '10.0.0.5'},
                      {**self.profile['hosts'], 'All': '127.0.0.1'},
                      {k: v for k, v in self.profile['hosts'].items() if k != 'Map'}):
            with self.subTest(hosts=hosts):
                self.path.write_text(json.dumps({**self.profile, 'hosts': hosts}), encoding='utf-8')
                with self.assertRaises(ValueError):
                    self.derive()
        self.path.write_text(json.dumps(self.profile).replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1'), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            self.derive()

    def test_schema_mode_and_role_require_supported_values(self):
        for field, bad_values in (
                ('schema_version', (True, False, 0, 2, '1', 1.0, None)),
                ('mode', ('Realtime', 'live', '', None, 1, True, [], {}))):
            for value in bad_values:
                with self.subTest(field=field, value=value):
                    self.path.write_text(json.dumps({**self.profile, field: value}), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        self.derive()
        self.write_profile()
        for role in (None, '', 'map', 'all', 'Pilot', [], {}):
            with self.subTest(role=role), self.assertRaises(ValueError):
                self.derive(role)

    def test_ipv4_unicast_addresses_are_required(self):
        for address in ('localhost', '192.168.1.1/24', '192.168.1.1:8080', '::1',
                        '::ffff:192.168.1.1', '0.0.0.0', '0.1.2.3',
                        '224.0.0.1', '239.255.255.250', '240.0.0.1',
                        '255.255.255.255', '192.168.10.010', '256.1.1.1',
                        ' 192.168.10.11', '', 12345, None, True):
            with self.subTest(address=address):
                profile = copy.deepcopy(self.profile)
                profile['hosts']['Command'] = address
                self.path.write_text(json.dumps(profile), encoding='utf-8')
                with self.assertRaisesRegex(ValueError, 'Command'):
                    self.derive()

    def test_repeated_and_loopback_hosts_support_local_and_mixed_deployments(self):
        for addresses in (('127.0.0.1',) * 4, ('192.168.1.2',) * 4,
                          ('127.0.0.1', '127.1.2.3', '192.168.1.3', '192.168.1.3')):
            self.profile['hosts'] = dict(zip(DEPLOYMENT_ROLES, addresses))
            self.write_profile()
            for role in DEPLOYMENT_SELECTIONS:
                with self.subTest(addresses=addresses, role=role):
                    config = self.derive(role)
                    self.assertEqual(config['deployment_hosts'], self.profile['hosts'])
                    self.assertEqual(config['deployment_role'], role)
                    self.assertEqual(config['http_endpoint'], f'http://{addresses[0]}:8080')
                    self.assertEqual(config['ws_endpoint'], f'ws://{addresses[0]}:8081/ws')

    def test_all_selection_keeps_four_host_keys_and_shared_mode(self):
        for mode in ('demo', 'realtime'):
            with self.subTest(mode=mode):
                self.profile['mode'] = mode
                self.profile['hosts'] = dict.fromkeys(DEPLOYMENT_ROLES, '127.0.0.1')
                self.write_profile()
                config = self.derive('All')
                self.assertEqual(set(config['deployment_hosts']), set(DEPLOYMENT_ROLES))
                self.assertEqual(config['launch_mode'], mode)
                self.assertEqual(config['runtime_directory'], f'Saved/Deployment/{mode}/All')
                self.assertEqual(config['deployment_role'], 'All')

    def test_other_valid_unicast_ranges_are_allowed(self):
        self.profile['hosts'] = dict(zip(DEPLOYMENT_ROLES,
                                        ('10.1.1.1', '172.16.0.2', '169.254.1.3', '8.8.8.8')))
        self.write_profile()
        self.assertEqual(self.derive()['deployment_hosts'], self.profile['hosts'])

    def test_ports_require_distinct_non_boolean_integers(self):
        for field in ('http_port', 'ws_port'):
            for value in (True, False, 0, -1, 65536, 8080.0, '8080', None, [], {}):
                with self.subTest(field=field, value=value):
                    self.path.write_text(json.dumps({**self.profile, field: value}), encoding='utf-8')
                    with self.assertRaisesRegex(ValueError, field):
                        self.derive()
        self.profile.update(http_port=1, ws_port=65535)
        self.write_profile()
        self.assertEqual(self.derive()['ws_endpoint'], 'ws://192.168.10.10:65535/ws')
        self.profile['ws_port'] = 1
        self.write_profile()
        with self.assertRaisesRegex(ValueError, 'must differ'):
            self.derive()


class LocalRoleValidationTests(DeploymentFixture):
    def test_local_validation_only_binds_selected_address_and_closes(self):
        for role in DEPLOYMENT_ROLES:
            with self.subTest(role=role), patch('deployment.socket.socket') as factory:
                probe = factory.return_value.__enter__.return_value
                self.assertIsNone(validate_local_role(self.derive(role)))
                factory.assert_called_once_with(socket.AF_INET, socket.SOCK_DGRAM)
                probe.bind.assert_called_once_with((self.profile['hosts'][role], 0))
                probe.connect.assert_not_called()
                probe.send.assert_not_called()
                probe.sendto.assert_not_called()
                factory.return_value.__exit__.assert_called_once()

    def test_all_selection_binds_every_role_address_and_closes_each_probe(self):
        for addresses in (tuple(self.profile['hosts'].values()), ('127.0.0.1',) * 4):
            with self.subTest(addresses=addresses), patch('deployment.socket.socket') as factory:
                self.profile['hosts'] = dict(zip(DEPLOYMENT_ROLES, addresses))
                self.write_profile()
                probe = factory.return_value.__enter__.return_value
                self.assertIsNone(validate_local_role(self.derive('All')))
                self.assertEqual(factory.call_args_list, [call(socket.AF_INET, socket.SOCK_DGRAM)] * 4)
                self.assertEqual(probe.bind.call_args_list, [call((address, 0)) for address in addresses])
                self.assertEqual(factory.return_value.__exit__.call_count, 4)
                probe.connect.assert_not_called()
                probe.send.assert_not_called()
                probe.sendto.assert_not_called()

    def test_all_selection_rejects_first_nonlocal_role_before_remaining_probes(self):
        with patch('deployment.socket.socket') as factory:
            probe = factory.return_value.__enter__.return_value
            probe.bind.side_effect = [None, OSError('not local')]
            with self.assertRaisesRegex(RuntimeError, 'Command.*192.168.10.11'):
                validate_local_role(self.derive('All'))
            self.assertEqual(probe.bind.call_args_list,
                             [call(('192.168.10.10', 0)), call(('192.168.10.11', 0))])
            self.assertEqual(factory.call_count, 2)
            self.assertEqual(factory.return_value.__exit__.call_count, 2)

    def test_wrong_computer_fails_with_role_and_address(self):
        with patch('deployment.socket.socket') as factory:
            factory.return_value.__enter__.return_value.bind.side_effect = OSError('not local')
            with self.assertRaisesRegex(RuntimeError, 'Map.*192.168.10.12'):
                validate_local_role(self.derive())
            factory.return_value.__exit__.assert_called_once()

    def test_validation_never_resolves_a_hostname(self):
        config = self.derive()
        config['deployment_hosts']['Map'] = 'server.example'
        with patch('deployment.socket.socket') as factory, self.assertRaises(ValueError):
            validate_local_role(config)
        factory.assert_not_called()


class LocalSelectionTests(DeploymentFixture):
    def test_all_selection_persists_shared_profile_path_without_copying_hosts(self):
        before = self.path.read_bytes()
        save_deployment_selection(self.root, 'All', self.path)
        expected = {'role': 'All', 'path': str(self.path.resolve())}
        self.assertEqual(load_deployment_selection(self.root), expected)
        self.assertEqual(json.loads((self.root / SELECTION_PATH).read_text(encoding='utf-8')), expected)
        self.assertEqual(self.path.read_bytes(), before)

    def test_choice_persists_only_role_and_profile_path(self):
        self.assertEqual(load_deployment_selection(self.root), {'role': None, 'path': None})
        before = self.path.read_bytes()
        save_deployment_selection(self.root, 'Video', self.path.name)
        expected = {'role': 'Video', 'path': str(self.path.resolve())}
        self.assertEqual(load_deployment_selection(self.root), expected)
        self.assertEqual(json.loads((self.root / SELECTION_PATH).read_text(encoding='utf-8')), expected)
        self.assertEqual(self.path.read_bytes(), before)
        self.assertFalse((self.root / SELECTION_PATH.with_suffix('.tmp')).exists())
        save_deployment_selection(self.root, None, None)
        self.assertEqual(load_deployment_selection(self.root), {'role': None, 'path': None})

    def test_missing_profile_is_retained_but_bad_choice_is_ignored(self):
        save_deployment_selection(self.root, 'Map', 'missing-profile.json')
        self.assertEqual(load_deployment_selection(self.root)['role'], 'Map')
        for value in ('{', 'null', '[]', '{"role":"Map","path":null}',
                      '{"role":"Unknown","path":"a.json"}',
                      '{"role":"Map","path":"a.json","hosts":{}}'):
            with self.subTest(value=value):
                (self.root / SELECTION_PATH).write_text(value, encoding='utf-8')
                self.assertEqual(load_deployment_selection(self.root), {'role': None, 'path': None})

    def test_invalid_selection_does_not_create_local_file(self):
        for role, path in (('Unknown', 'a.json'), ('Command', None), ('Backend', '')):
            with self.subTest(role=role, path=path), self.assertRaises(ValueError):
                save_deployment_selection(self.root, role, path)
        self.assertFalse((self.root / SELECTION_PATH).exists())


if __name__ == '__main__':
    unittest.main()
