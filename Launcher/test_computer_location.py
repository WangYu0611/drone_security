"""Mock-only location tests: never start PowerShell or query a real location."""
import json
import math
import subprocess
import threading
import unittest
from unittest.mock import Mock, patch

import computer_location


NOW = 1000.0
POSITION = {'ok': True, 'longitude': 121.4737, 'latitude': 31.2304,
            'altitude': 20.0, 'altitude_reference': 'ellipsoid',
            'accuracy_m': 40.0, 'timestamp': NOW - 5}


class ComputerLocationTests(unittest.TestCase):
    def setUp(self):
        self.process = Mock()
        self.process.returncode = 0
        self.process.poll.return_value = 0
        self.process.communicate.return_value = (json.dumps(POSITION), '')
        self.popen = self.enterContext(patch.object(computer_location.subprocess, 'Popen', return_value=self.process))
        self.enterContext(patch.object(computer_location.sys, 'platform', 'win32'))
        self.enterContext(patch.object(computer_location.time, 'time', return_value=NOW))

    def locate(self, payload=None, **options):
        if payload is not None:
            self.process.communicate.return_value = (json.dumps(payload), '')
        return computer_location.computer_origin(**options)

    def test_measured_ellipsoid_height_ignores_uav_geoid_and_helper_is_hidden(self):
        result = self.locate(geoid=12.5)
        self.assertEqual(result, {'longitude': POSITION['longitude'], 'latitude': POSITION['latitude'],
                                 'altitude': 20.0, 'source': 'computer',
                                 'accuracy_m': 40.0, 'height_estimated': False})
        arguments = self.popen.call_args.args[0]
        self.assertTrue(arguments[0].lower().endswith('powershell.exe'))
        self.assertIn('-NonInteractive', arguments)
        self.assertIn('-File', arguments)
        self.assertTrue(arguments[arguments.index('-File') + 1].endswith('computer_location.ps1'))
        self.assertEqual(arguments[arguments.index('-DeadlineUnixMilliseconds') + 1], '1015000')
        self.assertNotIn('-Command', arguments)
        self.assertEqual(self.popen.call_args.kwargs['stdin'], subprocess.DEVNULL)
        if hasattr(subprocess, 'CREATE_NO_WINDOW'):
            self.assertEqual(self.popen.call_args.kwargs['creationflags'], subprocess.CREATE_NO_WINDOW)
            self.assertEqual(self.popen.call_args.kwargs['startupinfo'].wShowWindow, subprocess.SW_HIDE)
        self.process.kill.assert_not_called()

    def test_missing_or_nan_altitude_is_explicit_estimated_ellipsoid_anchor(self):
        for altitude in (None, math.nan):
            with self.subTest(altitude=altitude):
                result = self.locate({**POSITION, 'altitude': altitude}, geoid=35)
                self.assertEqual(result['altitude'], 0.0)
                self.assertTrue(result['height_estimated'])
                self.assertEqual(result['source'], 'computer')
        payload = dict(POSITION)
        del payload['altitude']
        self.assertTrue(self.locate(payload)['height_estimated'])

    def test_unknown_or_missing_altitude_datum_is_estimated_not_relabelled(self):
        for datum in (None, 'unknown', 'AMSL', 'Ellipsoid'):
            with self.subTest(datum=datum):
                result = self.locate({**POSITION, 'altitude_reference': datum}, geoid=35)
                self.assertEqual(result['altitude'], 0.0)
                self.assertTrue(result['height_estimated'])
        payload = dict(POSITION)
        del payload['altitude_reference']
        self.assertTrue(self.locate(payload)['height_estimated'])

    def test_invalid_coordinates_accuracy_timestamps_and_height_are_rejected(self):
        for changes in ({'longitude': math.nan}, {'longitude': math.inf}, {'longitude': 181},
                        {'latitude': -91}, {'latitude': 86}, {'latitude': -85.05112879},
                        {'latitude': '31'}, {'latitude': True},
                        {'longitude': 0, 'latitude': 0}, {'longitude': None},
                        {'accuracy_m': 0}, {'accuracy_m': -1}, {'accuracy_m': math.inf}, {'accuracy_m': math.nan},
                        {'accuracy_m': None}, {'accuracy_m': True},
                        {'timestamp': None}, {'timestamp': True}, {'timestamp': math.inf},
                        {'altitude': math.inf}, {'altitude': True}, {'altitude': '20'},
                        {'altitude': -1001}, {'altitude': 100001}):
            with self.subTest(changes=changes), self.assertRaises(RuntimeError):
                self.locate({**POSITION, **changes})

    def test_five_minute_boundary_and_equator_are_valid_but_stale_and_future_are_rejected(self):
        self.assertEqual(self.locate({**POSITION, 'timestamp': NOW - 300, 'latitude': 0})['latitude'], 0)
        self.assertEqual(self.locate({**POSITION, 'latitude': 85.05112878})['latitude'], 85.05112878)
        with self.assertRaisesRegex(RuntimeError, 'five minutes'):
            self.locate({**POSITION, 'timestamp': NOW - 300.01})
        with self.assertRaisesRegex(RuntimeError, 'invalid'):
            self.locate({**POSITION, 'timestamp': NOW + .01})

    def test_system_errors_are_clear_and_do_not_forward_raw_diagnostics(self):
        for code, word in (('unsupported', 'unsupported'), ('denied', 'permission'),
                           ('disabled', 'disabled'), ('timeout', 'timed out'),
                           ('no_position', 'no usable'), ('unavailable', 'could not'),
                           ('invalid_position', 'invalid'), ('untrusted diagnostic', 'could not')):
            with self.subTest(code=code), self.assertRaisesRegex(RuntimeError, word) as caught:
                self.locate({'ok': False, 'error': code})
            self.assertNotIn('untrusted diagnostic', str(caught.exception))
        self.process.returncode = 1
        self.process.communicate.return_value = ('private output', 'private diagnostic')
        with self.assertRaisesRegex(RuntimeError, 'could not') as caught:
            computer_location.computer_origin()
        self.assertNotIn('private', str(caught.exception))

    def test_malformed_json_or_non_object_output_is_rejected(self):
        for output in ('not JSON', '[1,2]', 'null', '{}', '{}\n{}', '{"ok":false,"error":[]}'):
            with self.subTest(output=output), self.assertRaises(RuntimeError):
                self.process.communicate.return_value = (output, '')
                computer_location.computer_origin()

    def test_timeout_kills_and_reaps_only_the_retained_helper(self):
        self.process.poll.return_value = None
        self.process.communicate.side_effect = [subprocess.TimeoutExpired('fixed helper', .2), ('', '')]
        with patch.object(computer_location.time, 'monotonic', side_effect=[0, 0, 2]):
            with self.assertRaisesRegex(RuntimeError, 'timed out'):
                computer_location.computer_origin(timeout=1)
        self.process.kill.assert_called_once_with()
        self.assertEqual(self.process.communicate.call_args.kwargs['timeout'], 2)

    def test_process_startup_consumes_the_same_timeout_budget(self):
        elapsed = [0.0]
        self.process.poll.return_value = None

        def slow_process_start(*args, **kwargs):
            elapsed[0] += 2.0
            return self.process

        self.popen.side_effect = slow_process_start
        with patch.object(computer_location.time, 'monotonic', side_effect=lambda: elapsed[0]):
            with self.assertRaisesRegex(RuntimeError, 'timed out'):
                computer_location.computer_origin(timeout=1)
        self.process.kill.assert_called_once_with()
        # No location-output wait starts after the startup budget was exhausted.
        self.process.communicate.assert_called_once_with(timeout=2)

    def test_cancellation_after_start_kills_and_reaps_helper(self):
        self.process.poll.return_value = None
        cancelled = Mock(side_effect=[False, True])
        with self.assertRaisesRegex(RuntimeError, 'cancelled'):
            computer_location.computer_origin(cancelled=cancelled)
        self.process.kill.assert_called_once_with()
        self.process.communicate.assert_called_once_with(timeout=2)

    def test_cancelled_event_never_starts_provider(self):
        cancelled = threading.Event()
        cancelled.set()
        with self.assertRaisesRegex(RuntimeError, 'cancelled'):
            computer_location.computer_origin(cancelled=cancelled)
        self.popen.assert_not_called()

    def test_unsupported_platform_and_invalid_options_never_start_provider(self):
        with patch.object(computer_location.sys, 'platform', 'linux'):
            with self.assertRaisesRegex(RuntimeError, 'unsupported'):
                computer_location.computer_origin()
        for options in ({'timeout': 0}, {'timeout': math.nan}, {'timeout': True},
                        {'timeout': 601}, {'geoid': math.inf}, {'geoid': True}):
            with self.subTest(options=options), self.assertRaises(RuntimeError):
                computer_location.computer_origin(**options)
        self.popen.assert_not_called()

    def test_spawn_failure_is_clear_without_exception_details(self):
        self.popen.side_effect = OSError('private path or operating-system detail')
        with self.assertRaisesRegex(RuntimeError, 'could not') as caught:
            computer_location.computer_origin()
        self.assertNotIn('private', str(caught.exception))


if __name__ == '__main__':
    unittest.main()
