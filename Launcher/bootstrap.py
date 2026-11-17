"""Console first-run entry point with a retained log and checked build steps."""
from pathlib import Path
import json
import os
import runpy
import shutil
import subprocess
import sys
import traceback
from environment import load_config, preflight

ROOT = Path(__file__).resolve().parents[1]


def build(config, backend_only=False):
    workloads = 'Desktop C++' if backend_only else 'Desktop C++ and Game development with C++'
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        raise RuntimeError('Install Visual Studio with ' + workloads + ' workloads.')
    installations = json.loads(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-format', 'json', '-utf8'], encoding='utf-8-sig'))
    if not installations:
        raise RuntimeError('Visual Studio C++ tools missing. Modify installation: ' + workloads + '.')
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
        [cmake, '--build', str(target), '--config', 'Release', '--target', 'DroneBackend']]
    if not backend_only:
        steps.append([str(Path(config['ue_executable']).parents[3] / 'Engine/Build/BatchFiles/Build.bat'),
         'UE5DroneControlEditor', 'Win64', 'Development', '-Project=' + str(ROOT / config['project']),
         '-WaitMutex', '-NoHotReloadFromIDE', '-NoUBTMakefiles', '-NoUBA'])
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
    backend_only = '--build-backend' in sys.argv
    building = '--build' in sys.argv or backend_only
    if not building and '--check' not in sys.argv:
        # The GUI selects this workstation's deployment role before checking
        # its dependencies. A Backend-only machine does not require UE.
        runpy.run_path(str(ROOT / 'Launcher/launcher.pyw'), run_name='__main__')
        return 0
    if backend_only:
        config = {**config, 'deployment_role': 'Backend', 'launch_mode': 'realtime'}
    issues = preflight(ROOT, config, require_binaries=not building)
    report = {'ue_executable': config.get('ue_executable', ''), 'backend_executable': config['backend_executable'], 'issues': issues}
    (ROOT / 'Saved/environment-check.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    if issues:
        print('\n\n'.join(issues))
        print('\nHelp: Docs/First-Run-Windows.md | Report: Saved/environment-check.json')
        return 1
    if building:
        if backend_only:
            build(config, backend_only=True)
            final_config = {**load_config(ROOT), 'deployment_role': 'Backend', 'launch_mode': 'realtime'}
            issues = preflight(ROOT, final_config, require_binaries=True)
        else:
            build(config)
            issues = preflight(ROOT, load_config(ROOT))
        if issues:
            raise RuntimeError('\n'.join(issues))
        print(('Backend build complete. Run DroneSecurityLauncher.cmd and select the Backend role.' if backend_only else
               'Build complete. Run DroneSecurityLauncher.cmd.'))
    elif '--check' in sys.argv:
        print('Environment checks passed. Cesium ion token/network access must be configured in Unreal Editor.')
    return 0


if __name__ == '__main__':
    if '--probe' in sys.argv:
        import tkinter
        sys.exit(0 if sys.version_info >= (3, 10) else 1)
    try:
        sys.exit(main())
    except Exception as exc:
        print('ERROR:', exc)
        try:
            (ROOT / 'Saved').mkdir(exist_ok=True)
            with (ROOT / 'Saved/first-run-error.log').open('a', encoding='utf-8') as log:
                traceback.print_exc(file=log)
            print('Details: Saved/first-run-error.log')
        except OSError:
            pass
        sys.exit(1)
