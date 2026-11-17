import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from environment import load_config, preflight, EDITOR


class EnvironmentTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name) / 'project with spaces'
        self.engine = Path(self.folder.name) / 'custom engine'
        self.config = {'ue_executable': '', 'backend_executable': 'Backend/build/Release/DroneBackend.exe',
                       'project': 'UE5DroneControl.uproject', 'map': '/Game/Level/CesiumWorld'}
        self.put('Launcher/config.json', json.dumps(self.config))
        self.put(self.config['project'], '{}')
        self.put('Content/Level/CesiumWorld.umap', 'asset')
        self.put(self.config['backend_executable'], 'binary')
        self.put('Binaries/Win64/UnrealEditor-UE5DroneControl.dll', 'binary')
        for path, value in [(EDITOR, ''), (Path('Engine/Build/Build.version'), '{"MajorVersion":5,"MinorVersion":8}'),
                            (Path('Engine/Plugins/Marketplace/GeneratedName/CesiumForUnreal.uplugin'), '{}')]:
            target = self.engine / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(value)

    def put(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8')

    def config_for_test(self):
        with patch('environment.engine_candidates', return_value=[self.engine]):
            return load_config(self.root)

    def test_custom_engine_path_and_generated_plugin_folder(self):
        self.assertEqual(preflight(self.root, self.config_for_test()), [])
        self.assertFalse(self.config_for_test()['mock_reset_on_start'])

    def test_startup_reset_is_an_explicit_config_choice(self):
        self.config['mock_reset_on_start'] = True
        self.put('Launcher/config.json', json.dumps(self.config))
        self.assertTrue(self.config_for_test()['mock_reset_on_start'])
        self.put('Launcher/config.local.json', '{"mock_reset_on_start": false}')
        self.assertFalse(self.config_for_test()['mock_reset_on_start'])

    def test_missing_cesium_is_actionable(self):
        next(self.engine.rglob('*.uplugin')).unlink()
        self.assertIn('Cesium for Unreal missing', '\n'.join(preflight(self.root, self.config_for_test())))

    def test_lfs_placeholder_blocks_launch(self):
        self.put('Content/Level/CesiumWorld.umap', 'version https://git-lfs.github.com/spec/v1\noid sha256:123')
        self.assertIn('git lfs pull', '\n'.join(preflight(self.root, self.config_for_test())))

    def test_source_download_can_build_but_not_launch(self):
        (self.root / self.config['backend_executable']).unlink()
        (self.root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll').unlink()
        config = self.config_for_test()
        self.assertEqual(len(preflight(self.root, config)), 2)
        self.assertEqual(preflight(self.root, config, require_binaries=False), [])

    def test_explicit_override_is_not_silently_replaced(self):
        self.put('Launcher/config.local.json', json.dumps({'ue_executable': 'Z:/missing/UnrealEditor.exe',
                                                         'backend_executable': 'missing.exe'}))
        self.put('Backend/build-p53/Release/DroneBackend.exe', 'binary')
        config = self.config_for_test()
        self.assertEqual(config['backend_executable'], 'missing.exe')
        self.assertIn('UE 5.8 not found', '\n'.join(preflight(self.root, config)))

    def test_wrong_engine_version(self):
        (self.engine / 'Engine/Build/Build.version').write_text('{"MajorVersion":5,"MinorVersion":7}')
        self.assertIn('UE 5.8 is required', '\n'.join(preflight(self.root, self.config_for_test())))

    def test_existing_p53_compatibility(self):
        (self.root / self.config['backend_executable']).unlink()
        self.put('Backend/build-p53/Release/DroneBackend.exe', 'binary')
        self.assertEqual(preflight(self.root, self.config_for_test()), [])

    def test_backend_only_preflight_accepts_lightweight_directory_without_ue(self):
        root = Path(self.folder.name) / 'backend only'
        (root / 'Backend').mkdir(parents=True)
        (root / 'DroneBackend.exe').write_text('binary')
        (root / 'Backend/config.yaml').write_text('http: {}')
        config = {'deployment_role': 'Backend', 'backend_executable': 'DroneBackend.exe',
                  'launch_mode': 'realtime'}
        with patch('environment.has_cesium', side_effect=AssertionError('UE checks must be skipped')):
            self.assertEqual(preflight(root, config), [])

    def test_backend_only_requires_binary_and_realtime_configuration(self):
        root = Path(self.folder.name) / 'empty backend'
        config = {'deployment_role': 'Backend', 'backend_executable': 'DroneBackend.exe'}
        issues = '\n'.join(preflight(root, config))
        self.assertIn('Backend not built', issues)
        self.assertIn('Realtime Backend config missing', issues)
        self.assertNotIn('UE 5.8', issues)
        self.assertEqual(len(preflight(root, config, require_binaries=False)), 1)

    def test_backend_config_prefers_resolved_path_then_realtime_override(self):
        self.put('configuration/real.yaml', 'http: {}')
        config = {'deployment_role': 'Backend',
                  'backend_executable': self.config['backend_executable'],
                  'realtime_backend_config': 'configuration/real.yaml'}
        self.assertEqual(preflight(self.root, config), [])
        config['backend_config'] = 'configuration/missing.yaml'
        self.assertIn('configuration/missing.yaml', '\n'.join(preflight(self.root, config)).replace('\\', '/'))
        self.put('configuration/resolved.yaml', 'http: {}')
        config['backend_config'] = 'configuration/resolved.yaml'
        config['realtime_backend_config'] = 'configuration/missing.yaml'
        self.assertEqual(preflight(self.root, config), [])

    def test_demo_backend_uses_generated_config_without_realtime_yaml(self):
        config = {'deployment_role': 'Backend', 'launch_mode': 'demo',
                  'backend_executable': self.config['backend_executable']}
        self.assertEqual(preflight(self.root, config), [])

    def test_ui_roles_do_not_require_backend_binary_or_configuration(self):
        (self.root / self.config['backend_executable']).unlink()
        config = self.config_for_test()
        config.pop('backend_executable')
        config['backend_config'] = 'missing.yaml'
        for role in ('Command', 'Map', 'Video'):
            with self.subTest(role=role):
                self.assertEqual(preflight(self.root, {**config, 'deployment_role': role}), [])

    def test_ui_roles_still_require_ue_assets_cesium_and_module(self):
        next(self.engine.rglob('*.uplugin')).unlink()
        self.put('Content/Level/CesiumWorld.umap', 'version https://git-lfs.github.com/spec/v1\noid sha256:123')
        (self.root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll').unlink()
        for role in ('Command', 'Map', 'Video'):
            with self.subTest(role=role):
                issues = '\n'.join(preflight(self.root, {**self.config_for_test(), 'deployment_role': role}))
                self.assertIn('Cesium for Unreal missing', issues)
                self.assertIn('git lfs pull', issues)
                self.assertIn('UE module not built', issues)

    def test_invalid_explicit_deployment_role_is_rejected_before_other_checks(self):
        for role in ('All', 'backend', '', None, ['Backend']):
            with self.subTest(role=role):
                issues = preflight(self.root, {'deployment_role': role})
                self.assertEqual(len(issues), 1)
                self.assertIn('Invalid deployment role', issues[0])

    def test_unspecified_role_keeps_all_components_required(self):
        (self.root / self.config['backend_executable']).unlink()
        issues = '\n'.join(preflight(self.root, self.config_for_test()))
        self.assertIn('Backend not built', issues)


if __name__ == '__main__':
    unittest.main(verbosity=2)
