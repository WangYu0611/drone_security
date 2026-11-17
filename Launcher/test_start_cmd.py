"""Exercise the Windows entry script with real Python probes, never launch UI."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


@unittest.skipUnless(os.name == 'nt', 'Windows command entry point')
class StartCommandTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix='drone launcher entry ')
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.launcher = self.root / 'Launcher with spaces'
        self.launcher.mkdir()
        self.start = self.launcher / 'Start.cmd'
        shutil.copyfile(Path(__file__).with_name('Start.cmd'), self.start)
        self.choice = self.launcher / 'python.path'
        self.calls = self.root / 'calls.jsonl'
        # The probe still imports the actual Tcl/Tk module and checks Python.
        # Only the normal application body is replaced, so no UI can open.
        (self.launcher / 'bootstrap.py').write_text('''\
import json, os, sys
from pathlib import Path
with Path(os.environ['DRONE_ENTRY_TEST_CALLS']).open('a', encoding='utf-8') as log:
    log.write(json.dumps({'args': sys.argv[1:], 'executable': sys.executable}) + '\\n')
if '--probe' in sys.argv:
    import tkinter
    sys.exit(int(os.environ.get('DRONE_ENTRY_TEST_PROBE_EXIT', '0'))
             if sys.version_info >= (3, 10) else 1)
sys.exit(int(os.environ.get('DRONE_ENTRY_TEST_EXIT', '0')))
''', encoding='utf-8')
        self.env = dict(os.environ)
        system = Path(os.environ['SystemRoot']) / 'System32'
        self.command = str(system / 'cmd.exe')
        self.env['PATH'] = str(Path(sys.executable).parent) + os.pathsep + str(system)
        self.env['DRONE_ENTRY_TEST_CALLS'] = str(self.calls)

    def run_entry(self, *args):
        # A wrapper keeps Start.cmd's absolute path and arguments correctly
        # quoted even when the temporary workspace has spaces.
        wrapper = self.root / 'invoke.cmd'
        command = subprocess.list2cmdline([str(self.start), *args])
        wrapper.write_text('@echo off\ncall ' + command + '\nexit /b %ERRORLEVEL%\n',
                           encoding='utf-8', newline='\r\n')
        result = subprocess.run([self.command, '/d', '/c', str(wrapper)],
                                cwd=self.root, env=self.env, input='\n' * 6,
                                capture_output=True, text=True, timeout=15)
        calls = ([json.loads(line) for line in self.calls.read_text(encoding='utf-8').splitlines()]
                 if self.calls.exists() else [])
        return result, calls

    def configure(self, value):
        self.choice.write_text(value + '\n', encoding='utf-8', newline='\r\n')

    def test_explicit_python_works_without_discovery_and_preserves_arguments_and_exit(self):
        self.configure(sys.executable)
        self.env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
        self.env['DRONE_ENTRY_TEST_EXIT'] = '23'
        result, calls = self.run_entry('--profile', 'shared config.json')
        self.assertEqual(result.returncode, 23, result.stdout + result.stderr)
        self.assertEqual([call['args'] for call in calls],
                         [['--probe'], ['--profile', 'shared config.json']])
        self.assertEqual(Path(calls[-1]['executable']), Path(sys.executable))

    def test_invalid_explicit_choice_does_not_fall_back_to_working_path_python(self):
        for value in ('', 'python.exe', str(self.root / 'missing python.exe'),
                      '"' + sys.executable + '"'):
            with self.subTest(value=value):
                self.configure(value)
                result, calls = self.run_entry('--probe')
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn('Launcher/python.path is invalid', result.stdout)
                self.assertEqual(calls, [])

    def test_explicit_python_probe_failure_does_not_fall_back(self):
        self.configure(sys.executable)
        self.env['DRONE_ENTRY_TEST_PROBE_EXIT'] = '9'
        result, calls = self.run_entry('--probe')
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('its Python probe failed', result.stdout)
        self.assertEqual([call['args'] for call in calls], [['--probe']])

    def test_unconfigured_entry_retains_path_discovery(self):
        result, calls = self.run_entry('--probe')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual([call['args'] for call in calls], [['--probe'], ['--probe']])

    def test_python_path_with_spaces_and_punctuation_is_quoted(self):
        # A directory junction references the already installed interpreter;
        # no executable is copied or newly installed for this test.
        linked = self.root / 'Python runtime & tools!'
        result = subprocess.run([self.command, '/d', '/c', 'mklink', '/J', str(linked),
                                 str(Path(sys.executable).parent)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.addCleanup(linked.rmdir)
        self.configure(str(linked / Path(sys.executable).name))
        result, calls = self.run_entry('--probe')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual([call['args'] for call in calls], [['--probe'], ['--probe']])


if __name__ == '__main__':
    unittest.main(verbosity=2)
