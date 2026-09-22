import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock
import bootstrap


class BuildTests(unittest.TestCase):
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


if __name__ == '__main__':
    unittest.main(verbosity=2)
