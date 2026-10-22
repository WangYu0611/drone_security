"""Geographic defaults and explicit isolation of historical route regressions."""
import json
import math
import configparser
import re
import unittest
from unittest.mock import patch

from verify import (ROOT, BEIJING_MOCK_FIXTURE, PRODUCTION_MOCK_FIXTURE,
                    mock_fixture_for_test, production_map_test)
from qa_host import BeijingQaRuntime
from runtime import Runtime, ROLES, role_arguments


class FixturePolicyTests(unittest.TestCase):
    def test_historical_online_host_disables_city_override_only_for_its_clients(self):
        config = {'ue_executable': 'UnrealEditor.exe', 'project': 'UE5DroneControl.uproject',
                  'map': '/Game/Level/CesiumWorld', 'http_endpoint': 'http://127.0.0.1:22580',
                  'ws_endpoint': 'ws://127.0.0.1:22581/ws', 'max_fps': 30}
        instance = object.__new__(BeijingQaRuntime)
        flag = '-ini:Game:[MapStartup]:Enabled=false'
        with patch.object(Runtime, '_spawn') as spawn:
            for role in ROLES:
                with self.subTest(role=role):
                    args = role_arguments(config, ROOT, role, 'test-instance', ROOT / 'unused.log')
                    original = list(args)
                    instance._spawn(role, args, ROOT)
                    spawn.assert_called_with(role, original + [flag], ROOT)
                    self.assertEqual(args, original, 'The shared production argument list must remain unchanged')
                    self.assertNotIn(flag, role_arguments(config, ROOT, role, 'production', ROOT / 'unused.log'))
            backend_args = ['DroneBackend.exe', 'backend.yaml']
            instance._spawn('Backend', backend_args, ROOT)
            spawn.assert_called_with('Backend', backend_args, ROOT)

    def test_production_camera_and_mock_origin_share_one_location(self):
        text = (ROOT / 'Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
        section = re.search(r'(?ms)^\[MapStartup\]\s*\n.*?(?=^\[|\Z)', text)
        self.assertIsNotNone(section, 'Production map startup section is required')
        config = configparser.ConfigParser(interpolation=None, strict=False)
        config.read_string(section.group(0))
        settings = config['MapStartup']
        origin = json.loads((ROOT / PRODUCTION_MOCK_FIXTURE).read_text(encoding='utf-8'))['startup_formation']['origin']
        self.assertTrue(settings.getboolean('Enabled'))
        for setting, coordinate in [('Latitude', 'latitude'), ('Longitude', 'longitude'),
                                    ('HeightMeters', 'altitude')]:
            self.assertAlmostEqual(settings.getfloat(setting), origin[coordinate], places=9)

    def test_shipped_homes_match_marina_bay_reset_formation(self):
        fixture = json.loads((ROOT / PRODUCTION_MOCK_FIXTURE).read_text(encoding='utf-8'))
        origin = {'latitude': 1.2834, 'longitude': 103.8585, 'altitude': 50.0}
        self.assertEqual(fixture['startup_formation'],
                         {'origin': origin, 'spacing_m': 2.0, 'direction': 'east'})
        self.assertTrue(fixture['simulation'])
        self.assertEqual(sorted(fixture['uavs']), ['UAV-01', 'UAV-02', 'UAV-03', 'UAV-04'])
        scale = 6371000.0 * math.pi / 180.0 * math.cos(math.radians(origin['latitude']))
        for index, uid in enumerate(sorted(fixture['uavs'])):
            home = fixture['uavs'][uid]['home_position']
            self.assertEqual(home['latitude'], origin['latitude'])
            self.assertEqual(home['altitude'], origin['altitude'])
            self.assertAlmostEqual((home['longitude'] - origin['longitude']) * scale,
                                   index * 2.0, places=7)

    def test_beijing_geometry_is_an_independent_fixture(self):
        fixture = json.loads((ROOT / BEIJING_MOCK_FIXTURE).read_text(encoding='utf-8'))
        for index, uid in enumerate(sorted(fixture['uavs'])):
            home = fixture['uavs'][uid]['home_position']
            self.assertEqual(home['latitude'], 39.9807)
            self.assertAlmostEqual(home['longitude'], 116.34703 + index * .00003, places=10)
            self.assertEqual(home['altitude'], 30.0)

    def test_historical_tests_select_beijing_without_production_reset(self):
        for test in ('DroneOps.PlanConfiguration.MissionActions',
                     'DroneOps.P4.Workflow.MapLiveDraftGuard',
                     'DroneOps.P55.ReservationWidgets', 'DroneOps.Fleet.Workflow'):
            with self.subTest(test=test):
                self.assertEqual(mock_fixture_for_test(test), BEIJING_MOCK_FIXTURE)
                self.assertFalse(production_map_test(test))

    def test_startup_visibility_and_explicit_reset_exercise_production(self):
        self.assertTrue(production_map_test('DroneOps.Map.StartupVisibility'))
        self.assertEqual(mock_fixture_for_test('DroneOps.Map.StartupVisibility'),
                         PRODUCTION_MOCK_FIXTURE)
        self.assertEqual(mock_fixture_for_test('DroneOps.Fleet.Workflow', True),
                         PRODUCTION_MOCK_FIXTURE)


if __name__ == '__main__':
    unittest.main(verbosity=2)
