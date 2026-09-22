"""Console first-run entry point with a retained log and checked build steps."""
from pathlib import Path
import json
import os
import runpy
import shutil
import subprocess
import sys
from environment import load_config, preflight

ROOT = Path(__file__).resolve().parents[1]


def build(config):
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        raise RuntimeError('Install Visual Studio with Desktop C++ and Game development with C++ workloads.')
    installations = json.loads(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json'], encoding='utf-8-sig'))
    if not installations:
        raise RuntimeError('Visual Studio C++ tools missing. Modify installation: Desktop C++ / Game development with C++.')
    vs = Path(installations[0]['installationPath'])
    cmake = shutil.which('cmake') or str(vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
    toolchain = os.environ.get('UE5DRONE_VCPKG_TOOLCHAIN')
    if not toolchain:
        vcpkg = Path(os.environ.get('VCPKG_ROOT', str(vs / 'VC/vcpkg')))
        toolchain = str(vcpkg / 'scripts/buildsystems/vcpkg.cmake')
    if not Path(toolchain).is_file():
        raise RuntimeError('vcpkg missing. Install the VS vcpkg component or set VCPKG_ROOT. See Docs/First-Run-Windows.md.')
    if not shutil.which(cmake) and not Path(cmake).is_file():
        raise RuntimeError('CMake missing. Install C++ CMake tools for Windows in Visual Studio Installer.')
    major = int(installations[0]['installationVersion'].split('.')[0])
    generators = {17: 'Visual Studio 17 2022', 18: 'Visual Studio 18 2026'}
    if major not in generators:
        raise RuntimeError('Unsupported Visual Studio version; use VS 2022 or 2026.')
    target = ROOT / 'Backend/build'
    steps = [
        [cmake, '-S', str(ROOT / 'Backend'), '-B', str(target), '-G', generators[major], '-A', 'x64',
         '-DCMAKE_TOOLCHAIN_FILE=' + toolchain, '-DVCPKG_TARGET_TRIPLET=x64-windows'],
        [cmake, '--build', str(target), '--config', 'Release', '--target', 'DroneBackend'],
        [str(Path(config['ue_executable']).parents[3] / 'Engine/Build/BatchFiles/Build.bat'),
         'UE5DroneControlEditor', 'Win64', 'Development', '-Project=' + str(ROOT / config['project']),
         '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBTMakefiles', '-NoUBA']]
    # Keep existing caches and data. A mismatched CMake cache fails with a visible error.
    with (ROOT / 'Saved/first-run-build.log').open('a', encoding='utf-8') as log:
        for args in steps:
            print('Build:', subprocess.list2cmdline(args), flush=True)
            process = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            for line in iter(process.stdout.readline, b''):
                value = line.decode('utf-8', errors='replace')
                print(value, end='', flush=True)
                log.write(value)
            if process.wait():
                raise RuntimeError('Build failed. See Saved/first-run-build.log; no project data was deleted.')


def main():
    (ROOT / 'Saved').mkdir(exist_ok=True)
    config = load_config(ROOT)
    building = '--build' in sys.argv
    issues = preflight(ROOT, config, require_binaries=not building)
    report = {'ue_executable': config['ue_executable'], 'backend_executable': config['backend_executable'], 'issues': issues}
    (ROOT / 'Saved/environment-check.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    if issues:
        print('\n\n'.join(issues))
        print('\nHelp: Docs/First-Run-Windows.md | Report: Saved/environment-check.json')
        return 1
    if building:
        build(config)
        issues = preflight(ROOT, load_config(ROOT))
        if issues:
            raise RuntimeError('\n'.join(issues))
        print('Build complete. Run DroneSecurityLauncher.cmd.')
    elif '--check' in sys.argv:
        print('Environment checks passed. Cesium ion token/network access must be configured in Unreal Editor.')
    else:
        runpy.run_path(str(ROOT / 'Launcher/launcher.pyw'), run_name='__main__')
    return 0


if __name__ == '__main__':
    if '--probe' in sys.argv:
        import tkinter
        sys.exit(0 if sys.version_info >= (3, 10) else 1)
    try:
        sys.exit(main())
    except Exception as exc:
        print('ERROR:', exc)
        sys.exit(1)
