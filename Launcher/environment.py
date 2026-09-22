"""Workstation discovery and actionable checks; never installs or deletes data."""
from __future__ import annotations
import json
import os
from pathlib import Path

EDITOR = Path('Engine/Binaries/Win64/UnrealEditor.exe')


def has_cesium(folder):
    for _, directories, files in os.walk(folder):
        if 'CesiumForUnreal.uplugin' in files:
            return True
        # Plugin descriptors live above these very large asset/build trees.
        directories[:] = [d for d in directories if d not in
                          ('Content', 'Binaries', 'Intermediate', 'Source', 'Resources', '.git')]
    return False


def engine_candidates():
    if os.environ.get('UE5DRONE_UE_ROOT'):
        yield Path(os.environ['UE5DRONE_UE_ROOT'])
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\EpicGames\Unreal Engine\5.8') as key:
            yield Path(winreg.QueryValueEx(key, 'InstalledDirectory')[0])
    except (ImportError, OSError):
        pass
    manifest = Path(os.environ.get('PROGRAMDATA', 'C:/ProgramData')) / 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
    if manifest.is_file():
        for item in json.loads(manifest.read_text(encoding='utf-8-sig')).get('InstallationList', []):
            if item.get('AppName') == 'UE_5.8':
                yield Path(item['InstallLocation'])
    yield Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Epic Games/UE_5.8'


def load_config(root):
    root = Path(root)
    config = json.loads((root / 'Launcher/config.json').read_text(encoding='utf-8-sig'))
    local = root / 'Launcher/config.local.json'
    overrides = json.loads(local.read_text(encoding='utf-8-sig')) if local.exists() else {}
    config.update(overrides)
    if not config.get('ue_executable'):
        config['ue_executable'] = next((str(p / EDITOR) for p in engine_candidates() if (p / EDITOR).is_file()), '')
    # Legacy directory is only a compatibility fallback for existing P5.3 users.
    if 'backend_executable' not in overrides and not (root / config['backend_executable']).is_file():
        legacy = root / 'Backend/build-p53/Release/DroneBackend.exe'
        if legacy.is_file():
            config['backend_executable'] = str(legacy)
    return config


def preflight(root, config, require_binaries=True):
    root = Path(root)
    issues = []
    project = root / config['project']
    if not project.is_file():
        issues.append('Missing UE5DroneControl.uproject / 缺少工程文件。请完整下载项目。')
    editor = Path(config.get('ue_executable') or '__missing_engine__')
    if not editor.is_file():
        issues.append('UE 5.8 not found / 未找到引擎。设置 UE5DRONE_UE_ROOT 或 Launcher/config.local.json 中的 ue_executable。')
    else:
        engine = editor.parents[3]
        version = engine / 'Engine/Build/Build.version'
        if not version.is_file():
            issues.append('Invalid UE installation / 引擎缺少 Build.version。')
        else:
            value = json.loads(version.read_text(encoding='utf-8-sig'))
            if (value.get('MajorVersion'), value.get('MinorVersion')) != (5, 8):
                issues.append('UE 5.8 is required / 请使用 UE 5.8。')
        if not (has_cesium(root / 'Plugins') or has_cesium(engine / 'Engine/Plugins')):
            issues.append('Cesium for Unreal missing / 请在 Epic Games Launcher 的 Fab 库中将 Cesium for Unreal 安装到 UE 5.8。见 Docs/First-Run-Windows.md。')
    missing_assets = []
    for folder in ('Content', 'Plugins'):
        for path in (root / folder).rglob('*'):
            if path.suffix.lower() not in ('.uasset', '.umap') or not path.is_file():
                continue
            with path.open('rb') as source:
                if source.read(80).startswith(b'version https://git-lfs.github.com/spec/v1'):
                    missing_assets.append(str(path.relative_to(root)))
    if missing_assets:
        issues.append(f'Git LFS assets missing / {len(missing_assets)} 个资源仍为占位文件。运行 git lfs install 和 git lfs pull。示例: {missing_assets[0]}')
    map_file = root / ('Content/' + config['map'].removeprefix('/Game/') + '.umap')
    if not map_file.is_file():
        issues.append('Map asset missing / 缺少地图资源: ' + str(map_file))
    if require_binaries:
        if not (root / config['backend_executable']).is_file():
            issues.append('Backend not built / 尚未编译后端。先运行 SetupDroneSecurity.cmd。')
        if not (root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll').is_file():
            issues.append('UE module not built / 尚未编译 UE 工程。先运行 SetupDroneSecurity.cmd。')
    return issues
