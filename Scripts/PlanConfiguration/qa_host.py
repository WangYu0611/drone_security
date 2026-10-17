"""Command-controlled, owned Mock host for native online-Cesium acceptance."""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import sys
import time

from verify import ROOT, cleanup_owned, isolated_config, sha256, start_owned_backend, write_json
from runtime import ROLES, Runtime, SingleInstance, compatible_snapshot


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--http-port', type=int, default=22580)
    parser.add_argument('--ws-port', type=int, default=22581)
    parser.add_argument('--start-all', action='store_true', help='Start all three clients after owned Backend readiness')
    parser.add_argument('--altitude-ellipsoid', type=float, default=80.0,
                        help='Explicit isolated fixture altitude in metres; this is not AGL')
    args = parser.parse_args(argv)
    if not math.isfinite(args.altitude_ellipsoid) or not -1000 <= args.altitude_ellipsoid <= 10000:
        parser.error('--altitude-ellipsoid must be finite and between -1000 and 10000 metres')
    run = str(time.time_ns())
    directory = f'Saved/PlanConfiguration-QA-{run}'
    config = isolated_config(args.http_port, args.ws_port, directory)
    folder = ROOT / directory
    folder.mkdir(parents=True)
    fixture = json.loads((ROOT / 'Backend/mock_execution.json').read_text(encoding='utf-8-sig'))
    if fixture.get('simulation') is not True:
        raise RuntimeError('Native QA requires the explicit Mock fixture')
    for uav in fixture['uavs'].values():
        uav['home_position']['altitude'] = args.altitude_ellipsoid
    fixture_path = folder / 'fixture.json'
    write_json(fixture_path, fixture)
    config.update(mock_fixture=str(fixture_path), map='/Game/Level/CesiumWorld', max_fps=30)
    runtime = Runtime(ROOT, config)
    commands = folder / 'command.json'
    metadata = {'run': run, 'directory': str(folder), 'commands': str(commands),
                'http_endpoint': config['http_endpoint'], 'ws_endpoint': config['ws_endpoint'],
                'map': config['map'], 'fixture_altitude_ellipsoid_m': args.altitude_ellipsoid,
                'backend': str((ROOT / config['backend_executable']).resolve()),
                'backend_sha256': sha256(ROOT / config['backend_executable']),
                'ue_module_sha256': sha256(ROOT / 'Binaries/Win64/UnrealEditor-UE5DroneControl.dll'),
                'started_at_unix_s': time.time(), 'native_validation': 'PENDING'}
    write_json(folder / 'host.json', metadata)
    guard = SingleInstance(config['http_endpoint'])
    last_content = ''
    last_poll = 0.0
    failed = False
    seen_ids = set()
    try:
        start_owned_backend(runtime, args.http_port, args.ws_port)
        runtime.transition('STARTING')
        runtime.deadline = time.monotonic() + config['startup_timeout']
        if args.start_all:
            for role in ROLES:
                runtime.start_role(role)
        print('MOCK_BACKEND_READY ' + json.dumps(metadata, ensure_ascii=False), flush=True)
        while True:
            if commands.exists():
                current = commands.read_text(encoding='utf-8-sig')
                if current != last_content:
                    # A non-atomic editor write can be partial; retry it next tick.
                    try:
                        request = json.loads(current)
                    except json.JSONDecodeError:
                        request = None
                    if request is not None:
                        last_content = current
                        result = {'at_unix_s': time.time(), 'request': request, 'pass': False}
                        should_stop = False
                        try:
                            if not isinstance(request, dict):
                                raise ValueError('Command must be an object')
                            request_id = request.get('request_id')
                            if not isinstance(request_id, str) or not request_id:
                                raise ValueError('Every command needs a nonempty unique request_id')
                            if request_id in seen_ids:
                                raise ValueError('request_id already processed; use a fresh ID')
                            seen_ids.add(request_id)
                            action = request.get('action')
                            role = request.get('role')
                            if action in ('start', 'stop_role', 'restart'):
                                if role not in ROLES and not (action == 'restart' and role == 'Backend'):
                                    raise ValueError('Select an owned Command, Map or Video role; restart also accepts Backend')
                                if action == 'start':
                                    runtime.start_role(role)
                                elif action == 'stop_role':
                                    runtime.stop_role(role)
                                else:
                                    runtime.restart(role)
                            elif action == 'start_all':
                                for client_role in ROLES:
                                    owned = runtime.owned.get(client_role)
                                    if not owned or owned.process.poll() is not None:
                                        runtime.start_role(client_role)
                            elif action == 'snapshot':
                                snapshot = folder / f'snapshot-{time.time_ns()}.json'
                                write_json(snapshot, compatible_snapshot(config['http_endpoint']))
                                result['snapshot'] = str(snapshot)
                            elif action == 'stop':
                                should_stop = True
                            else:
                                raise ValueError('Allowed actions: start, start_all, stop_role, restart, snapshot, stop')
                            result['pass'] = True
                        except Exception as exc:
                            result['error'] = f'{type(exc).__name__}: {exc}'
                        write_json(folder / 'last-command.json', result)
                        with (folder / 'commands.jsonl').open('a', encoding='utf-8') as log:
                            log.write(json.dumps(result, ensure_ascii=False) + '\n')
                        print('COMMAND_DONE ' + json.dumps(result, ensure_ascii=False), flush=True)
                        if should_stop:
                            break
            if time.monotonic() - last_poll >= 1.0:
                last_poll = time.monotonic()
                status = {'at_unix_s': time.time()}
                try:
                    runtime.poll()
                except Exception as exc:
                    status['poll_error'] = f'{type(exc).__name__}: {exc}'
                status.update(state=runtime.state, components=runtime.components,
                              processes=[p.record() for p in runtime.owned.values()])
                write_json(folder / 'status.json', status)
            time.sleep(.25)
    except KeyboardInterrupt:
        metadata['interrupted'] = True
    except Exception as exc:
        failed = True
        metadata['error'] = f'{type(exc).__name__}: {exc}'
        raise
    finally:
        metadata['cleanup_errors'] = cleanup_owned(runtime)
        metadata['ownership_after_stop'] = [p.record() for p in runtime.owned.values()]
        metadata['stopped_at_unix_s'] = time.time()
        metadata['owned_processes_stopped'] = not metadata['cleanup_errors'] and not metadata['ownership_after_stop']
        write_json(folder / 'host.json', metadata)
        guard.close()
        print('OWNED_PROCESSES_STOPPED ' + json.dumps({'directory': str(folder),
              'pass': metadata['owned_processes_stopped']}, ensure_ascii=False), flush=True)
    return 1 if failed or not metadata['owned_processes_stopped'] else 0


if __name__ == '__main__':
    sys.exit(main())
