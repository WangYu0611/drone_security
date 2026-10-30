"""Launch profiles and received-GPS selection; no writes or flight commands."""
from __future__ import annotations

import configparser
import copy
import json
import math
from pathlib import Path
import time
from urllib.parse import urlsplit

MODES = ('demo', 'realtime')


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def valid_origin(origin):
    return (isinstance(origin, dict)
            and all(number(origin.get(k)) for k in ('longitude', 'latitude', 'altitude'))
            and abs(origin['longitude']) <= 180
            and abs(origin['latitude']) <= 85.05112878
            and -1000 <= origin['altitude'] <= 100000)


def mode_config(root, config, mode):
    if mode not in MODES:
        raise ValueError('Unknown launch mode / 未知启动模式: ' + str(mode))
    result = copy.deepcopy(config)
    result['launch_mode'] = mode
    if mode == 'demo':
        # DEMO always uses the shipped Singapore fixture, independently of
        # historical QA fixtures and the realtime Backend's existing files.
        result['mock_fixture'] = 'Backend/mock_execution.json'
        fixture = json.loads((Path(root) / result['mock_fixture']).read_text(encoding='utf-8-sig'))
        origin = fixture['startup_formation']['origin']
        if not valid_origin(origin):
            raise ValueError('Invalid DEMO origin / DEMO 初始化位置无效')
        result['startup_origin'] = dict(origin)
    else:
        result.update({
            'http_endpoint': config.get('realtime_http_endpoint', 'http://127.0.0.1:8080'),
            'ws_endpoint': config.get('realtime_ws_endpoint', 'ws://127.0.0.1:8081/ws'),
            'runtime_directory': config.get('realtime_runtime_directory', 'Saved/Realtime'),
            'backend_config': config.get('realtime_backend_config', 'Backend/config.yaml'),
            'mock_reset_on_start': False,
        })
        result.pop('startup_origin', None)
        if ((Path(root) / result['runtime_directory']).resolve()
                == (Path(root) / config['runtime_directory']).resolve()):
            raise ValueError('Modes require separate runtime directories / 两种模式必须使用不同数据目录')
        def address(endpoint):
            uri = urlsplit(endpoint)
            host = uri.hostname
            if host in ('localhost', '127.0.0.1', '::1'):
                host = 'loopback'
            if not host or uri.port is None:
                raise ValueError('Backend endpoint requires an explicit port / 后端地址必须包含端口')
            return host, uri.port
        demo_addresses = {address(config[key]) for key in ('http_endpoint', 'ws_endpoint')}
        realtime_addresses = {address(result[key]) for key in ('http_endpoint', 'ws_endpoint')}
        if demo_addresses & realtime_addresses:
            raise ValueError('Modes require separate Backend ports / 两种模式必须使用不同后端端口')
    return result


def geoid_separation(root):
    """Match the UE -game WindowsEditor config without changing shared INIs."""
    parser = configparser.RawConfigParser(strict=False)
    parser.read([str(Path(root) / name) for name in (
        'Config/DefaultGame.ini', 'Saved/Config/WindowsEditor/Game.ini')], encoding='utf-8-sig')
    value = parser.getfloat('/Script/UE5DroneControl.DroneNetworkManager',
                           'GeoidSeparationMeters', fallback=0.0)
    if not math.isfinite(value):
        raise ValueError('Invalid GeoidSeparationMeters / 高程基准修正无效')
    return value


def received_origin(drones, context, *, now=None, max_age=5.0, geoid=0.0):
    """Prefer the selected UAV, otherwise the lowest slot with a fresh real fix.

    telemetry_received_at is stamped by Backend when the packet arrives, not
    when the launcher polls. Received GPS altitude is AMSL; Cesium uses ellipsoid.
    """
    now = time.time() if now is None else now
    if not number(max_age) or max_age <= 0 or not number(geoid):
        raise ValueError('Invalid real GPS settings / 实时 GPS 配置无效')
    candidates = []
    for drone in drones:
        if not isinstance(drone, dict):
            continue
        if (drone.get('simulation') is not False or drone.get('telemetry_source') != 'received'
                or drone.get('status') != 'online' or drone.get('gps_fix') is not True):
            continue
        received_at = drone.get('telemetry_received_at')
        if not number(received_at) or not 0 <= now - received_at <= max_age:
            continue
        altitude = drone.get('gps_alt')
        origin = {'longitude': drone.get('gps_lon'), 'latitude': drone.get('gps_lat'),
                  'altitude': altitude + geoid if number(altitude) else None}
        if not valid_origin(origin) or (origin['longitude'] == 0 and origin['latitude'] == 0):
            continue
        slot = drone.get('slot')
        if type(slot) is not int or slot <= 0:
            continue
        uid = f'UAV-{slot:02}'
        candidates.append((uid != context.get('active_uav_id'), slot, uid, origin))
    if not candidates:
        return None
    _, _, uid, origin = min(candidates, key=lambda value: value[:3])
    return {**origin, 'uav_id': uid}


def has_real_connection(drones):
    # A configured registry entry or an outgoing connection probe is not proof
    # of aircraft presence. Require an actual packet received by Backend.
    return any(isinstance(drone, dict) and drone.get('simulation') is False
               and drone.get('telemetry_source') == 'received'
               and drone.get('status') in ('online', 'connecting') for drone in drones)


def check_backend_mode(snapshot, mode):
    plans = snapshot.get('plans', {})
    mock = plans.get('mock_execution', {})
    if mode == 'realtime':
        if 'telemetry_receive_time' not in snapshot['backend'].get('capabilities', []):
            raise RuntimeError('Backend GPS freshness support missing. Run SetupDroneSecurity.cmd. / '
                               '后端缺少真实定位时间信息，请先运行 SetupDroneSecurity.cmd 重新编译。')
        # Real adapter configuration is also stored under the legacy
        # mock_execution key; its name alone does not identify a Mock session.
        if plans.get('execution_adapter') != 'Real' and (
                mock.get('uavs') or plans.get('mock_position_baseline', {}).get('positions')):
            raise RuntimeError('Realtime mode cannot reuse a Mock Backend. / 实时模式不能复用 DEMO 后端。')
    elif mode == 'demo':
        if plans.get('execution_adapter') == 'Real' or not mock.get('uavs'):
            raise RuntimeError('DEMO requires a Mock Backend. / DEMO 需要模拟后端。')
