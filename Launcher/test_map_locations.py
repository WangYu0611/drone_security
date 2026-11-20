"""Launcher map coordinate validation and local favorites storage."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from map_locations import MAP_LOCATIONS_PATH, load_map_locations, map_origin, named_location, save_map_locations


class MapCoordinateTests(unittest.TestCase):
    def test_normalizes_input_including_zero_and_map_extrema(self):
        self.assertEqual(map_origin(' 0 ', 0), {'latitude': 0.0, 'longitude': 0.0, 'altitude': 0.0})
        for latitude, longitude in ((85.05112878, -180), (-85.05112878, 180)):
            with self.subTest(latitude=latitude, longitude=longitude):
                self.assertEqual(map_origin(latitude, longitude), {
                    'latitude': latitude, 'longitude': float(longitude), 'altitude': 0.0,
                })
        self.assertEqual(map_origin('+1.5e1', '-.25'), {
            'latitude': 15.0, 'longitude': -0.25, 'altitude': 0.0,
        })

    def test_rejects_missing_malformed_nonfinite_or_out_of_range_values(self):
        for value in ('', '   ', None, True, [], '1,2', '1_0', 'north', '1.2.3',
                      'NaN', 'inf', '1e9999', float('inf'), float('nan'), 10 ** 400):
            for latitude, longitude in ((value, 0), (0, value)):
                with self.subTest(latitude=latitude, longitude=longitude), self.assertRaises(ValueError):
                    map_origin(latitude, longitude)
        for latitude, longitude in ((85.05112879, 0), (-90, 0), (0, 180.000001), (0, -181)):
            with self.subTest(latitude=latitude, longitude=longitude), self.assertRaisesRegex(ValueError, 'within'):
                map_origin(latitude, longitude)

    def test_name_can_be_entered_or_derived_from_coordinates(self):
        self.assertEqual(named_location('  仓库  ', '31.25', '121.5'), {
            'name': '仓库', 'latitude': 31.25, 'longitude': 121.5,
        })
        self.assertEqual(named_location('  ', '0', '120.25')['name'], '0.0, 120.25')
        self.assertEqual(named_location(None, '0', '120.25')['name'], '0.0, 120.25')
        with self.assertRaisesRegex(ValueError, 'name'):
            named_location(1, 0, 0)


class MapLocationStoreTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='launcher locations ')
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.path = self.root / MAP_LOCATIONS_PATH
        self.first = named_location('厂区', '39.9', '116.3')
        self.second = named_location('仓库', '31.25', '121.5')

    def test_reload_empty_added_and_deleted_locations(self):
        self.assertEqual(load_map_locations(self.root), [])
        self.assertFalse(self.path.exists())
        save_map_locations(self.root, [self.first])
        self.assertEqual(load_map_locations(self.root), [self.first])
        save_map_locations(self.root, [self.first, self.second])
        self.assertEqual(load_map_locations(self.root), [self.first, self.second])
        self.assertIn('仓库', self.path.read_text(encoding='utf-8'))
        save_map_locations(self.root, [self.second])
        self.assertEqual(load_map_locations(self.root), [self.second])
        save_map_locations(self.root, [])
        self.assertEqual(load_map_locations(self.root), [])
        self.assertEqual(json.loads(self.path.read_text(encoding='utf-8')), [])

    def test_invalid_input_leaves_the_previous_list_unchanged(self):
        save_map_locations(self.root, [self.first])
        previous = self.path.read_bytes()
        with self.assertRaises(ValueError):
            save_map_locations(self.root, [self.second, {**self.first, 'latitude': 90}])
        self.assertEqual(self.path.read_bytes(), previous)

    def test_failed_replace_leaves_the_previous_file_and_cleans_up(self):
        save_map_locations(self.root, [self.first])
        previous = self.path.read_bytes()
        with patch('map_locations.Path.replace', side_effect=PermissionError('file is locked')):
            with self.assertRaises(PermissionError):
                save_map_locations(self.root, [self.second])
        self.assertEqual(self.path.read_bytes(), previous)
        self.assertEqual(list(self.path.parent.iterdir()), [self.path])

    def test_damaged_file_remains_available_for_recovery(self):
        self.path.parent.mkdir(parents=True)
        for raw in (b'{', b'null', b'[{"name":"only name"}]', b'\xff'):
            with self.subTest(raw=raw):
                self.path.write_bytes(raw)
                with self.assertRaisesRegex(ValueError, 'Cannot load saved locations'):
                    load_map_locations(self.root)
                self.assertEqual(self.path.read_bytes(), raw)


if __name__ == '__main__':
    unittest.main()
