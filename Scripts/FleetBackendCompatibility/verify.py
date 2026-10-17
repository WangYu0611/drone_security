"""Exercise the user's default Launcher backend, not a phase-specific build override."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Launcher'))
from environment import load_config
from runtime import BackendCompatibilityError, Runtime

parser = argparse.ArgumentParser()
parser.add_argument('--legacy-backend', type=Path,
                    help='Explicit diagnostic only: probe a preserved old executable after readiness rejects it')
args = parser.parse_args()
legacy = args.legacy_backend is not None
run = ('legacy-guard-' if legacy else 'after-') + str(time.time_ns())
out = root / 'Evidence/FleetBackendCompatibility' / run
out.mkdir(parents=True)
config = load_config(root)
if legacy:
    config['backend_executable'] = str(args.legacy_backend.resolve())
config.update(http_endpoint='http://127.0.0.1:21980', ws_endpoint='ws://127.0.0.1:21981/ws',
              runtime_directory='Saved/FleetBackendCompatibility-' + run)
binary = root / config['backend_executable']
metadata = {'backend': config['backend_executable'], 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
            'mode': 'legacy-diagnostic' if legacy else 'current-default', 'passed': False}
runtime = Runtime(root, config)
try:
    try:
        runtime._backend()
    except BackendCompatibilityError as exc:
        if not legacy or 'Backend' not in runtime.owned:
            raise
        # The product readiness gate must reject this owned, isolated old server.
        # Only this explicit diagnostic continues to reproduce its raw API response.
        metadata['compatibility_rejection'] = str(exc)
    if legacy:
        assert 'compatibility_rejection' in metadata, 'Legacy executable was not rejected at readiness'
    env = dict(os.environ, P1_HTTP=config['http_endpoint'], P1_WS=config['ws_endpoint'],
               FLEET_EVIDENCE=str(out / 'probe'), EXPECT_GROUP_STATUS='403' if legacy else '200')
    with (out / 'probe.log').open('w', encoding='utf-8') as log:
        subprocess.run(['node', str(Path(__file__).with_name('probe.mjs'))], cwd=root, env=env,
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=45)
    if not legacy:
        # Use a fresh persisted store for the complete permission/conflict/control protocol.
        runtime.stop()
        config['runtime_directory'] += '-protocol'
        runtime = Runtime(root, config)
        runtime._backend()
        env.update(FLEET_EVIDENCE=str(out / 'protocol'), FLEET_ALTITUDE_DELTA='50')
        with (out / 'protocol.log').open('w', encoding='utf-8') as log:
            subprocess.run(['node', str(root / 'Scripts/FleetControl/protocol.mjs')], cwd=root, env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=150)
    metadata['passed'] = True
finally:
    runtime.stop()
    metadata['ownership_after_stop'] = json.loads((runtime.directory / 'ownership.json').read_text())
    (out / 'result.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    print(out)
    print(json.dumps(metadata, ensure_ascii=False))
