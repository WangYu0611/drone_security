import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock
import bootstrap


class BuildTests(unittest.TestCase):
    def test_backend_build_runs_only_cmake_without_any_ue_configuration(self):
        with tempfile.TemporaryDirectory(prefix='drone backend build ') as folder:
            root = Path(folder)
            (root / 'Saved').mkdir()
            vswhere = root / 'Microsoft Visual Studio/Installer/vswhere.exe'
            vswhere.parent.mkdir(parents=True)
            vswhere.touch()
            toolchain = root / 'vcpkg/scripts/buildsystems/vcpkg.cmake'
            toolchain.parent.mkdir(parents=True)
            toolchain.touch()
            with patch.object(bootstrap, 'ROOT', root), patch.dict(os.environ, {
                    'ProgramFiles(x86)': folder, 'UE5DRONE_VCPKG_TOOLCHAIN': str(toolchain)}), \
                 patch('bootstrap.subprocess.check_output', return_value=json.dumps([
                     {'installationPath': str(root / 'VS'), 'installationVersion': '18.0'}])), \
                 patch('bootstrap.shutil.which', return_value='cmake.exe'), \
                 patch('bootstrap.subprocess.Popen', side_effect=lambda *a, **k: Mock(
                     stdout=io.BytesIO(b'backend build output\n'), wait=Mock(return_value=0))) as popen, \
                 patch('sys.stdout', new_callable=io.StringIO):
                # Missing UE/project keys must not even be read in this mode.
                bootstrap.build({}, backend_only=True)
            self.assertEqual(popen.call_count, 2)
            configure, compile_backend = [call.args[0] for call in popen.call_args_list]
            self.assertEqual(configure[0], 'cmake.exe')
            self.assertIn('Visual Studio 18 2026', configure)
            self.assertIn('-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain), configure)
            self.assertEqual(compile_backend,
                             ['cmake.exe', '--build', str(root / 'Backend/build'),
                              '--config', 'Release', '--target', 'DroneBackend'])

    def test_build_steps_use_current_project_and_fail_fast(self):
        with tempfile.TemporaryDirectory(prefix='drone 中文 build ') as folder:
            root = Path(folder)
            (root / 'Saved').mkdir()
            sentinel = root / 'Saved/user-plan.json'
            sentinel.write_text('preserve')
            vswhere = root / 'Microsoft Visual Studio/Installer/vswhere.exe'
            vswhere.parent.mkdir(parents=True)
            vswhere.touch()
            toolchain = root / 'vcpkg/scripts/buildsystems/vcpkg.cmake'
            toolchain.parent.mkdir(parents=True)
            toolchain.touch()
            config = {'ue_executable': str(root / 'UE/Engine/Binaries/Win64/UnrealEditor.exe'),
                      'project': 'UE5DroneControl.uproject'}
            for exit_code, expected_count in [(0, 3), (1, 1)]:
                with patch.object(bootstrap, 'ROOT', root), patch.dict(os.environ, {
                    'ProgramFiles(x86)': folder, 'UE5DRONE_VCPKG_TOOLCHAIN': str(toolchain)}), \
                    patch('bootstrap.subprocess.check_output', return_value=json.dumps([
                        {'installationPath': str(root / 'VS'), 'installationVersion': '17.14',
                         'displayName': 'Visual Studio 生成工具'}], ensure_ascii=False)) as discovery, \
                    patch('bootstrap.shutil.which', return_value='cmake.exe'), \
                    patch('bootstrap.subprocess.Popen', side_effect=lambda *a, **k: Mock(
                        stdout=io.BytesIO(b'build output\n'), wait=Mock(return_value=exit_code))) as popen:
                    if exit_code:
                        with self.assertRaisesRegex(RuntimeError, 'Build failed'):
                            bootstrap.build(config)
                    else:
                        bootstrap.build(config)
                    self.assertEqual(popen.call_count, expected_count)
                    self.assertIn('-utf8', discovery.call_args.args[0])
                    self.assertEqual(discovery.call_args.kwargs['encoding'], 'utf-8-sig')
                    self.assertIn(str(root / 'Backend/build'), popen.call_args_list[0].args[0])
                    self.assertEqual(sentinel.read_text(), 'preserve')


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.config = {'ue_executable': '', 'backend_executable': 'DroneBackend.exe'}

    def test_gui_opens_before_role_specific_preflight(self):
        with patch.object(bootstrap, 'ROOT', self.root), \
             patch.object(bootstrap, 'load_config', return_value=self.config) as load, \
             patch.object(bootstrap, 'preflight', side_effect=AssertionError('Choose role first')) as check, \
             patch.object(bootstrap.runpy, 'run_path') as run, \
             patch.object(bootstrap.sys, 'argv', ['bootstrap.py']):
            self.assertEqual(bootstrap.main(), 0)
        load.assert_called_once_with(self.root)
        check.assert_not_called()
        run.assert_called_once_with(str(self.root / 'Launcher/launcher.pyw'), run_name='__main__')
        self.assertFalse((self.root / 'Saved/environment-check.json').exists())

    def test_check_reports_missing_dependencies_without_opening_gui(self):
        with patch.object(bootstrap, 'ROOT', self.root), \
             patch.object(bootstrap, 'load_config', return_value=self.config), \
             patch.object(bootstrap, 'preflight', return_value=['UE missing']) as check, \
             patch.object(bootstrap.runpy, 'run_path') as run, \
             patch.object(bootstrap.sys, 'argv', ['bootstrap.py', '--check']), \
             patch('sys.stdout', new_callable=io.StringIO):
            self.assertEqual(bootstrap.main(), 1)
        check.assert_called_once_with(self.root, self.config, require_binaries=True)
        run.assert_not_called()
        report = json.loads((self.root / 'Saved/environment-check.json').read_text(encoding='utf-8'))
        self.assertEqual(report['issues'], ['UE missing'])

    def test_build_keeps_source_and_final_binary_checks(self):
        with patch.object(bootstrap, 'ROOT', self.root), \
             patch.object(bootstrap, 'load_config', return_value=self.config), \
             patch.object(bootstrap, 'preflight', return_value=[]) as check, \
             patch.object(bootstrap, 'build') as build, \
             patch.object(bootstrap.runpy, 'run_path') as run, \
             patch.object(bootstrap.sys, 'argv', ['bootstrap.py', '--build']), \
             patch('sys.stdout', new_callable=io.StringIO):
            self.assertEqual(bootstrap.main(), 0)
        self.assertEqual(check.call_count, 2)
        self.assertEqual(check.call_args_list[0].args, (self.root, self.config))
        self.assertEqual(check.call_args_list[0].kwargs, {'require_binaries': False})
        self.assertEqual(check.call_args_list[1].args, (self.root, self.config))
        self.assertEqual(check.call_args_list[1].kwargs, {})
        build.assert_called_once_with(self.config)
        run.assert_not_called()

    def test_build_backend_uses_role_specific_checks_without_ue_or_assets(self):
        config = {'backend_executable': 'Backend/build/Release/DroneBackend.exe',
                  'startup_mode': 'demo', 'realtime_backend_config': 'operator/backend.yaml'}
        backend_config = self.root / config['realtime_backend_config']
        backend_config.parent.mkdir()
        backend_config.write_text('server:\n  http_port: 8080\n', encoding='utf-8')

        def complete_backend_build(selected, backend_only=False):
            self.assertTrue(backend_only)
            target = self.root / selected['backend_executable']
            target.parent.mkdir(parents=True)
            target.touch()

        with patch.object(bootstrap, 'ROOT', self.root), \
             patch.object(bootstrap, 'load_config', return_value=config), \
             patch.object(bootstrap, 'preflight', wraps=bootstrap.preflight) as check, \
             patch.object(bootstrap, 'build', side_effect=complete_backend_build) as build, \
             patch.object(bootstrap.runpy, 'run_path') as run, \
             patch.object(bootstrap.sys, 'argv', ['bootstrap.py', '--build-backend']), \
             patch('sys.stdout', new_callable=io.StringIO):
            self.assertEqual(bootstrap.main(), 0)
        expected = {**config, 'deployment_role': 'Backend', 'launch_mode': 'realtime'}
        self.assertEqual(check.call_count, 2)
        self.assertEqual(check.call_args_list[0].args, (self.root, expected))
        self.assertEqual(check.call_args_list[0].kwargs, {'require_binaries': False})
        self.assertEqual(check.call_args_list[1].args, (self.root, expected))
        self.assertEqual(check.call_args_list[1].kwargs, {'require_binaries': True})
        build.assert_called_once_with(expected, backend_only=True)
        self.assertNotIn('deployment_role', config)
        self.assertNotIn('launch_mode', config)
        run.assert_not_called()

    def test_build_backend_requires_operator_config_before_building(self):
        with patch.object(bootstrap, 'ROOT', self.root), \
             patch.object(bootstrap, 'load_config', return_value=self.config), \
             patch.object(bootstrap, 'build') as build, \
             patch.object(bootstrap.sys, 'argv', ['bootstrap.py', '--build-backend']), \
             patch('sys.stdout', new_callable=io.StringIO):
            self.assertEqual(bootstrap.main(), 1)
        build.assert_not_called()
        report = json.loads((self.root / 'Saved/environment-check.json').read_text(encoding='utf-8'))
        self.assertEqual(len(report['issues']), 1)
        self.assertIn('Realtime Backend config missing', report['issues'][0])

    def test_build_backend_checks_resulting_binary(self):
        (self.root / 'Backend').mkdir()
        (self.root / 'Backend/config.yaml').touch()
        with patch.object(bootstrap, 'ROOT', self.root), \
             patch.object(bootstrap, 'load_config', return_value=self.config), \
             patch.object(bootstrap, 'build') as build, \
             patch.object(bootstrap.sys, 'argv', ['bootstrap.py', '--build-backend']), \
             patch('sys.stdout', new_callable=io.StringIO):
            with self.assertRaisesRegex(RuntimeError, 'Backend not built'):
                bootstrap.main()
        build.assert_called_once()


if __name__ == '__main__':
    unittest.main(verbosity=2)
