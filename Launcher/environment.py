"""Workstation discovery and actionable checks; never installs or deletes data."""
from __future__ import annotations
import json
import os
from pathlib import Path

from deployment import DEPLOYMENT_SELECTIONS, PILOT_ROLE, deployment_roles

EDITOR = Path('Engine/Binaries/Win64/UnrealEditor.exe')
PILOT_IDENTITIES = {'Command': 'pilot_control_command_identity', PILOT_ROLE: 'pilot_control_pilot_identity'}
PILOT_CREDENTIALS_ENV = 'DRONE_PILOT_CONTROL_CREDENTIALS'


def pilot_credentials_path():
    configured_path = os.environ.get(PILOT_CREDENTIALS_ENV)
    path = Path(configured_path) if configured_path else None
    if path is None or not path.is_absolute() or not path.is_file():
        raise RuntimeError('Set DRONE_PILOT_CONTROL_CREDENTIALS to an existing absolute file path / '
                           '请将 DRONE_PILOT_CONTROL_CREDENTIALS 设置为现有凭据文件的绝对路径。')
    return path


def shared_pilot_roles(config):
    selection = config.get('deployment_role')
    hosts = config.get('deployment_hosts', {})
    if selection is None or config.get('launch_mode') != 'demo' or PILOT_ROLE not in hosts:
        return ()
    roles = deployment_roles(selection, hosts, config.get('deployment_local_roles'))
    return tuple(role for role in roles if role == 'Backend' or role in PILOT_IDENTITIES)


def validate_shared_pilot_credentials(config):
    local_roles = shared_pilot_roles(config)
    if not local_roles:
        return
    local_identities = {}
    for role in local_roles:
        if role not in PILOT_IDENTITIES:
            continue
        key = PILOT_IDENTITIES[role]
        value = config.get(key)
        if not isinstance(value, str) or not value.strip():
            raise RuntimeError(f'Set {key} in Launcher/config.local.json / '
                               f'请在 Launcher/config.local.json 设置 {key}。')
        local_identities[role] = value
    path = pilot_credentials_path()
    try:
        document = json.loads(path.read_text(encoding='utf-8'))
        credentials = document.get('credentials') if isinstance(document, dict) else None
        if not isinstance(credentials, list) or not credentials:
            raise ValueError()
        actors = set()
        tokens = set()
        for entry in credentials:
            if not isinstance(entry, dict):
                raise ValueError()
            role, identity, token = (entry.get(key) for key in ('role', 'id', 'token'))
            if (role not in PILOT_IDENTITIES or not isinstance(identity, str)
                    or not identity or len(identity.encode('utf-8')) > 128
                    or not isinstance(token, str) or not token or len(token.encode('utf-8')) > 512
                    or any(character in token for character in ' \t\r\n')
                    or (role, identity) in actors or token in tokens):
                raise ValueError()
            actors.add((role, identity))
            tokens.add(token)
    except (OSError, ValueError, TypeError):
        raise RuntimeError('Invalid Pilot control credential file / 驾驶员控制权凭据文件无效。') from None
    for role, identity in local_identities.items():
        if (role, identity) not in actors:
            raise RuntimeError(f'{role} identity has no matching entry in the credential file / '
                               f'凭据文件中没有匹配的 {role} 身份。')


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
    config.setdefault('mock_reset_on_start', False)
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
    selection = config.get('deployment_role')
    if 'deployment_role' in config and selection not in DEPLOYMENT_SELECTIONS:
        return ['Invalid deployment role / 部署角色必须为 Auto、All、Clients、Backend、Command、Map、Video 或 Pilot。']
    roles = deployment_roles(selection, config.get('deployment_hosts', {}),
                             config.get('deployment_local_roles')) if selection is not None else ()
    if PILOT_ROLE in roles and config.get('launch_mode') != 'demo':
        return ['Pilot requires a Mock deployment / Pilot 仅支持 DEMO 部署。']
    try:
        validate_shared_pilot_credentials(config)
    except RuntimeError as error:
        issues.append(str(error))
    if 'Backend' in roles:
        if require_binaries and not (root / config['backend_executable']).is_file():
            issues.append('Backend not built / 尚未编译后端。先运行 SetupDroneBackend.cmd。')
        # DEMO generates its own configuration. A real Backend must use the
        # operator-selected configuration even on machines without UE assets.
        if config.get('launch_mode', 'realtime') == 'realtime':
            backend_config = (config.get('backend_config') or config.get('realtime_backend_config')
                              or 'Backend/config.yaml')
            if not (root / backend_config).is_file():
                issues.append('Realtime Backend config missing / 缺少实时后端配置: ' + str(root / backend_config))
        if roles == ('Backend',):
            return issues
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
    if not roles or any(role not in ('Backend', PILOT_ROLE) for role in roles):
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
        if selection is None and not (root / config['backend_executable']).is_file():
            issues.append('Backend not built / 尚未编译后端。先运行 SetupDroneSecurity.cmd。')
        if not (root / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll').is_file():
            issues.append('UE module not built / 尚未编译 UE 工程。先运行 SetupDroneSecurity.cmd。')
    return issues
