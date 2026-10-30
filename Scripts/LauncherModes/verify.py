"""Isolated Backend/Launcher integration. UE commands are recorded, never run.

All test endpoints and UDP targets are dynamically allocated on loopback.
The existing Backend listener implementation binds wildcard addresses; this
script does not change that implementation or use an operator's configuration.
"""
from __future__ import annotations

from contextlib import contextmanager
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import socket
import sys
import threading
import time
import traceback
import urllib.parse
import urllib.request
import uuid
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Launcher'))
from launch_modes import geoid_separation  # noqa: E402
from runtime import ROLES, Runtime, api, role_arguments  # noqa: E402

LOOPBACK = '127.0.0.1'
SINGAPORE = {'latitude': 1.279445, 'longitude': 103.862188, 'altitude': 15.0}
SYNTHETIC_FIX = {'latitude': 40.1, 'longitude': 116.4, 'altitude': 75.0}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def original_files():
    names = ['Backend/config.yaml', 'Backend/mock_execution.json',
             'Launcher/config.json', 'Launcher/config.local.json',
             'Config/DefaultGame.ini', 'Config/DefaultEngine.ini',
             'Content/CesiumSettings/CesiumIonServers/CesiumIonSaaS.uasset',
             'Saved/DroneRegistry.json', 'Saved/Config/WindowsEditor/Game.ini']
    result = {name: digest(ROOT / name) if (ROOT / name).is_file() else None for name in names}
    for folder in ('Backend/data', 'Saved/Stage1/data'):
        directory = ROOT / folder
        result[folder + '/'] = directory.exists()
        if directory.exists():
            for path in sorted(directory.rglob('*')):
                if path.is_file():
                    result[path.relative_to(ROOT).as_posix()] = digest(path)
    return result


def same_position(actual, expected):
    for key in ('latitude', 'longitude', 'altitude'):
        require(abs(actual[key] - expected[key]) < 1e-8,
                f'{key} differs: {actual[key]} versus {expected[key]}')


def until(read, predicate, timeout=20):
    end = time.monotonic() + timeout
    last = None
    while time.monotonic() < end:
        try:
            last = read()
            if predicate(last):
                return last
        except (OSError, ValueError):
            pass
        time.sleep(.05)
    raise AssertionError('Timed out waiting for expected isolated test state: ' + repr(last))


class Ports:
    def __init__(self):
        self.used = set()

    def socket(self, kind):
        while True:
            sock = socket.socket(socket.AF_INET, kind)
            sock.bind((LOOPBACK, 0))
            port = sock.getsockname()[1]
            if port not in self.used and port not in (8080, 8081, 19880, 19881, 29999):
                self.used.add(port)
                return sock
            sock.close()

    def reserve(self, kind):
        sock = self.socket(kind)
        port = sock.getsockname()[1]
        sock.close()
        return port


@contextmanager
def audited_http(ports, requests, phase):
    """Use the real HTTP transport, disabling process-local proxy inheritance."""
    original = urllib.request.urlopen
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    lock = threading.Lock()
    def request(value, *args, **kwargs):
        url = value.full_url if isinstance(value, urllib.request.Request) else value
        uri = urllib.parse.urlsplit(url)
        require(uri.scheme == 'http' and uri.hostname == LOOPBACK and uri.port in ports,
                'Attempted non-test HTTP request: ' + str(url))
        row = {'phase': phase['name'], 'method': value.get_method()
               if isinstance(value, urllib.request.Request) else 'GET',
               'path': uri.path, 'port': uri.port, 'at': time.time()}
        if isinstance(value, urllib.request.Request) and value.data:
            row['body'] = json.loads(value.data)
        with lock:
            requests.append(row)
        return opener.open(value, *args, **kwargs)
    urllib.request.urlopen = request
    try:
        yield
    finally:
        urllib.request.urlopen = original


