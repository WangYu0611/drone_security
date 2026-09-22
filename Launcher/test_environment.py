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


if __name__ == '__main__':
    unittest.main(verbosity=2)
