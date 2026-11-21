"""Launcher orchestration tests with inert widgets and no child processes."""
import json
from pathlib import Path
import queue
import runpy
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from deployment import load_deployment_selection, save_deployment_selection
from launch_modes import mode_config
from map_locations import load_map_locations, save_map_locations
from runtime import Runtime


Application = runpy.run_path(str(Path(__file__).with_name('launcher.pyw')),
                            run_name='launcher_ui_test')['Application']
UI = Application.__init__.__globals__


class Value:
    def __init__(self, value='', **kwargs):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


class Widget:
    def __init__(self, *args, **kwargs):
        self.values = dict(kwargs)
        self.selected = -1
        self.grid = Mock()
        self.pack = Mock()
        self.bind = Mock()
        self.columnconfigure = Mock()
        self.destroy = Mock()

    def config(self, **kwargs):
        self.values.update(kwargs)

    def current(self, index=None):
        if index is None:
            return self.selected
        self.selected = index

    def set(self, value):
        self.selected = -1


class ImmediateThread:
    def __init__(self, target, **kwargs):
        self.target = target

    def start(self):
        self.target()


class DeploymentUITests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.base = {'http_endpoint': 'http://127.0.0.1:19880',
                     'ws_endpoint': 'ws://127.0.0.1:19881/ws',
                     'runtime_directory': 'Saved/Stage1', 'auto_start': False}
        self.patch = patch.dict(UI, {'ROOT': self.root})
        self.patch.start()
        self.addCleanup(self.patch.stop)
        # The orchestrator tests must never cross into a real process/network.
        for target in ('runtime.OwnedProcess', 'runtime.api', 'runtime.port_open',
                       'runtime.compatible_snapshot'):
            guard = patch(target, side_effect=AssertionError('Unexpected external side effect: ' + target))
            guard.start()
            self.addCleanup(guard.stop)

    def app(self, role='Command'):
        app = Application.__new__(Application)
        app.config = dict(self.base)
        app.runtime = SimpleNamespace(owned={}, state='STOPPED', stop=Mock())
        app.role_choice = Value(role)
        app.deployment_path = Value(str(self.root / 'shared.json'))
        app.latitude, app.longitude, app.location_name = Value(), Value(), Value()
        app.map_locations, app.favorite_picker = [], Widget()
        app.location_feedback = 'Launcher.LocationHint'
        app.placed = {100}
        app.busy = app.polling = app.closing = False
        app.messages = queue.Queue()
        app.error = Widget()
        app.root = Mock()
        app.update_buttons = Mock()
        return app

    def derived(self, role='Command'):
        return {**self.base, 'deployment_role': role, 'deployment_path': str(self.root / 'shared.json'),
                'launch_mode': 'realtime', 'http_endpoint': 'http://192.168.20.10:28080',
                'ws_endpoint': 'ws://192.168.20.10:28081/ws'}

    def test_start_selected_role_derives_profile_then_runs_role_preflight_before_start(self):
        for role in ('All', 'Auto', 'Backend', 'Clients', 'Command', 'Map', 'Video', 'Pilot'):
            with self.subTest(role=role):
                app = self.app(role)
                config = self.derived(role)
                replacement = Mock(owned={})
                events = []
                loader = Mock(return_value=self.base)
                derive = Mock(side_effect=lambda *args, **kwargs: events.append('profile') or config)
                preflight = Mock(side_effect=lambda *args: events.append('preflight') or [])
                factory = Mock(side_effect=lambda *args: events.append('runtime') or replacement)
                replacement.start.side_effect = lambda *args: events.append('start')
                with patch.dict(UI, {'load_config': loader, 'deployment_config': derive,
                                     'preflight': preflight, 'Runtime': factory}), \
                     patch.object(UI['threading'], 'Thread', ImmediateThread):
                    app.start()
                derive.assert_called_once_with(self.root, self.base, app.deployment_path.get(), role)
                preflight.assert_called_once_with(self.root, config)
                factory.assert_called_once_with(self.root, config)
                self.assertEqual(events, ['profile', 'preflight', 'runtime', 'start'])
                replacement.start.assert_called_once_with()
                self.assertEqual(app.placed, set())
                self.assertTrue(app.messages.empty())
                self.assertEqual(load_deployment_selection(self.root),
                                 {'role': role, 'path': str(self.root / 'shared.json')})

    def test_invalid_profile_is_reported_without_preflight_or_runtime_replacement(self):
        app = self.app()
        original = app.runtime
        derive = Mock(side_effect=ValueError('Invalid deployment JSON'))
        preflight, factory = Mock(), Mock()
        with patch.dict(UI, {'load_config': Mock(return_value=self.base), 'deployment_config': derive,
                             'preflight': preflight, 'Runtime': factory}), \
             patch.object(UI['threading'], 'Thread', ImmediateThread):
            app.start()
        self.assertIs(app.runtime, original)
        self.assertEqual(app.messages.get_nowait(), 'Invalid deployment JSON')
        self.assertFalse(app.busy)
        preflight.assert_not_called()
        factory.assert_not_called()
        self.assertEqual(load_deployment_selection(self.root), {'role': None, 'path': None})

    def test_failed_role_preflight_preserves_runtime_and_previous_selection(self):
        app = self.app('Map')
        original = app.runtime
        save_deployment_selection(self.root, 'Video', self.root / 'previous.json')
        factory = Mock()
        with patch.dict(UI, {'preflight': Mock(return_value=['Missing Map assets']), 'Runtime': factory}):
            with self.assertRaisesRegex(RuntimeError, 'Missing Map assets'):
                app.prepare_start(self.derived('Map'))
        factory.assert_not_called()
        self.assertIs(app.runtime, original)
        self.assertEqual(load_deployment_selection(self.root)['role'], 'Video')

    def test_prepare_start_refuses_any_live_owned_process_even_when_state_says_failed(self):
        for state in ('STOPPED', 'FAILED', 'DEGRADED'):
            with self.subTest(state=state):
                app = self.app()
                live = Mock()
                live.process.poll.return_value = None
                app.runtime.state = state
                app.runtime.owned = {'Map': live}
                original = app.runtime
                factory = Mock()
                with patch.dict(UI, {'preflight': Mock(return_value=[]), 'Runtime': factory}):
                    with self.assertRaisesRegex(RuntimeError, 'current session'):
                        app.prepare_start(self.derived())
                self.assertIs(app.runtime, original)
                factory.assert_not_called()
                live.stop.assert_not_called()
                original.stop.assert_not_called()

    def test_prepare_start_reclaims_exited_owned_process_handles_before_replacement(self):
        app = self.app()
        old = Runtime(self.root, {**self.base, 'launch_mode': 'realtime'})
        exited = Mock()
        exited.process.poll.return_value = 0
        exited.record.return_value = {'role': 'Map', 'pid': 100, 'exit_code': 0}
        old.owned['Map'] = exited
        app.runtime = old
        replacement = Mock(owned={})
        with patch.dict(UI, {'preflight': Mock(return_value=[]), 'Runtime': Mock(return_value=replacement)}):
            app.prepare_start(self.derived())
        exited.stop.assert_called_once()
        self.assertEqual(old.owned, {})
        self.assertIs(app.runtime, replacement)

    def test_prepare_start_requires_session_shutdown_even_without_owned_processes(self):
        app = self.app()
        app.runtime.state = 'READY'
        factory = Mock()
        with patch.dict(UI, {'preflight': Mock(return_value=[]), 'Runtime': factory}):
            with self.assertRaisesRegex(RuntimeError, 'current session'):
                app.prepare_start(self.derived())
        factory.assert_not_called()

    def test_stop_failure_retains_live_ownership_and_blocks_role_replacement(self):
        app = self.app()
        old = Runtime(self.root, {**self.base, 'launch_mode': 'realtime'})
        process = Mock()
        process.process.poll.return_value = None
        process.stop.side_effect = RuntimeError('Unable to stop process')
        old.owned['Map'] = process
        app.runtime = old
        with self.assertRaisesRegex(RuntimeError, 'Unable to stop process'):
            old.stop_role('Map')
        self.assertIs(old.owned.get('Map'), process)
        factory = Mock()
        with patch.dict(UI, {'preflight': Mock(return_value=[]), 'Runtime': factory}):
            with self.assertRaisesRegex(RuntimeError, 'current session'):
                app.prepare_start(self.derived())
        factory.assert_not_called()

    def test_replacing_unowned_session_never_controls_external_backend(self):
        app = self.app()
        old = Runtime(self.root, {**self.base, 'launch_mode': 'realtime'})
        old.existing_backend = True
        old.pause_for_shutdown = Mock(side_effect=AssertionError('Do not pause external Backend'))
        old._backend = Mock(side_effect=AssertionError('Do not launch external Backend'))
        app.runtime = old
        replacement = Mock(owned={})
        with patch.dict(UI, {'preflight': Mock(return_value=[]), 'Runtime': Mock(return_value=replacement)}):
            app.prepare_start(self.derived('Video'))
        old.pause_for_shutdown.assert_not_called()
        old._backend.assert_not_called()
        self.assertEqual(old.owned, {})

    def test_local_all_start_uses_realtime_with_manual_coordinates_and_clears_lan_selection(self):
        app = self.app('All')
        app.deployment_path.set('  ')
        app.latitude.set(' 31.25 ')
        app.longitude.set('121.5')
        save_deployment_selection(self.root, 'Command', self.root / 'shared.json')
        replacement = Mock(owned={})
        derive = Mock(side_effect=AssertionError('Local All must not read LAN profile'))
        local_mode = Mock(wraps=mode_config)
        preflight = Mock(return_value=[])
        with patch.dict(UI, {'load_config': Mock(return_value=self.base), 'deployment_config': derive,
                             'mode_config': local_mode, 'preflight': preflight,
                             'Runtime': Mock(return_value=replacement)}), \
             patch.object(UI['threading'], 'Thread', ImmediateThread):
            app.start()
        derive.assert_not_called()
        local_mode.assert_called_once_with(self.root, self.base, 'realtime', validate_isolation=False)
        config = preflight.call_args.args[1]
        self.assertEqual(config['launch_mode'], 'realtime')
        self.assertEqual(config['manual_map_origin'], {'latitude': 31.25, 'longitude': 121.5, 'altitude': 0.0})
        self.assertFalse(config['mock_reset_on_start'])
        replacement.start.assert_called_once_with()
        self.assertEqual(load_deployment_selection(self.root), {'role': None, 'path': None})

    def test_explicit_shared_profile_selects_demo_with_or_without_pilot(self):
        fixture = self.root / 'Backend/mock_execution.json'
        fixture.parent.mkdir()
        fixture.write_text(json.dumps({'startup_formation': {
            'origin': {'latitude': 1.27, 'longitude': 103.86, 'altitude': 10}}}), encoding='utf-8')
        profile = self.root / 'shared.json'
        for include_pilot in (False, True):
            hosts = {'Backend': '192.168.20.10', 'Command': '192.168.20.11',
                     'Map': '192.168.20.11', 'Video': '192.168.20.11'}
            if include_pilot:
                hosts['Pilot'] = '192.168.20.11'
            profile.write_text(json.dumps({
                'schema_version': 1, 'mode': 'demo', 'hosts': hosts,
                'http_port': 28080, 'ws_port': 28081,
            }), encoding='utf-8')
            roles = ('Backend', 'Clients', 'Map') + (('Pilot',) if include_pilot else ())
            for role in roles:
                with self.subTest(role=role, include_pilot=include_pilot):
                    app = self.app(role)
                    app.latitude.set('37.75')
                    app.prepare_start = Mock()
                    with patch.dict(UI, {'load_config': Mock(return_value=self.base)}), \
                         patch.object(UI['threading'], 'Thread', ImmediateThread):
                        app.start()
                    self.assertTrue(app.messages.empty())
                    config = app.prepare_start.call_args.args[0]
                    self.assertEqual(config['launch_mode'], 'demo')
                    self.assertEqual(config['deployment_role'], role)
                    self.assertEqual(config['http_endpoint'], 'http://192.168.20.10:28080')
                    self.assertEqual('Pilot' in config['deployment_hosts'], include_pilot)
                    self.assertNotIn('manual_map_origin', config)

    def test_shared_map_and_client_group_use_manual_coordinates_and_reject_partial_coordinates(self):
        for role in ('Map', 'Clients'):
            for longitude in ('', '-122.4'):
                with self.subTest(role=role, longitude=longitude):
                    app = self.app(role)
                    app.latitude.set('37.75')
                    app.longitude.set(longitude)
                    app.prepare_start = Mock()
                    with patch.dict(UI, {'load_config': Mock(return_value=self.base),
                                         'deployment_config': Mock(return_value=self.derived(role))}), \
                         patch.object(UI['threading'], 'Thread', ImmediateThread):
                        app.start()
                    if longitude:
                        self.assertTrue(app.messages.empty())
                        self.assertEqual(app.prepare_start.call_args.args[0]['manual_map_origin'], {
                            'latitude': 37.75, 'longitude': -122.4, 'altitude': 0.0,
                        })
                    else:
                        app.prepare_start.assert_not_called()
                        self.assertIn('Longitude', app.messages.get_nowait())

    def test_auto_map_location_applies_only_when_map_resolves_locally(self):
        for local_roles in (('Backend', 'Command'), ('Map', 'Video')):
            with self.subTest(local_roles=local_roles):
                app = self.app('Auto')
                app.latitude.set('37.75')
                app.longitude.set('-122.4')
                app.prepare_start = Mock()
                config = {**self.derived('Auto'), 'deployment_local_roles': local_roles}
                with patch.dict(UI, {'load_config': Mock(return_value=self.base),
                                     'deployment_config': Mock(return_value=config)}), \
                     patch.object(UI['threading'], 'Thread', ImmediateThread):
                    app.start()
                self.assertTrue(app.messages.empty())
                self.assertEqual(app.prepare_start.call_args.args[0].get('manual_map_origin'),
                                 {'latitude': 37.75, 'longitude': -122.4, 'altitude': 0.0}
                                 if 'Map' in local_roles else None)

    def test_location_switch_stages_idle_coordinates_or_updates_running_map(self):
        for state in ('STOPPED', 'READY'):
            with self.subTest(state=state):
                app = self.app('Map')
                app.latitude.set(' 0 ')
                app.longitude.set('180')
                app.runtime.state = state
                app.runtime.manages_role = lambda role: role == 'Map'
                app.runtime.apply_map_location = Mock()
                with patch.object(UI['threading'], 'Thread', ImmediateThread):
                    app.switch_location()
                self.assertTrue(app.messages.empty())
                if state == 'READY':
                    app.runtime.apply_map_location.assert_called_once_with({
                        'latitude': 0.0, 'longitude': 180.0, 'altitude': 0.0,
                    })
                    self.assertEqual(app.location_feedback, 'Launcher.LocationHint')
                else:
                    app.runtime.apply_map_location.assert_not_called()
                    self.assertEqual(app.location_feedback, 'Launcher.LocationNextLaunch')

    def test_favorites_load_select_add_and_delete_without_starting_map(self):
        app = self.app('Map')
        saved = {'name': '厂区', 'latitude': 31.25, 'longitude': 121.5}
        save_map_locations(self.root, [saved])
        with patch.object(UI['tk'], 'Frame', Widget), patch.object(UI['tk'], 'Label', Widget), \
             patch.object(UI['tk'], 'Button', Widget), patch.object(UI['tk'], 'StringVar', Value), \
             patch.object(UI['ttk'], 'Combobox', Widget), patch.object(UI['ttk'], 'Entry', Widget):
            app.buttons = []
            app.text = lambda key: key
            app.build_location_panel(6)
        self.assertEqual(app.favorite_picker.values['values'], ['厂区'])
        app.favorite_picker.current(0)
        app.select_location()
        self.assertEqual((app.location_name.get(), app.latitude.get(), app.longitude.get()),
                         ('厂区', '31.25', '121.5'))

        app.location_name.set('')
        app.latitude.set('39.9')
        app.longitude.set('116.3')
        app.runtime.apply_map_location = Mock()
        app.prepare_start = Mock()
        app.add_location()
        added = {'name': '39.9, 116.3', 'latitude': 39.9, 'longitude': 116.3}
        self.assertEqual(load_map_locations(self.root), [saved, added])
        self.assertEqual(app.favorite_picker.current(), 1)
        self.assertEqual(app.favorite_picker.values['values'], ['厂区', '39.9, 116.3'])

        app.favorite_picker.current(0)
        app.remove_location()
        self.assertEqual(load_map_locations(self.root), [added])
        self.assertEqual(app.favorite_picker.values['values'], ['39.9, 116.3'])
        self.assertEqual(app.favorite_picker.current(), -1)
        app.runtime.apply_map_location.assert_not_called()
        app.prepare_start.assert_not_called()

    def button_app(self, local='Command', existing=False):
        app = self.app(local)
        owned = Mock()
        owned.process.poll.return_value = None
        app.runtime = SimpleNamespace(owned={local: owned}, state='READY', launch_mode='realtime',
                                      existing_backend=existing, manages_role=lambda role: role == local)
        app.buttons, app.role_buttons = [], {}
        for role in ('Backend', 'Command', 'Map', 'Video'):
            for key in ('Launcher.Restart', 'Launcher.Stop', 'Launcher.Focus'):
                button = Widget()
                app.buttons.append((button, key))
                app.role_buttons[button] = role
        app.role_selector = Widget()
        app.profile_entry = Widget()
        app.browse_button = Widget()
        app.start_button = Widget()
        app.switch_button = Widget()
        app.buttons.extend(((app.start_button, 'Launcher.Start'),
                            (app.switch_button, 'Launcher.SwitchLocation')))
        app.location_entries = [Widget(), Widget(), Widget()]
        app.location_labels = {}
        app.location_hint = Widget()
        app.text = lambda key: key
        return app

    def test_remote_operation_buttons_are_disabled_while_local_controls_remain_enabled(self):
        app = self.button_app()
        Application.update_buttons(app)
        for button in app.role_buttons:
            self.assertEqual(button.values['state'],
                             'normal' if app.role_buttons[button] == 'Command' else 'disabled')
        self.assertEqual(app.role_selector.values['state'], 'disabled')
        self.assertEqual(app.start_button.values['state'], 'disabled')
        self.assertEqual(app.switch_button.values['state'], 'disabled')

    def test_running_local_map_can_switch_location(self):
        app = self.button_app('Map')
        Application.update_buttons(app)
        self.assertEqual(app.start_button.values['state'], 'disabled')
        self.assertEqual(app.switch_button.values['state'], 'normal')
        self.assertEqual(app.favorite_picker.values['state'], 'readonly')

    def test_client_group_location_controls_follow_effective_mode(self):
        app = self.button_app('Map')
        app.role_choice.set('Clients')
        Application.update_buttons(app)
        self.assertEqual(app.switch_button.values['state'], 'normal')
        app.runtime.launch_mode = 'demo'
        Application.update_buttons(app)
        self.assertEqual(app.switch_button.values['state'], 'disabled')
        app.runtime.state = 'STOPPED'
        app.runtime.owned = {}
        Application.update_buttons(app)
        self.assertEqual(app.switch_button.values['state'], 'normal')

    def test_pilot_row_follows_local_mock_session(self):
        app = self.button_app('Map')
        app.labels = {role: Widget() for role in ('Backend', 'Command', 'Map', 'Video')}
        app.pilot_row = 11
        app.runtime.config = {'deployment_hosts': {'Pilot': '192.168.20.11'}}
        app.runtime.manages_role = lambda role: role == 'Map'
        with patch.object(UI['tk'], 'Label', Widget), patch.object(UI['tk'], 'Button', Widget):
            app.sync_pilot_row()
            self.assertNotIn('Pilot', app.labels)
            app.runtime.launch_mode = 'demo'
            app.sync_pilot_row()
            self.assertNotIn('Pilot', app.labels)
            app.runtime.manages_role = lambda role: role in ('Map', 'Pilot')
            app.sync_pilot_row()
            label = app.labels['Pilot']
            buttons = [button for button, role in app.role_buttons.items() if role == 'Pilot']
            self.assertEqual(len(buttons), 3)
            app.sync_pilot_row()
            self.assertIs(app.labels['Pilot'], label)
            app.runtime.owned['Pilot'] = Mock()
            Application.update_buttons(app)
            self.assertTrue(all(button.values['state'] == 'normal' for button in buttons))
            app.runtime.launch_mode = 'realtime'
            app.sync_pilot_row()
        label.destroy.assert_called_once()
        self.assertNotIn('Pilot', app.labels)
        self.assertNotIn('Pilot', app.role_buttons.values())
        for button in buttons:
            button.destroy.assert_called_once()

    def test_window_placement_uses_pilot_layout_when_pilot_is_local(self):
        app = self.button_app('Pilot')
        app.runtime.local_client_roles = ('Command', 'Map', 'Video', 'Pilot')
        app.runtime.owned['Pilot'].process.pid = 101
        app.runtime.components = {'Pilot': 'ONLINE'}
        app.runtime.log = Mock()
        area, rect = (0, 0, 1000, 800), (500, 0, 500, 400)
        layout = Mock(return_value={'Pilot': rect})
        place = Mock(return_value=True)
        with patch.dict(UI, {'work_area': Mock(return_value=area), 'rectangles': layout, 'place': place}):
            app.place_ready()
        layout.assert_called_once_with(area, pilot=True)
        place.assert_called_once_with(101, rect, 'Drone Security | Pilot')
        self.assertIn(101, app.placed)

    def test_existing_external_backend_buttons_are_disabled_even_on_backend_workstation(self):
        app = self.button_app('Backend', existing=True)
        app.runtime.owned = {}
        Application.update_buttons(app)
        for button in app.role_buttons:
            self.assertEqual(button.values['state'], 'disabled')

    def test_background_poll_keeps_idle_role_and_profile_controls_usable(self):
        for role in ('All', 'Auto', 'Backend', 'Clients', 'Command', 'Map', 'Video', 'Pilot'):
            for state in ('STOPPED', 'FAILED'):
                with self.subTest(role=role, state=state):
                    app = self.button_app(role)
                    app.runtime.owned = {}
                    app.runtime.state = state
                    app.update_buttons = lambda: Application.update_buttons(app)
                    workers = []
                    poll = Mock()
                    # Hold the poll worker in flight, as it is between UI ticks.
                    # Its completion does not call update_buttons again.
                    factory = lambda target, **kwargs: SimpleNamespace(
                        start=lambda: workers.append(target))
                    with patch.object(UI['threading'], 'Thread', factory):
                        app.action(poll, clear_error=False)
                    self.assertTrue(app.busy)
                    self.assertTrue(app.polling)
                    for before_completion in (True, False):
                        if not before_completion:
                            workers.pop()()
                        self.assertEqual(app.role_selector.values['state'], 'readonly')
                        self.assertEqual(app.profile_entry.values['state'], 'normal')
                        self.assertEqual(app.browse_button.values['state'], 'normal')
                        self.assertEqual(app.start_button.values['state'], 'normal')
                    poll.assert_called_once()
                    self.assertFalse(app.busy)

    def test_foreground_start_disables_role_and_profile_controls_until_complete(self):
        for role in ('All', 'Auto', 'Backend', 'Clients', 'Command', 'Map', 'Video', 'Pilot'):
            for state in ('STOPPED', 'FAILED'):
                with self.subTest(role=role, state=state):
                    app = self.button_app(role)
                    app.runtime.owned = {}
                    app.runtime.state = state
                    app.update_buttons = lambda: Application.update_buttons(app)
                    workers = []
                    start = Mock()
                    factory = lambda target, **kwargs: SimpleNamespace(
                        start=lambda: workers.append(target))
                    with patch.object(UI['threading'], 'Thread', factory):
                        app.action(start)
                    self.assertTrue(app.busy)
                    self.assertFalse(app.polling)
                    for widget in (app.role_selector, app.profile_entry,
                                   app.browse_button, app.start_button, app.favorite_picker,
                                   *app.location_entries):
                        self.assertEqual(widget.values['state'], 'disabled')
                    start.assert_not_called()
                    workers.pop()()
                    start.assert_called_once()
                    self.assertFalse(app.busy)
                    app.update_buttons()
                    self.assertEqual(app.role_selector.values['state'], 'readonly')
                    self.assertEqual(app.profile_entry.values['state'], 'normal')
                    self.assertEqual(app.browse_button.values['state'], 'normal')
                    self.assertEqual(app.start_button.values['state'], 'normal')

    def test_constructor_restores_realtime_roles_and_uses_single_start_action(self):
        catalog = {key: [key, key] for key in ('Stage1.Name', 'Launcher.STOPPED', 'Launcher.Layout',
                   'Launcher.ModeRealtime', 'Launcher.Restart', 'Launcher.Focus', 'Launcher.Stop',
                   'Launcher.Start', 'Launcher.Arrange', 'Launcher.Shutdown', 'Launcher.LocationHint',
                   'Launcher.FavoriteLocations', 'Launcher.Latitude', 'Launcher.Longitude',
                   'Launcher.LocationName', 'Launcher.RemoveLocation', 'Launcher.SwitchLocation',
                   'Launcher.AddLocation')}
        catalog_path = self.root / 'Scripts/P4/localization.json'
        catalog_path.parent.mkdir(parents=True)
        catalog_path.write_text(json.dumps(catalog), encoding='utf-8')
        for role in ('All', 'Auto', 'Backend', 'Clients', 'Pilot', None):
            with self.subTest(role=role):
                save_deployment_selection(self.root, role, self.root / 'shared.json' if role else None)
                runtime = SimpleNamespace(owned={}, state='STOPPED', launch_mode=None,
                                          existing_backend=False, manages_role=lambda _: True)
                preflight = Mock(side_effect=AssertionError('Choose role before preflight'))
                # A stale single-workstation setting must not prevent choosing
                # a valid shared profile in the launcher.
                stale_config = {**self.base, 'http_endpoint': None,
                                'runtime_directory': None, 'launch_mode': 'realtime',
                                'auto_start': True, 'startup_mode': 'demo'}
                factory, guard = Mock(return_value=runtime), Mock()
                root = Mock()
                with patch.dict(UI, {'load_config': Mock(return_value=stale_config),
                                     'Runtime': factory, 'SingleInstance': guard,
                                     'preflight': preflight}), \
                     patch.object(UI['tk'], 'Tk', return_value=root), \
                     patch.object(UI['tk'], 'StringVar', Value), \
                     patch.object(UI['tk'], 'Frame', Widget), \
                     patch.object(UI['tk'], 'Label', Widget), \
                     patch.object(UI['tk'], 'Entry', Widget), \
                     patch.object(UI['tk'], 'Button', Widget), \
                     patch.object(UI['ttk'], 'Combobox', Widget), \
                     patch.object(UI['ttk'], 'Entry', Widget):
                    start = Mock()
                    with patch.object(Application, 'start', start):
                        app = Application()
                preflight.assert_not_called()
                guard.assert_called_once_with('launcher:' + str(self.root.resolve()))
                idle = factory.call_args.args[1]
                self.assertEqual(idle['runtime_directory'], 'Saved/Launcher/Idle')
                self.assertNotIn('launch_mode', idle)
                self.assertEqual(app.role_choice.get(), role or 'All')
                self.assertEqual(app.deployment_path.get(), str(self.root / 'shared.json')
                                 if role else '')
                self.assertEqual(set(app.role_selector.values['values']),
                                 {'All', 'Auto', 'Backend', 'Clients', 'Command', 'Map', 'Video', 'Pilot'})
                self.assertEqual(app.role_selector.values['state'], 'readonly')
                start_buttons = [(button, key) for button, key in app.buttons if 'Start' in key]
                self.assertEqual([key for _, key in start_buttons], ['Launcher.Start'])
                self.assertEqual(start_buttons[0][0].values['state'], 'normal')
                callbacks = [call.args[1] for call in root.after.call_args_list
                             if call.args[0] == 500]
                self.assertEqual(len(callbacks), 1)
                callbacks[0]()
                start.assert_called_once_with()


if __name__ == '__main__':
    unittest.main(verbosity=2)
