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
from runtime import Runtime


Application = runpy.run_path(str(Path(__file__).with_name('launcher.pyw')),
                            run_name='launcher_ui_test')['Application']
UI = Application.__init__.__globals__


class Value:
    def __init__(self, value=None, **kwargs):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


class Widget:
    def __init__(self, *args, **kwargs):
        self.values = dict(kwargs)
        self.grid = Mock()
        self.pack = Mock()
        self.bind = Mock()
        self.columnconfigure = Mock()

    def config(self, **kwargs):
        self.values.update(kwargs)


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
        for role in ('Backend', 'Command', 'Map', 'Video'):
            with self.subTest(role=role):
                app = self.app(role)
                config = self.derived(role)
                replacement = Mock(owned={})
                events = []
                loader = Mock(return_value=self.base)
                derive = Mock(side_effect=lambda *args: events.append('profile') or config)
                preflight = Mock(side_effect=lambda *args: events.append('preflight') or [])
                factory = Mock(side_effect=lambda *args: events.append('runtime') or replacement)
                replacement.start.side_effect = lambda *args: events.append('start')
                with patch.dict(UI, {'load_config': loader, 'deployment_config': derive,
                                     'preflight': preflight, 'Runtime': factory}), \
                     patch.object(UI['threading'], 'Thread', ImmediateThread):
                    app.start_deployment()
                derive.assert_called_once_with(self.root, self.base, app.deployment_path.get(), role)
                preflight.assert_called_once_with(self.root, config)
                factory.assert_called_once_with(self.root, config)
                self.assertEqual(events, ['profile', 'preflight', 'runtime', 'start'])
                self.assertEqual(replacement.start.call_count, 1)
                self.assertIn(replacement.start.call_args.args, ((), (None,)))
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
            app.start_deployment()
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

    def test_legacy_all_start_uses_local_config_and_clears_saved_lan_selection(self):
        app = self.app('All')
        save_deployment_selection(self.root, 'Command', self.root / 'shared.json')
        replacement = Mock(owned={})
        derive = Mock(side_effect=AssertionError('Legacy All must not read LAN profile'))
        preflight = Mock(return_value=[])
        with patch.dict(UI, {'load_config': Mock(return_value=self.base), 'deployment_config': derive,
                             'preflight': preflight, 'Runtime': Mock(return_value=replacement)}), \
             patch.object(UI['threading'], 'Thread', ImmediateThread):
            app.start('demo')
        derive.assert_not_called()
        preflight.assert_called_once_with(self.root, self.base)
        replacement.start.assert_called_once_with('demo')
        self.assertEqual(load_deployment_selection(self.root), {'role': None, 'path': None})

    def test_start_buttons_cannot_launch_wrong_deployment_mode(self):
        app = self.app('Map')
        app.action = Mock()
        app.start('demo')
        app.start('realtime')
        app.action.assert_not_called()
        app.role_choice.set('All')
        app.start_deployment()
        app.action.assert_not_called()

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
        app.deployment_start = Widget()
        app.text = lambda key: key
        return app

    def test_remote_operation_buttons_are_disabled_while_local_controls_remain_enabled(self):
        app = self.button_app()
        Application.update_buttons(app)
        for button, key in app.buttons:
            self.assertEqual(button.values['state'],
                             'normal' if app.role_buttons[button] == 'Command' else 'disabled')
        self.assertEqual(app.role_selector.values['state'], 'disabled')
        self.assertEqual(app.deployment_start.values['state'], 'disabled')

    def test_existing_external_backend_buttons_are_disabled_even_on_backend_workstation(self):
        app = self.button_app('Backend', existing=True)
        app.runtime.owned = {}
        Application.update_buttons(app)
        for button, key in app.buttons:
            self.assertEqual(button.values['state'], 'disabled')

    def test_background_poll_keeps_idle_role_and_profile_controls_usable(self):
        for role in ('All', 'Backend', 'Command', 'Map', 'Video'):
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
                        self.assertEqual(app.deployment_start.values['state'],
                                         'disabled' if role == 'All' else 'normal')
                    poll.assert_called_once()
                    self.assertFalse(app.busy)

    def test_foreground_start_disables_role_and_profile_controls_until_complete(self):
        for role in ('All', 'Backend', 'Command', 'Map', 'Video'):
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
                                   app.browse_button, app.deployment_start):
                        self.assertEqual(widget.values['state'], 'disabled')
                    start.assert_not_called()
                    workers.pop()()
                    start.assert_called_once()
                    self.assertFalse(app.busy)
                    app.update_buttons()
                    self.assertEqual(app.role_selector.values['state'], 'readonly')
                    self.assertEqual(app.profile_entry.values['state'], 'normal')
                    self.assertEqual(app.browse_button.values['state'], 'normal')
                    self.assertEqual(app.deployment_start.values['state'],
                                     'disabled' if role == 'All' else 'normal')

    def test_constructor_restores_choice_and_opens_gui_without_global_preflight(self):
        catalog = {key: [key, key] for key in ('Stage1.Name', 'Launcher.STOPPED', 'Launcher.Layout',
                   'Launcher.ModeUnset', 'Launcher.Restart', 'Launcher.Focus', 'Launcher.Stop',
                   'Launcher.StartDemo', 'Launcher.StartRealtime', 'Launcher.Arrange', 'Launcher.Shutdown')}
        catalog_path = self.root / 'Scripts/P4/localization.json'
        catalog_path.parent.mkdir(parents=True)
        catalog_path.write_text(json.dumps(catalog), encoding='utf-8')
        for role in ('Backend', None):
            with self.subTest(role=role):
                save_deployment_selection(self.root, role, self.root / 'shared.json' if role else None)
                runtime = SimpleNamespace(owned={}, state='STOPPED', launch_mode=None,
                                          existing_backend=False, manages_role=lambda _: True)
                preflight = Mock(side_effect=AssertionError('Choose role before preflight'))
                # A stale single-workstation setting must not prevent choosing
                # a valid shared profile in the launcher.
                stale_config = {**self.base, 'http_endpoint': None,
                                'runtime_directory': None, 'launch_mode': 'realtime'}
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
                     patch.object(UI['ttk'], 'Combobox', Widget):
                    app = Application()
                preflight.assert_not_called()
                guard.assert_called_once_with('launcher:' + str(self.root.resolve()))
                idle = factory.call_args.args[1]
                self.assertEqual(idle['runtime_directory'], 'Saved/Launcher/Idle')
                self.assertNotIn('launch_mode', idle)
                self.assertEqual(app.role_choice.get(), role or 'All')
                self.assertEqual(app.role_selector.values['state'], 'readonly')
                self.assertEqual(app.deployment_start.values['state'], 'normal' if role else 'disabled')


if __name__ == '__main__':
    unittest.main(verbosity=2)