def attach_recorders(runtime, commands, processes, binary, output):
    """Preserve real Backend lifecycle; prevent any UE executable launch."""
    spawn = runtime._spawn
    def backend_only(role, arguments, cwd):
        require(role == 'Backend', 'Only Backend processes may execute in this QA')
        require(Path(arguments[0]).resolve() == binary, 'Unexpected Backend executable')
        config_path = Path(arguments[1]).resolve()
        require(output in config_path.parents, 'Backend configuration escaped the isolated QA directory')
        spawn(role, arguments, cwd)
        processes.append(runtime.owned[role])
    def record_role(role):
        require(role in ROLES, 'Unexpected UE role')
        arguments = role_arguments(runtime.config, ROOT, role, str(uuid.uuid4()),
                                   runtime.logs / (role.lower() + '-not-executed.log'))
        commands.append({'role': role, 'mode': runtime.launch_mode, 'arguments': arguments,
                         'origin': dict(runtime.config['startup_origin']),
                         'commands_not_executed': True, 'at': time.time()})
    runtime._spawn = backend_only
    runtime.start_role = record_role


def main():
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    output = ROOT / 'Saved/LauncherModesQA' / ('run-' + stamp)
    output.mkdir(parents=True, exist_ok=False)
    evidence = {'scope': 'Actual owned Backend and Runtime with synthetic loopback UDP; '
                         'UE role arguments only; no native UI, UE rendering, hardware or flight acceptance',
                'commands_not_executed': True, 'real_execution_enabled': False,
                'network': {'client_and_udp_target_host': LOOPBACK,
                            'existing_backend_listener_bind': '0.0.0.0 / wildcard'},
                'passed': [], 'commands': [], 'requests': [], 'result': 'FAIL'}
    binary = (ROOT / 'Backend/build/Release/DroneBackend.exe').resolve()
    runtimes, processes, threads, sockets = [], [], [], []
    stop = threading.Event()
    enable_fix = threading.Event()
    worker_errors, thread_errors, sink_packets = [], [], []
    phase = {'name': 'preparation'}
    original_before = original_files()
    evidence['original_files_before'] = original_before
    def passed(name):
        evidence['passed'].append(name)
        print('PASS ' + name, flush=True)
    try:
        require(binary.is_file(), 'Build Backend/build/Release/DroneBackend.exe before validation')
        evidence['backend_binary'] = {'path': binary.relative_to(ROOT).as_posix(),
                                      'sha256': digest(binary), 'bytes': binary.stat().st_size}
        ports = Ports()
        demo_http, demo_ws, real_http, real_ws, computer_http, computer_ws = [
            ports.reserve(socket.SOCK_STREAM) for _ in range(6)]
        sink = ports.socket(socket.SOCK_DGRAM)
        sender = ports.socket(socket.SOCK_DGRAM)
        recv_port = ports.reserve(socket.SOCK_DGRAM)
        sockets.extend([sink, sender])
        sink.settimeout(.1)
        sink_port = sink.getsockname()[1]
        evidence['network']['ports'] = {'demo_http': demo_http, 'demo_ws': demo_ws,
                                        'realtime_http': real_http, 'realtime_ws': real_ws,
                                        'computer_http': computer_http, 'computer_ws': computer_ws,
                                        'realtime_udp_receive': recv_port, 'udp_sink': sink_port,
                                        'udp_sender': sender.getsockname()[1]}
        real_directory = output / 'realtime'
        data = real_directory / 'data'
        data.mkdir(parents=True)
        registry = data / 'drones.json'
        registry.write_text(json.dumps([{'id': 'd1', 'slot': 1,
            'name': 'Owned synthetic realtime location QA', 'model': 'Loopback synthetic telemetry',
            'ip': LOOPBACK, 'port': sink_port, 'video_url': ''}], indent=2), encoding='utf-8')
        registry_before = digest(registry)
        real_config = {'server': {'http_port': real_http, 'ws_port': real_ws, 'debug': False},
                       'drone': {'lost_timeout_sec': 2}, 'jetson': {'host': LOOPBACK},
                       'port_map': {'1': {'host': LOOPBACK, 'send_port': sink_port,
                                          'recv_port': recv_port, 'ros_topic_prefix': ''}},
                       'storage': {'path': str(registry), 'video_metadata_path': str(data / 'metadata')},
                       'log': {'level': 'info', 'file': str(real_directory / 'backend.log')}}
        config_path = real_directory / 'backend-config.json'
        config_path.write_text(json.dumps(real_config, indent=2), encoding='utf-8')
        config_before = digest(config_path)
        require(not (data / 'mock_execution.json').exists() and not (data / 'real_execution.json').exists(),
                'Realtime test must have no Mock fixture or real-execution adapter file')
        base = {'backend_executable': str(binary), 'ue_executable': 'COMMANDS_NOT_EXECUTED-UnrealEditor.exe',
                'project': 'UE5DroneControl.uproject', 'map': '/Game/Level/CesiumWorld',
                'http_endpoint': f'http://{LOOPBACK}:{demo_http}',
                'ws_endpoint': f'ws://{LOOPBACK}:{demo_ws}/ws',
                'runtime_directory': str(output / 'demo'), 'mock_reset_on_start': True,
                'mock_fixture': 'Backend/mock_execution.json', 'startup_timeout': 30, 'max_fps': 20,
                'realtime_http_endpoint': f'http://{LOOPBACK}:{real_http}',
                'realtime_ws_endpoint': f'ws://{LOOPBACK}:{real_ws}/ws',
                'realtime_runtime_directory': str(real_directory),
                'realtime_backend_config': str(config_path),
                'realtime_gps_timeout': 20, 'realtime_gps_max_age': 5}
        (output / 'launcher-config.json').write_text(json.dumps(base, indent=2), encoding='utf-8')
        with audited_http({demo_http, real_http, computer_http}, evidence['requests'], phase), \
             patch('runtime.computer_origin', side_effect=AssertionError(
                 'This GPS integration must not query the actual Windows location service')) as computer:
            phase['name'] = 'demo'
            demo = Runtime(ROOT, base)
            runtimes.append(demo)
            attach_recorders(demo, evidence['commands'], processes, binary, output)
            demo.start('demo')
            require(not demo.existing_backend, 'DEMO unexpectedly reused a Backend')
            same_position(demo.config['startup_origin'], SINGAPORE)
            state = api(base['http_endpoint'], '/api/security-plans')
            drones = api(base['http_endpoint'], '/api/drones')
            baseline = state['mock_position_baseline']
            require(baseline['epoch'] >= 1 and len(baseline['positions']) == 4, 'DEMO reset not acknowledged for four UAVs')
            same_position(baseline['positions']['UAV-01'], SINGAPORE)
            require(len(drones) == 4, 'DEMO did not seed exactly four aircraft')
            for drone in drones:
                require(drone['simulation'] is True and drone['telemetry_source'] == 'mock_reset',
                        'DEMO did not expose acknowledged Mock-reset provenance')
                require(drone['status'] == 'offline', 'DEMO reset invented hardware Online')
                same_position({'latitude': drone['gps_lat'], 'longitude': drone['gps_lon'],
                               'altitude': drone['gps_alt']},
                              baseline['positions'][f'UAV-{drone["slot"]:02}'])
            demo_commands = [row for row in evidence['commands'] if row['mode'] == 'demo']
            require([row['role'] for row in demo_commands] == list(ROLES), 'DEMO did not prepare all three roles')
            for row in demo_commands:
                same_position(row['origin'], SINGAPORE)
            resets = [row for row in evidence['requests'] if row['phase'] == 'demo'
                      and row['path'] == '/api/mock-executions/reset' and row['method'] == 'POST']
            require(len(resets) == 1, 'DEMO reset must be one acknowledged startup transaction')
            evidence['demo'] = {'baseline': baseline, 'drones': drones,
                                'runtime_state_without_ue_clients': demo.state}
            passed('DEMO real Runtime start acknowledged one Singapore reset and prepared all three unexecuted role commands')
            demo.stop()
            require(not demo.owned, 'DEMO normal shutdown left owned processes')
            passed('DEMO normal shutdown acknowledged Mock pause and reclaimed its owned Backend')

            phase['name'] = 'realtime'
            def drain_sink():
                try:
                    while not stop.is_set():
                        try:
                            payload, address = sink.recvfrom(65535)
                        except socket.timeout:
                            continue
                        require(address[0] == LOOPBACK, 'Non-loopback packet reached the QA sink')
                        row = json.loads(payload)
                        require(row.get('mode') != 'move', 'Location initialization issued a move command')
                        sink_packets.append({'type': row.get('type'), 'mode': row.get('mode'),
                                             'slot': row.get('slot'), 'from_host': address[0]})
                except Exception:
                    thread_errors.append(traceback.format_exc())
            def emit_telemetry():
                try:
                    while not stop.is_set():
                        packet = {'gps_lat': SYNTHETIC_FIX['latitude'], 'gps_lon': SYNTHETIC_FIX['longitude'],
                                  'gps_alt': SYNTHETIC_FIX['altitude'], 'gps_fix': enable_fix.is_set(),
                                  'local_position': [0, 0, -30], 'local_position_valid': True,
                                  'battery': 81, 'telemetry_received_at': 1}
                        sender.sendto(json.dumps(packet).encode('utf-8'), (LOOPBACK, recv_port))
                        stop.wait(.05)
                except Exception:
                    thread_errors.append(traceback.format_exc())
            for name, target in [('loopback-control-sink', drain_sink), ('synthetic-telemetry-sender', emit_telemetry)]:
                thread = threading.Thread(target=target, name=name)
                thread.start()
                threads.append(thread)
            realtime = Runtime(ROOT, base)
            runtimes.append(realtime)
            attach_recorders(realtime, evidence['commands'], processes, binary, output)
            def startup():
                try:
                    realtime.start('realtime')
                except Exception:
                    worker_errors.append(traceback.format_exc())
            worker = threading.Thread(target=startup, name='realtime-runtime-start')
            worker.start()
            threads.append(worker)
            real_base = base['realtime_http_endpoint']
            invalid = until(lambda: api(real_base, '/api/drones'), lambda rows: len(rows) == 1
                            and rows[0].get('telemetry_source') == 'received'
                            and rows[0].get('status') == 'online' and rows[0].get('gps_fix') is False
                            and realtime.location_status == 'waiting')
            require(not realtime.existing_backend, 'Realtime unexpectedly reused a Backend')
            require(realtime.location_status == 'waiting', 'Realtime did not wait for valid GPS')
            guard_end = time.monotonic() + .8
            while time.monotonic() < guard_end:
                require(worker.is_alive() and not worker_errors, 'Realtime startup failed during no-fix phase')
                require(not any(row['mode'] == 'realtime' for row in evidence['commands']),
                        'UE role commands were prepared before a valid real-source fix')
                time.sleep(.05)
            evidence['realtime_before_fix'] = invalid
            passed('Received Online telemetry without a GPS fix keeps startup waiting and prepares no UE role commands')
            enable_fix.set()
            worker.join(timeout=20)
            require(not worker.is_alive() and not worker_errors,
                    'Realtime startup did not complete: ' + repr(worker_errors))
            accepted = api(real_base, '/api/drones')[0]
            require(accepted['simulation'] is False and accepted['telemetry_source'] == 'received'
                    and accepted['status'] == 'online' and accepted['gps_fix'] is True,
                    'Accepted location did not come from fresh received Online GPS')
            require(0 <= time.time() - accepted['telemetry_received_at'] <= 5
                    and accepted['telemetry_received_at'] != 1, 'Backend did not assign a fresh receive time')
            expected = {**SYNTHETIC_FIX,
                        'altitude': SYNTHETIC_FIX['altitude'] + geoid_separation(ROOT)}
            real_commands = [row for row in evidence['commands'] if row['mode'] == 'realtime']
            require([row['role'] for row in real_commands] == list(ROLES), 'Realtime did not prepare all three roles')
            for row in real_commands:
                same_position(row['origin'], expected)
                require(row['origin']['longitude'] != SINGAPORE['longitude'], 'Realtime fell back to Singapore')
            require(realtime.location_status == 'ready', 'Realtime location state did not become ready')
            evidence['realtime'] = {'accepted_telemetry': accepted, 'startup_origin': realtime.config['startup_origin'],
                                    'geoid_separation_m': geoid_separation(ROOT),
                                    'runtime_state_without_ue_clients': realtime.state}
            passed('Fresh synthetic received GPS initializes all three unexecuted roles at the non-Singapore ellipsoid origin')
            realtime.stop()
            require(not realtime.owned, 'Realtime normal shutdown left owned processes')
            real_requests = [row for row in evidence['requests'] if row['phase'] == 'realtime']
            require(not any(row['method'] != 'GET' for row in real_requests), 'Realtime performed a mutation request')
            require(not any('/api/mock-executions/' in row['path'] for row in real_requests),
                    'Realtime performed a Mock reset/pause request')
            require(digest(registry) == registry_before and digest(config_path) == config_before,
                    'Realtime rewrote the synthetic operator registry or Backend configuration')
            require(not (data / 'mock_execution.json').exists() and not (data / 'real_execution.json').exists(),
                    'Realtime created a Mock fixture or enabled Real execution')
            require(not (real_directory / 'backend.yaml').exists(), 'Realtime generated a DEMO Backend seed configuration')
            computer.assert_not_called()
            evidence['os_computer_location_queried'] = False
            passed('Realtime performs only GETs, preserves its sole registry entry, creates no Mock seed/reset/real-execution file, and stops its Backend')

            # Exercise the no-aircraft branch against another real empty Backend.
            # Its location provider result is synthetic; Windows is never queried.
            phase['name'] = 'computer-fallback'
            computer_directory = output / 'computer-fallback'
            computer_data = computer_directory / 'data'
            computer_data.mkdir(parents=True)
            computer_registry = computer_data / 'drones.json'
            computer_registry.write_text('[]', encoding='utf-8')
            computer_config = copy.deepcopy(real_config)
            computer_config.update({'server': {'http_port': computer_http, 'ws_port': computer_ws, 'debug': False},
                                    'port_map': {},
                                    'storage': {'path': str(computer_registry),
                                                'video_metadata_path': str(computer_data / 'metadata')},
                                    'log': {'level': 'info', 'file': str(computer_directory / 'backend.log')}})
            computer_config_path = computer_directory / 'backend-config.json'
            computer_config_path.write_text(json.dumps(computer_config, indent=2), encoding='utf-8')
            computer_base = {**base,
                             'realtime_http_endpoint': f'http://{LOOPBACK}:{computer_http}',
                             'realtime_ws_endpoint': f'ws://{LOOPBACK}:{computer_ws}/ws',
                             'realtime_runtime_directory': str(computer_directory),
                             'realtime_backend_config': str(computer_config_path),
                             'realtime_device_wait': 0}
            computer_fix = {'longitude': 2.3522, 'latitude': 48.8566, 'altitude': 0.0,
                            'source': 'computer', 'accuracy_m': 25.0, 'height_estimated': True}
            computer.side_effect = None
            computer.return_value = computer_fix
            command_start = len(evidence['commands'])
            computer_runtime = Runtime(ROOT, computer_base)
            runtimes.append(computer_runtime)
            attach_recorders(computer_runtime, evidence['commands'], processes, binary, output)
            computer_runtime.start('realtime')
            computer.assert_called_once()
            require(computer_runtime.location_source == 'computer' and computer_runtime.location_status == 'estimated',
                    'No-aircraft fallback did not identify its computer source and estimated height')
            computer_commands = evidence['commands'][command_start:]
            require([row['role'] for row in computer_commands] == list(ROLES),
                    'Computer fallback did not prepare all three unexecuted roles')
            for row in computer_commands:
                same_position(row['origin'], computer_fix)
            require(api(computer_base['realtime_http_endpoint'], '/api/drones') == [],
                    'Computer fallback seeded synthetic aircraft')
            computer_state = computer_runtime.state
            computer_runtime.stop()
            computer_requests = [row for row in evidence['requests'] if row['phase'] == 'computer-fallback']
            require(all(row['method'] == 'GET' for row in computer_requests),
                    'Computer fallback performed a mutation request')
            require(sum(row['path'] == '/api/drones' for row in computer_requests) >= 2,
                    'Computer fallback skipped the GPS-priority recheck')
            require(computer_registry.read_text(encoding='utf-8') == '[]', 'Computer fallback changed its empty registry')
            require(not (computer_data / 'mock_execution.json').exists()
                    and not (computer_data / 'real_execution.json').exists(),
                    'Computer fallback created a Mock or real-execution adapter file')
            evidence['computer_fallback'] = {'provider_mocked': True, 'actual_windows_location_queried': False,
                                              'origin': computer_runtime.config['startup_origin'],
                                              'accuracy_m': 25.0, 'height_estimated': True,
                                              'registry_remained_empty': True,
                                              'runtime_state_without_ue_clients': computer_state}
            passed('An empty real Backend uses the mocked computer location for three unexecuted roles, rechecks GPS and never seeds aircraft')
            evidence['result'] = 'PASS'
    except Exception:
        evidence['error'] = traceback.format_exc()
        print(evidence['error'], file=sys.stderr, flush=True)
    finally:
        phase['name'] = 'cleanup'
        stop.set()
        for runtime in runtimes:
            if runtime.state == 'STARTING':
                runtime.stopping.set()
        for thread in threads:
            thread.join(timeout=20)
        cleanup_errors = []
        for runtime in reversed(runtimes):
            try:
                runtime.stop()
            except Exception:
                cleanup_errors.append(traceback.format_exc())
                # A failed normal shutdown remains a failed check; reclaim only
                # these retained owned handles so a failed QA leaves no child.
                for process in list(runtime.owned.values()):
                    try:
                        process.stop()
                    except Exception:
                        cleanup_errors.append(traceback.format_exc())
        for sock in sockets:
            sock.close()
        original_after = original_files()
        evidence['original_files_after'] = original_after
        evidence['original_files_unchanged'] = original_before == original_after
        evidence['sink_packets'] = sink_packets
        evidence['thread_errors'] = thread_errors
        evidence['startup_worker_errors'] = worker_errors
        evidence['cleanup'] = {'errors': cleanup_errors,
                               'threads_alive': [thread.name for thread in threads if thread.is_alive()],
                               'owned_processes': [process.record() for process in processes],
                               'all_owned_processes_exited': all(process.process.poll() is not None for process in processes),
                               'all_runtime_ownership_empty': all(not runtime.owned for runtime in runtimes)}
        if (cleanup_errors or thread_errors or worker_errors or not evidence['original_files_unchanged']
                or evidence['cleanup']['threads_alive'] or not evidence['cleanup']['all_owned_processes_exited']
                or not evidence['cleanup']['all_runtime_ownership_empty']):
            evidence['result'] = 'FAIL'
        elif evidence['result'] == 'PASS':
            passed('All task-owned Backends and sender/sink/startup threads stopped; original business/configuration hashes remain unchanged')
        (output / 'result.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
        print('Evidence: ' + str(output), flush=True)
        print('Result: ' + evidence['result'], flush=True)
    return 0 if evidence['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
