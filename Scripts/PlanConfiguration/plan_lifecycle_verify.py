"""Own a fresh loopback Mock Backend, test plan lifecycle APIs, restart and verify history."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import urllib.request
from verify import ROOT, isolated_config, start_owned_backend, cleanup_owned, write_json, sha256
from runtime import Runtime, SingleInstance


class LifecycleRuntime(Runtime):
    def _spawn(self, role, args, cwd):
        if role == 'Backend':
            # The existing P1 suite emits a synthetic debug alert. Enable it only
            # in this fresh owned test directory, never in production config.
            config_path = Path(args[1]).resolve()
            if config_path.parent != self.directory.resolve():
                raise RuntimeError('Refusing to alter a config outside the isolated runtime')
            config_path.write_text(config_path.read_text(encoding='utf-8').replace(
                '  debug: false', '  debug: true', 1), encoding='utf-8')
        return super()._spawn(role, args, cwd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--http-port', type=int, default=24980)
    parser.add_argument('--ws-port', type=int, default=24981)
    args = parser.parse_args()
    directory = f'Saved/PlanLifecycle/api-{time.time_ns()}'
    config = isolated_config(args.http_port, args.ws_port, directory,
                             mock_fixture='Backend/mock_execution.json')
    runtime = LifecycleRuntime(ROOT, config)
    folder = ROOT / directory
    guard = SingleInstance(config['http_endpoint'])
    result = {'pass': False, 'native_verified': False, 'hardware_verified': False,
              'backend_sha256': sha256(ROOT / config['backend_executable'])}
    try:
        start_owned_backend(runtime, args.http_port, args.ws_port)
        env = dict(os.environ, P1_HTTP=config['http_endpoint'], P1_WS=config['ws_endpoint'],
                   PLAN_LIFECYCLE_OUT=str(folder), PLAN_LIFECYCLE_ISOLATED='1')
        with (folder / 'context-regression.log').open('w', encoding='utf-8') as log:
            subprocess.run(['node', str(ROOT / 'Backend/tests/p1_context_integration.mjs')],
                           cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                           timeout=60, check=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        result['context_regression'] = True
        with (folder / 'protocol.log').open('w', encoding='utf-8') as log:
            subprocess.run(['node', str(ROOT / 'Scripts/PlanConfiguration/plan_lifecycle_api.mjs')],
                           cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                           timeout=120, check=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        before = json.loads((folder / 'before-restart.json').read_text(encoding='utf-8'))
        runtime.stop_role('Backend')
        start_owned_backend(runtime, args.http_port, args.ws_port)
        with urllib.request.urlopen(config['http_endpoint'] + '/api/security-plans', timeout=10) as response:
            after = json.load(response)
        for key in ('plans', 'missions', 'paths', 'deployments', 'executions', 'deleted_plans'):
            assert after[key] == before[key], f'{key} changed across restart'
        write_json(folder / 'after-restart.json', after)
        result.update({'pass': True, 'restart_persistence': True})
    except Exception as exc:
        result['error'] = f'{type(exc).__name__}: {exc}'
    finally:
        errors = cleanup_owned(runtime)
        if errors:
            result['cleanup_errors'] = errors
            result['pass'] = False
        write_json(folder / 'verification.json', result)
        guard.close()
    print(json.dumps({'directory': str(folder), **result}, ensure_ascii=False))
    return 0 if result['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
