import json
from pathlib import Path
import time

out = Path(r'<USER>\Documents\ChatGPT\无人机安防项目\output\manual-takeover-native')
runtime = Path(r'Saved\ManualTakeover-native-1792718959342115200\data\security_plans.json')
read = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
before, after, paused = (read(out / name) for name in ('before-takeover.json', 'after-takeover.json', 'paused-state.json'))
source, peer, moved = 'execution-10', 'execution-11', 'execution-13'
assert before['executions'][source]['state'] == before['executions'][peer]['state'] == 'EXECUTING'
assert after['executions'][source]['state'] == 'ABORTED'
assert after['executions'][source]['completion_reason'] == 'MANUAL_TAKEOVER'
assert after['executions'][peer]['state'] == 'EXECUTING'
assert after['executions'][moved]['source_execution_id'] == source
assert after['executions'][moved]['group_id'] != after['executions'][source]['group_id']
assert len(after['executions']) == len(before['executions']) + 1
assert paused['executions'][peer]['state'] == 'EXECUTING'
assert paused['executions'][moved]['state'] == 'PAUSED'
deadline = time.monotonic() + 90
while True:
    final = read(runtime)
    movement = final['executions'][moved]
    if movement['state'] == 'COMPLETED':
        break
    if movement['state'] in ('FAILED', 'ABORTED') or time.monotonic() > deadline:
        raise RuntimeError('Native movement failed to complete: ' + movement['state'])
    time.sleep(.5)
assert final['executions'][source]['state'] == 'ABORTED'
peer_final = final['executions'][peer]
# The fixture can finish naturally while UI authorization and evidence review run.
# The immediate and paused snapshots above must still prove peer continuity.
assert peer_final['state'] in ('EXECUTING', 'COMPLETED')
if peer_final['state'] == 'COMPLETED':
    assert peer_final['completion_reason'] == 'ROUTE_FINISHED'
    assert peer_final['overall_progress'] == 1.0
for axis in ('latitude', 'longitude', 'altitude'):
    assert abs(movement['position'][axis] - movement['route_snapshot']['waypoints'][-1][axis]) < 1e-7
(out / 'completed-state.json').write_text(json.dumps(final, indent=2), encoding='utf-8')
result = dict(pass_=True, native_ui='Computer Use mouse clicks in Chinese Map client',
              fixture='API-created deployed Mock mission, restarted after first fixture completed during app authorization wait',
              real_hardware_verified=False, source=source, continuing_peer=peer, movement=moved,
              peer_final_state=peer_final['state'],
              verification_history=['Initial final-state assertion expected peer still EXECUTING; peer had naturally COMPLETED its original route by evidence review. Immediate and paused continuity assertions remain mandatory.'],
              checks=['selected executing UAV-01', 'right-click destination preview',
                      'explicit Chinese takeover consequence and confirmation', 'source aborted and new independent move',
                      'UAV-02 continues original task', 'native pause and resume of manual move',
                      'movement reaches chosen target', 'original task remains aborted'],
              limitations=['Command takeover native flow not replayed', 'no physical aircraft tested'])
result['pass'] = result.pop('pass_')
(out / 'acceptance.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
