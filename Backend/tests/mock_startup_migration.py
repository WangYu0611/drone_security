"""Owned loopback Backend only: migrate synthetic old state using shipped defaults.

Does not start UE, rebuild binaries, or read/write Saved/Stage1. Raw logs and
synthetic state stay in a unique Saved/MarinaBayQA directory.
"""
from __future__ import annotations

import copy
import hashlib
import json
import math
from pathlib import Path
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Launcher'))
from runtime import Runtime, api

EXPECTED_ORIGIN = {'latitude': 1.279445, 'longitude': 103.862188, 'altitude': 15.0}


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unused_ports():
    listeners = []
    try:
        for _ in range(2):
            listener = socket.socket()
            listeners.append(listener)
            if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            listener.bind(('127.0.0.1', 0))
        return [listener.getsockname()[1] for listener in listeners]
    finally:
        for listener in listeners:
            listener.close()


def main():
    out = ROOT / 'Saved/MarinaBayQA' / ('startup-' + str(time.time_ns()))
    out.mkdir(parents=True)
    binary = ROOT / 'Backend/build/Release/DroneBackend.exe'
    packaged = ROOT / 'Backend/mock_execution.json'
    fixture = json.loads(packaged.read_text(encoding='utf-8'))
    legacy_path = ROOT / 'Backend/tests/fixtures/mock_execution_beijing.json'
    legacy = json.loads(legacy_path.read_text(encoding='utf-8'))
    passed = []
    evidence = {'scope': 'owned loopback Backend; synthetic old state; no UE or hardware',
                'backend_sha256': sha256(binary), 'fixture_sha256': sha256(packaged),
                'legacy_fixture_sha256': sha256(legacy_path), 'expected_origin': EXPECTED_ORIGIN,
                'passed': passed}
    current = None

    def check(name):
        passed.append(name)

    def runtime(directory, reset):
        http, ws = unused_ports()
        return Runtime(ROOT, {'http_endpoint': f'http://127.0.0.1:{http}',
            'ws_endpoint': f'ws://127.0.0.1:{ws}/ws', 'runtime_directory': str(directory),
            'backend_executable': str(binary), 'mock_fixture': str(packaged),
            'mock_reset_on_start': reset})

    def start_owned(instance):
        # Ports were reserved from the OS immediately before this call. Refuse
        # all writes unless Runtime actually created this process.
        instance._backend()
        assert not instance.existing_backend and set(instance.owned) == {'Backend'}

    def stop_owned(instance):
        try:
            instance.stop()
        finally:
            for role in tuple(instance.owned):
                instance.stop_role(role)
        assert not instance.owned

    def state(instance):
        return api(instance.config['http_endpoint'], '/api/security-plans')

    def rows(instance):
        return api(instance.config['http_endpoint'], '/api/drones')

    def assert_formation(instance, expected_source):
        drones = rows(instance)
        assert len(drones) == 4
        # Slots are authoritative IDs; do not rely on JSON object iteration.
        drones.sort(key=lambda drone: int(drone['slot']))
        origin = fixture['startup_formation']['origin']
        scale = 6371000.0 * math.pi / 180.0 * math.cos(math.radians(origin['latitude']))
        for index, drone in enumerate(drones):
            home = fixture['uavs'][f'UAV-{index + 1:02}']['home_position']
            assert abs(drone['gps_lat'] - home['latitude']) < 1e-9
            assert abs(drone['gps_lon'] - home['longitude']) < 1e-9
            assert abs(drone['gps_alt'] - home['altitude']) < 1e-9
            assert abs((drone['gps_lon'] - origin['longitude']) * scale - index * 3.0) < 1e-6
            assert drone['status'] == 'offline' and drone['simulation'] is True
            assert drone['telemetry_source'] == expected_source

    try:
        assert fixture['startup_formation'] == {
            'origin': EXPECTED_ORIGIN, 'spacing_m': 3.0, 'direction': 'east'}
        assert all(uav['home_position']['altitude'] == EXPECTED_ORIGIN['altitude']
                   for uav in fixture['uavs'].values())
        current = runtime(out / 'fresh', False)
        start_owned(current)
        assert_formation(current, 'mock_home')
        assert 'mock_position_baseline' not in state(current)
        check('Fresh natural startup uses Singapore homes at numbered 3 m spacing without hardware Online')
        stop_owned(current)
        current = None

        data = out / 'legacy/data'
        custom = copy.deepcopy(legacy)
        custom['uavs']['UAV-01']['home_position']['altitude'] = 45.0
        write_json(data / 'mock_execution.json', custom)
        custom_bytes = (data / 'mock_execution.json').read_bytes()
        old_position = {'latitude': 39.9818, 'longitude': 116.3484, 'altitude': 60.0}
        route = {'pathId': 1, 'bClosedLoop': False, 'waypoints': [
            {'sequence': i + 1, 'latitude': 39.9818 + i * .0001,
             'longitude': 116.3484, 'altitude': 60.0, 'altitude_reference': 'Ellipsoid',
             'segmentSpeed': 5, 'waitTime': 0} for i in range(2)]}
        old_execution = {'execution_id': 'synthetic-old-execution', 'uav_id': 'UAV-01',
            'state': 'COMPLETED', 'simulation': True, 'position': old_position,
            'home_position': custom['uavs']['UAV-01']['home_position'],
            'route_snapshot': route, 'created_at': 1.0, 'updated_at': 4102444800.0}
        old_doc = {'version': 1, 'next_id': 2, 'plans': {}, 'missions': {},
            'paths': {'synthetic-old-route': route}, 'events': [],
            'executions': {'synthetic-old-execution': old_execution}, 'execution_version': 1}
        write_json(data / 'security_plans.json', old_doc)
        current = runtime(out / 'legacy', False)
        start_owned(current)
        before = state(current)
        assert before['executions']['synthetic-old-execution'] == old_execution
        restored = next(drone for drone in rows(current) if drone['slot'] == 1)
        assert abs(restored['gps_lat'] - old_position['latitude']) < 1e-9
        assert abs(restored['gps_lon'] - old_position['longitude']) < 1e-9
        assert restored['telemetry_source'] == 'mock_execution'
        assert 'mock_position_baseline' not in before
        check('Reset-disabled restart preserves the prior confirmed Beijing execution position')
        stop_owned(current)
        current = None

        for epoch in (1, 2):
            current = runtime(out / 'legacy', True)
            start_owned(current)
            after = state(current)
            assert after['mock_position_baseline']['epoch'] == epoch
            assert after['mock_position_baseline']['startup_formation'] == fixture['startup_formation']
            assert set(after['mock_position_baseline']['positions']) == set(fixture['uavs'])
            assert_formation(current, 'mock_reset')
            assert after['executions'] == before['executions']
            assert after['paths'] == before['paths']
            assert (data / 'mock_execution.json').read_bytes() == custom_bytes
            backups = sorted((data / 'mock-reset-history').glob('*.json'))
            assert len(backups) == epoch
            backup = json.loads(next(p for p in backups if p.name.startswith(f'before-{epoch}-')).read_text())
            assert backup['executions'] == before['executions'] and backup['paths'] == before['paths']
            check(f'Owned new startup {epoch} migrates to Singapore, preserving custom Home, old routes and complete backup')
            if epoch == 1:
                # Re-attachment must not create an epoch or take ownership.
                attached_config = dict(current.config, runtime_directory=str(out / 'attached'))
                attached = Runtime(ROOT, attached_config)
                attached._backend()
                assert attached.existing_backend and not attached.owned
                assert state(current)['mock_position_baseline']['epoch'] == epoch
                assert not (attached.directory / 'data').exists()
                check('Attaching an existing Backend does not reset or replace its state')
            stop_owned(current)
            current = None

        assert sha256(binary) == evidence['backend_sha256']
        assert sha256(packaged) == evidence['fixture_sha256']
        check('Validation left the shipped fixture and existing Backend binary unchanged')
        evidence['pass'] = True
    except Exception as error:
        evidence['pass'] = False
        evidence['error'] = f'{type(error).__name__}: {error}'
    finally:
        if current:
            try:
                stop_owned(current)
            except Exception as error:
                evidence['pass'] = False
                evidence['cleanup_error'] = f'{type(error).__name__}: {error}'
        evidence['passed_count'] = len(passed)
        evidence['expected_count'] = 6
        write_json(out / 'result.json', evidence)
        print(json.dumps({'pass': evidence['pass'], 'passed': len(passed), 'expected': 6,
                          'evidence': str(out / 'result.json'),
                          'error': evidence.get('error'), 'cleanup_error': evidence.get('cleanup_error')}))
    return 0 if evidence['pass'] and len(passed) == 6 else 1


if __name__ == '__main__':
    sys.exit(main())
