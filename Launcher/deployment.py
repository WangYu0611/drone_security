"""Read-only shared LAN profiles and a workstation's selected role.

Loading a profile never opens a socket or changes flight configuration. Local
address validation is a separate, bind-only operation performed before launch.
"""
from __future__ import annotations

import ipaddress
import json
from pathlib import Path
import socket

from launch_modes import mode_config


DEPLOYMENT_ROLES = ('Backend', 'Command', 'Map', 'Video')
PROFILE_FIELDS = frozenset(('schema_version', 'mode', 'hosts', 'http_port', 'ws_port'))
SELECTION_PATH = Path('Saved/Launcher/deployment-selection.json')


def _role(role):
    if role not in DEPLOYMENT_ROLES:
        raise ValueError('Choose Backend, Command, Map or Video / 请选择本机角色。')
    return role


def _profile_path(root, path):
    if not isinstance(path, (str, Path)) or not str(path).strip():
        raise ValueError('Select a shared deployment JSON / 请选择共享部署 JSON。')
    return (Path(root) / path).resolve()


def _unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate deployment field / 部署字段重复: ' + key)
        result[key] = value
    return result


def _exact_fields(value, expected, label):
    if not isinstance(value, dict):
        raise ValueError(label + ' must be an object / 必须为 JSON 对象。')
    missing, unknown = expected - value.keys(), value.keys() - expected
    if missing or unknown:
        raise ValueError(f'{label}: missing / 缺少 {sorted(missing)}; '
                         f'unknown / 未知字段 {sorted(unknown)}')


def _host(role, value):
    try:
        if not isinstance(value, str):
            raise ValueError()
        address = ipaddress.IPv4Address(value)
        # 0/8, loopback, multicast and reserved/broadcast addresses are not
        # usable host assignments for this four-workstation deployment.
        if (int(address) >> 24 == 0 or address.is_loopback or
                address.is_multicast or address.is_reserved):
            raise ValueError()
    except (ipaddress.AddressValueError, ValueError):
        raise ValueError(f'{role} requires a unicast IPv4 address / '
                         f'{role} 必须填写有效的非回环 IPv4 单播地址。') from None
    return str(address)


def deployment_config(root, base_config, path, role):
    """Derive one role's settings without writing or contacting any host."""
    role = _role(role)
    profile_path = _profile_path(root, path)
    try:
        profile = json.loads(profile_path.read_text(encoding='utf-8-sig'),
                             object_pairs_hook=_unique_keys)
    except OSError as exc:
        raise ValueError(f'Cannot read deployment JSON / 无法读取部署文件: {profile_path}') from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f'Invalid deployment JSON / 部署 JSON 格式错误: {profile_path}') from exc
    _exact_fields(profile, PROFILE_FIELDS, 'Deployment / 部署配置')
    if type(profile['schema_version']) is not int or profile['schema_version'] != 1:
        raise ValueError('Unsupported deployment schema_version / 部署 schema_version 必须为 1。')
    if profile['mode'] not in ('realtime', 'demo'):
        raise ValueError('Deployment mode must be realtime or demo / 部署 mode 必须为 realtime 或 demo。')
    _exact_fields(profile['hosts'], frozenset(DEPLOYMENT_ROLES), 'hosts')
    hosts = {name: _host(name, profile['hosts'][name]) for name in DEPLOYMENT_ROLES}
    if len(set(hosts.values())) != len(DEPLOYMENT_ROLES):
        raise ValueError('Each role needs a distinct IPv4 address / 四个角色必须使用不同的 IPv4 地址。')
    for key in ('http_port', 'ws_port'):
        if type(profile[key]) is not int or not 1 <= profile[key] <= 65535:
            raise ValueError(f'{key} must be an integer from 1 to 65535 / {key} 必须是 1–65535 的整数。')
    if profile['http_port'] == profile['ws_port']:
        raise ValueError('HTTP and WebSocket ports must differ / HTTP 与 WebSocket 端口必须不同。')

    http_endpoint = f"http://{hosts['Backend']}:{profile['http_port']}"
    ws_endpoint = f"ws://{hosts['Backend']}:{profile['ws_port']}/ws"
    # The shared profile owns network settings and deployment state directories.
    # Legacy per-workstation endpoints may be stale or invalid; they must not
    # block a valid profile, even before mode_config applies its mode rules.
    mode_base = dict(base_config)
    mode_base.update({
        'http_endpoint': http_endpoint,
        'ws_endpoint': ws_endpoint,
        'runtime_directory': f'Saved/Deployment/demo/{role}',
        'realtime_http_endpoint': http_endpoint,
        'realtime_ws_endpoint': ws_endpoint,
        'realtime_runtime_directory': f'Saved/Deployment/realtime/{role}',
    })
    # One shared profile selects one mode. Backend compatibility checks still
    # prevent attaching to the wrong mode; single-machine isolation is unrelated.
    result = mode_config(root, mode_base, profile['mode'], validate_isolation=False)
    result.update({
        'http_endpoint': http_endpoint,
        'ws_endpoint': ws_endpoint,
        'runtime_directory': f"Saved/Deployment/{profile['mode']}/{role}",
        'deployment_role': role,
        'deployment_path': str(profile_path),
        'deployment_hosts': hosts,
        'deployment_http_port': profile['http_port'],
        'deployment_ws_port': profile['ws_port'],
        'launch_mode': profile['mode'],
    })
    return result


def validate_local_role(config):
    """Check the selected IPv4 belongs locally; bind port 0, never send data."""
    role = _role(config.get('deployment_role'))
    address = _host(role, config.get('deployment_hosts', {}).get(role))
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind((address, 0))
    except OSError as exc:
        raise RuntimeError(f'{role}: {address} is not available on this computer / '
                           f'本机无法绑定角色 {role} 的 IP {address}；请检查角色选择和网卡地址。') from exc


def load_deployment_selection(root):
    """Read the local choice; missing/damaged choices mean no selected role."""
    empty = {'role': None, 'path': None}
    try:
        choice = json.loads((Path(root) / SELECTION_PATH).read_text(encoding='utf-8-sig'),
                            object_pairs_hook=_unique_keys)
        if not isinstance(choice, dict) or set(choice) != {'role', 'path'}:
            return empty
        if choice['role'] is None and choice['path'] is None:
            return empty
        return {'role': _role(choice['role']), 'path': str(_profile_path(root, choice['path']))}
    except (OSError, UnicodeError, ValueError, TypeError):
        return empty


def save_deployment_selection(root, role, path):
    """Persist only role and shared-file path; never copy hosts or edit profile."""
    choice = ({'role': None, 'path': None} if role is None else
              {'role': _role(role), 'path': str(_profile_path(root, path))})
    target = Path(root) / SELECTION_PATH
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix('.tmp')
    temporary.write_text(json.dumps(choice, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(target)
