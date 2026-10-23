// HTTP/WS evidence using synthetic clients; widget navigation and playback are separate checks.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {testClient, until} from '../ManualTakeover/client.mjs';

assert(process.env.EDIT_RETURN_VIDEO_EVIDENCE, 'Set a fresh EDIT_RETURN_VIDEO_EVIDENCE directory');
for (const endpoint of [process.env.P1_HTTP, process.env.P1_WS]) {
  assert(endpoint, 'Explicit isolated HTTP and WS endpoints are required');
  assert(![19880, 19881, 22992, 22993].includes(Number(new URL(endpoint).port)), 'Production/native host is protected');
}
const out = path.resolve(process.env.EDIT_RETURN_VIDEO_EVIDENCE);
fs.mkdirSync(out, {recursive: true});
assert(!fs.existsSync(path.join(out, 'result.json')), 'Choose a fresh evidence directory; preserve previous failures');
const client = testClient(), {api, connect, send, state} = client;
const passed = [], snapshots = {};
const pass = name => {passed.push(name); console.log('PASS ' + name);};
const stablePlans = s => ({plans: s.plans, missions: s.missions, paths: s.paths,
  deployments: s.deployments, executions: s.executions, execution_requests: s.execution_requests});
const videoState = () => api('/api/video-view');
const context = () => api('/api/context');
let mapSerial = 0;
const nextSerial = () => ++mapSerial;
const select = (c, primary, members = [primary], serial, clicked = primary, status = 200) => api('/api/context', {
  instance_id: c.id, patch: {active_uav_id: primary, selected_uav_ids: members},
  ...(serial === undefined ? {} : {map_selection_serial: serial, map_video_target_uav_id: clicked}),
}, status, 'PATCH');
const videoPatch = (c, target, extra = {}, status = 200) => api('/api/video-view', {
  instance_id: c.id, video_target_uav_id: target, ...extra,
}, status, 'PATCH');
const guard = (ack, serial) => ({expected_context_version: ack.context_version, map_selection_serial: serial});
let failure, finalState;
try {
  const command = await connect('Command'), map = await connect('Map'), video = await connect('Video');
  const initial = await state();
  assert.equal(initial.execution_adapter, 'Mock');
  assert.equal(Object.keys(initial.plans).length, 0, 'Requires a fresh plan store');
  assert.equal(Object.keys(initial.executions).length, 0, 'Requires a fresh execution store');
  const homes = initial.mock_execution.uavs;
  const one = homes['UAV-01'].home_position, two = homes['UAV-02'].home_position;
  const three = homes['UAV-03'].home_position, four = homes['UAV-04'].home_position;
  const center = {...one, longitude: (one.longitude + two.longitude) / 2};
  const spacing = (two.longitude - one.longitude) * Math.PI / 180 * 6371000 * Math.cos(one.latitude * Math.PI / 180);
  assert(spacing > 1.5, 'Fixture requires safe eastward home spacing');
  const route = points => ({pathId: 1, bClosedLoop: false, waypoints: points.map((p, i) => ({
    sequence: i + 1, ...p, altitude_reference: 'Ellipsoid', segmentSpeed: 5, waitTime: 0,
  }))});
  const safeFirst = route([center, {...center, latitude: center.latitude + .0003}]);
  const safeSecond = route([four, {...four, latitude: four.latitude + .0003}]);
  await send(command, 'begin_configuration');
  let response = await send(command, 'create_plan', {name: 'Edit return and redeploy regression',
    default_mission_name: 'Formation task'});
  const plan = response.plan_id, first = response.mission_id;
  const second = (await send(command, 'add_mission', {plan_id: plan, name: 'Independent safe task'})).mission_id;
  await send(command, 'assign', {plan_id: plan, mission_id: first,
    assigned_uav_ids: ['UAV-01', 'UAV-02'], formation: 'Line', spacing_m: spacing});
  await send(command, 'assign', {plan_id: plan, mission_id: second, assigned_uav_id: 'UAV-04'});
  async function saveRoute(mission, path) {
    const edit = await send(map, 'begin_route_edit', {plan_id: plan, mission_id: mission,
      content_revision: (await state()).plans[plan].content_revision});
    return send(map, 'save_route', {plan_id: plan, mission_id: mission, edit_session_id: edit.edit_session_id, path});
  }
  await saveRoute(first, safeFirst); await saveRoute(second, safeSecond);
  for (const action of ['validate', 'review', 'deploy']) response = await send(command, action, {plan_id: plan});
  const original = response.state, deployment = original.plans[plan].deployment_id;
  const firstRouteId = original.missions[first].route_id, secondRouteId = original.missions[second].route_id;
  assert.equal(original.plans[plan].status, 'DEPLOYED'); assert.equal(original.configuration_session, null);
  assert.equal(Object.keys(original.executions).length, 0);
  snapshots.original_deployment = original;
  pass('Two-task plan deploys with immutable history and without starting execution');

  await send(command, 'begin_configuration', {plan_id: plan});
  await send(command, 'reopen_plan', {plan_id: plan,
    expected_content_revision: (await state()).plans[plan].content_revision});
  // The first segment remains northward, keeping the Line slots east/west.
  // Its final positions coincide with the unselected third/fourth UAVs.
  const collision = route([center, {...center, latitude: center.latitude + .0002},
    {...three, longitude: (three.longitude + four.longitude) / 2}]);
  await saveRoute(first, collision);
  response = await send(command, 'validate', {plan_id: plan});
  const invalid = response.state, validation = invalid.plans[plan].validation;
  assert.equal(invalid.plans[plan].status, 'DRAFT'); assert.equal(validation.ready, false);
  assert.equal(invalid.missions[first].status, 'DRAFT', 'A trajectory conflict must not show a READY task');
  assert.equal(invalid.missions[second].status, 'READY', 'The independent valid task keeps its readiness');
  const issue = validation.issues.find(i => i.mission_id === first && i.code === 'TRAJECTORY_CONFLICT');
  assert(issue, 'Collision issue must name the failing task');
  assert(issue.params.uav_a && issue.params.uav_b);
  assert(issue.params.minimum_separation_m < issue.params.required_separation_m);
  assert.deepEqual(invalid.deployments, original.deployments);
  snapshots.collision_validation = invalid;
  pass('Collision validation marks the failing task and plan DRAFT with useful separation details');

  for (const action of ['review', 'deploy']) {
    const before = await state();
    const denied = await send(command, action, {plan_id: plan}, 409);
    assert.equal(denied.code, 'PLAN_NOT_READY');
    assert.deepEqual(stablePlans(await state()), stablePlans(before));
  }
  response = await send(command, 'end_configuration', {plan_id: plan});
  assert.equal(response.state.configuration_session, null);
  assert.equal(response.state.plans[plan].status, 'DRAFT');
  assert.equal(Object.keys(response.state.edit_sessions).length, 0);
  const listSelection = await send(command, 'select', {plan_id: '', mission_id: ''});
  assert.equal(listSelection.context.active_security_plan_id, null);
  assert.equal(listSelection.context.active_mission_id, null);
  assert(listSelection.state.plans[plan], 'Leaving configuration must not delete the plan');
  snapshots.after_configuration_exit = listSelection.state;
  pass('Failed review/deploy preserve data; configuration can release and plan selection can return to the list');

  await send(command, 'begin_configuration', {plan_id: plan});
  await saveRoute(first, route([center, {...center, latitude: center.latitude + .0004}]));
  for (const action of ['validate', 'review', 'deploy']) response = await send(command, action, {plan_id: plan});
  const repaired = response.state;
  assert.equal(repaired.plans[plan].status, 'DEPLOYED'); assert.equal(repaired.configuration_session, null);
  assert.deepEqual(repaired.plans[plan].mission_ids, [first, second]);
  assert.equal(repaired.missions[first].route_id, firstRouteId); assert.equal(repaired.missions[second].route_id, secondRouteId);
  assert.equal(repaired.missions[first].status, 'DEPLOYED'); assert.equal(repaired.missions[second].status, 'DEPLOYED');
  assert.equal(Object.keys(repaired.plans).length, 1); assert.equal(Object.keys(repaired.deployments).length, 2);
  assert.notEqual(repaired.plans[plan].deployment_id, deployment);
  assert.deepEqual(repaired.deployments[deployment], original.deployments[deployment]);
  assert.equal(Object.keys(repaired.executions).length, 0);
  snapshots.redeployed = repaired;
  pass('Correcting the route redeploys the same plan/task/route IDs and preserves the first deployment snapshot');

  await videoPatch(video, 'UAV-01');
  let beforeVideo = await videoState();
  await select(command, 'UAV-04');
  await select(map, 'UAV-03'); // Ordinary context change deliberately has no explicit-click serial.
  assert.deepEqual(await videoState(), beforeVideo);
  pass('Ordinary Command and Map context selection do not implicitly retarget Video');

  const clickSerial = nextSerial();
  const clickAck = await select(map, 'UAV-02', ['UAV-02'], clickSerial);
  assert.deepEqual(await videoState(), beforeVideo, 'Selection acknowledgement alone does not mutate VideoView');
  const accepted = await videoPatch(map, 'UAV-02', guard(clickAck, clickSerial));
  assert.equal(accepted.video_target_uav_id, 'UAV-02');
  assert.deepEqual(await context(), clickAck, 'Video target update must not rewrite shared selection');
  for (const c of [command, map, video]) await until(async () => c.events, events => events.some(e =>
    e.type === 'VideoViewChanged' && e.payload.video_view_version === accepted.video_view_version &&
    e.payload.video_target_uav_id === 'UAV-02'), `${c.role} receives guarded video change`);
  snapshots.first_map_video_ack = {context: clickAck, video: accepted};
  pass('Acknowledged explicit Map choice updates Video through a guarded request and broadcasts to every role');

  const staleSerial = nextSerial();
  const staleAck = await select(map, 'UAV-03', ['UAV-03'], staleSerial);
  const currentSerial = nextSerial();
  const currentAck = await select(map, 'UAV-04', ['UAV-04'], currentSerial);
  assert(currentAck.context_version > staleAck.context_version);
  beforeVideo = await videoState();
  let denied = await videoPatch(map, 'UAV-03', guard(staleAck, staleSerial), 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE');
  assert.deepEqual(await videoState(), beforeVideo);
  // Keep the latest intent but let another source change context: the version alone must reject it.
  await select(command, 'UAV-01');
  denied = await videoPatch(map, 'UAV-04', guard(currentAck, currentSerial), 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE'); assert.deepEqual(await videoState(), beforeVideo);
  const restoredSerial = nextSerial(), restoredAck = await select(map, 'UAV-04', ['UAV-04'], restoredSerial);
  await videoPatch(map, 'UAV-04', guard(restoredAck, restoredSerial));
  pass('Delayed video requests with stale serial or context version preserve the current Video view');

  await videoPatch(video, 'UAV-01');
  const oldSameSerial = nextSerial(), newSameSerial = nextSerial();
  const oldSameAck = await select(map, 'UAV-04', ['UAV-04'], oldSameSerial);
  const newSameAck = await select(map, 'UAV-04', ['UAV-04'], newSameSerial);
  assert.equal(newSameAck.context_version, oldSameAck.context_version, 'Identical selection remains a context no-op');
  beforeVideo = await videoState();
  denied = await videoPatch(map, 'UAV-04', guard(oldSameAck, oldSameSerial), 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE'); assert.deepEqual(await videoState(), beforeVideo);
  const sameAccepted = await videoPatch(map, 'UAV-04', guard(newSameAck, newSameSerial));
  await videoPatch(video, 'UAV-01');
  const beforeConsumedRetry = await videoState();
  const sameRetry = await videoPatch(map, 'UAV-04', guard(newSameAck, newSameSerial), 409);
  assert.equal(sameRetry.code, 'VIDEO_SELECTION_STALE');
  assert.deepEqual(await videoState(), beforeConsumedRetry, 'Consumed intent cannot overwrite later explicit Video choice');
  const freshSameSerial = nextSerial(), freshSameAck = await select(map, 'UAV-04', ['UAV-04'], freshSameSerial);
  assert.equal(freshSameAck.context_version, newSameAck.context_version);
  const freshSameVideo = await videoPatch(map, 'UAV-04', guard(freshSameAck, freshSameSerial));
  assert.equal(freshSameVideo.video_target_uav_id, sameAccepted.video_target_uav_id);
  const beforeStaleContext = await context(), beforeStaleVideo = await videoState();
  denied = await select(map, 'UAV-02', ['UAV-02'], oldSameSerial, 'UAV-02', 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE');
  assert.deepEqual(await context(), beforeStaleContext); assert.deepEqual(await videoState(), beforeStaleVideo);
  pass('Selection serial rejects stale context/video requests even at the same version; current same-UAV click repairs Video');

  const firstModifiedSerial = nextSerial(), modifiedSerial = nextSerial();
  const firstModifiedAck = await select(map, 'UAV-01', ['UAV-01', 'UAV-02'], firstModifiedSerial, 'UAV-01');
  const modifiedAck = await select(map, 'UAV-01', ['UAV-01', 'UAV-02'], modifiedSerial, 'UAV-02');
  assert.equal(firstModifiedAck.context_version, modifiedAck.context_version,
    'Changing the recorded clicked UAV with identical primary/multiselect is a context no-op');
  beforeVideo = await videoState();
  denied = await videoPatch(map, 'UAV-01', guard(firstModifiedAck, firstModifiedSerial), 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE'); assert.deepEqual(await videoState(), beforeVideo);
  const beforeWrongTarget = await videoState();
  denied = await videoPatch(map, 'UAV-01', guard(modifiedAck, modifiedSerial), 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE'); assert.deepEqual(await videoState(), beforeWrongTarget);
  await videoPatch(map, 'UAV-02', guard(modifiedAck, modifiedSerial));
  assert.equal((await context()).active_uav_id, 'UAV-01');
  assert.equal((await videoState()).video_target_uav_id, 'UAV-02');
  const afterModifiedVideo = await videoState();
  denied = await select(map, 'UAV-04', ['UAV-04'], firstModifiedSerial, 'UAV-04', 409);
  assert.equal(denied.code, 'VIDEO_SELECTION_STALE');
  assert.deepEqual(await context(), modifiedAck); assert.deepEqual(await videoState(), afterModifiedVideo);
  // Ctrl removes the clicked UAV from the final selection but still targets its video.
  const removeSerial = nextSerial();
  const removeAck = await select(map, 'UAV-01', ['UAV-01'], removeSerial, 'UAV-02');
  await videoPatch(map, 'UAV-02', guard(removeAck, removeSerial));
  assert.deepEqual((await context()).selected_uav_ids, ['UAV-01']);
  assert.equal((await videoState()).video_target_uav_id, 'UAV-02');
  pass('A newer same-context click supersedes its old target; stale requests cannot revert it, including modifier clicks');

  // Reconnect the same registered Video identity, then hydrate via HTTP after subscription.
  // This mirrors the existing subscribe-first/read-snapshot client contract.
  clearInterval(video.timer); video.socket.close();
  await until(() => api('/api/context/clients'), clients => clients.some(c => c.instance_id === video.id && c.state === 'OFFLINE'),
    'Old Video socket becomes offline');
  const reconnectEvents = [], socket = new WebSocket(process.env.P1_WS);
  video.socket = socket;
  socket.onmessage = event => {const message = JSON.parse(event.data); video.events.push(message); reconnectEvents.push(message);};
  await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('Video reconnect timed out')), 8000);
    socket.onopen = () => {clearTimeout(timeout); resolve();};
    socket.onerror = error => {clearTimeout(timeout); reject(error);};
  });
  socket.send(JSON.stringify({type: 'subscribe_context', instance_id: video.id}));
  await until(async () => reconnectEvents, events => events.some(e => e.type === 'context_subscribed'), 'Video resubscribes');
  const hydrated = {context: await context(), video: await videoState()};
  assert.equal(hydrated.video.video_target_uav_id, 'UAV-02');
  assert.equal(reconnectEvents.find(e => e.type === 'context_subscribed').payload.context_version, hydrated.context.context_version);
  video.timer = setInterval(() => {if (socket.readyState === WebSocket.OPEN)
    socket.send(JSON.stringify({type: 'context_ping', hydrated: true}));}, 2000);
  const latestSerial = nextSerial(), latestAck = await select(map, 'UAV-03', ['UAV-03'], latestSerial);
  const latestVideo = await videoPatch(map, 'UAV-03', guard(latestAck, latestSerial));
  await until(async () => reconnectEvents, events => events.some(e => e.type === 'VideoViewChanged' &&
    e.payload.video_view_version === latestVideo.video_view_version && e.payload.video_target_uav_id === 'UAV-03'),
    'Reconnected Video receives subsequent selection');
  snapshots.video_rehydration = hydrated;
  pass('The same Video identity reconnects, hydrates the current target and receives subsequent Map changes');

  beforeVideo = await videoState();
  await select(command, 'UAV-01');
  await send(command, 'select', {plan_id: plan, mission_id: second});
  assert.deepEqual(await videoState(), beforeVideo);
  finalState = await state();
  assert.deepEqual(stablePlans(finalState), stablePlans(repaired));
  pass('Video synchronization and later Command/task selections leave deployments and execution state unchanged');

  // Freeze ordinary ping timers so the only plan-store version changes below
  // come from explicitly observed WS owner heartbeats and acknowledged actions.
  for (const c of client.clients) clearInterval(c.timer);
  async function ping(c) {
    const count = c.events.filter(e => e.type === 'context_pong').length;
    client.traffic.push({at: new Date().toISOString(), method: 'WS', path: process.env.P1_WS,
      instance_id: c.id, body: {type: 'context_ping', hydrated: true}});
    c.socket.send(JSON.stringify({type: 'context_ping', hydrated: true}));
    await until(async () => c.events, events => events.filter(e => e.type === 'context_pong').length > count,
      `${c.role} owner heartbeat acknowledged`);
  }
  // Drain any ping already queued on each socket before acquiring the next lease.
  for (const c of client.clients) await ping(c);
  await send(command, 'begin_configuration', {plan_id: plan});
  response = await send(command, 'reopen_plan', {plan_id: plan,
    expected_content_revision: (await state()).plans[plan].content_revision});
  const frozenRevision = response.state.plans[plan].content_revision;
  const frozenLease = response.state.configuration_session.acquired_version;
  const heartbeatSteps = []; snapshots.deployment_heartbeats = heartbeatSteps;
  const mutationState = s => ({...stablePlans(s), version: s.version,
    configuration_session: s.configuration_session, edit_sessions: s.edit_sessions});
  for (const action of ['validate', 'review', 'deploy']) {
    const acknowledged = response.state;
    const guards = {expected_content_revision: frozenRevision,
      configuration_acquired_version: frozenLease, expected_plan_version: acknowledged.plans[plan].version};
    assert.equal(acknowledged.plans[plan].content_revision, frozenRevision);
    assert.equal(acknowledged.configuration_session.acquired_version, frozenLease);
    await ping(command);
    const heartbeat = await until(state, s => s.version > acknowledged.version,
      `${action} sees a WS heartbeat after its captured global version`);
    assert.deepEqual(stablePlans(heartbeat), stablePlans(acknowledged), 'Heartbeat must not alter plan contents or plan version');
    assert.equal(heartbeat.configuration_session.acquired_version, frozenLease);
    const request = {instance_id: command.id, action, plan_id: plan,
      expected_version: acknowledged.version, ...guards};
    const step = {action, captured: acknowledged, heartbeat, request}; heartbeatSteps.push(step);
    // Each explicit guard is checked both with stale and with current global CAS.
    for (const expectedVersion of [acknowledged.version, heartbeat.version]) {
      for (const [field, code] of [['expected_content_revision', 'CONTENT_REVISION_CONFLICT'],
        ['configuration_acquired_version', 'VERSION_CONFLICT'], ['expected_plan_version', 'VERSION_CONFLICT']]) {
        const before = await state();
        const rejected = await api('/api/security-plans', {...request, expected_version: expectedVersion,
          [field]: guards[field] + 1}, 409);
        assert.equal(rejected.code, code, `${action} rejects mismatched ${field} at global ${expectedVersion}`);
        assert.deepEqual(mutationState(await state()), mutationState(before), 'Rejected guard must not mutate plan or lease');
      }
    }
    const unguarded = await api('/api/security-plans', {instance_id: command.id, action, plan_id: plan,
      expected_version: acknowledged.version}, 409);
    assert.equal(unguarded.code, 'VERSION_CONFLICT', 'Legacy request retains strict global CAS');
    const partial = {...request}; delete partial.configuration_acquired_version;
    assert.equal((await api('/api/security-plans', partial, 409)).code, 'VERSION_CONFLICT');
    for (const invalidGlobal of [heartbeat.version + 1, frozenLease - 1])
      assert.equal((await api('/api/security-plans', {...request, expected_version: invalidGlobal}, 409)).code,
        'VERSION_CONFLICT', 'Explicit guards cannot authorize a future or pre-lease global version');
    assert.deepEqual(mutationState(await state()), mutationState(heartbeat));
    response = await api('/api/security-plans', request);
    step.accepted = response.state;
    assert(response.state.version > heartbeat.version);
    assert.equal(response.state.plans[plan].content_revision, frozenRevision);
  }
  pass('Wrong content, lease and plan guards reject at stale or exact global versions; legacy and incomplete requests remain strict');
  finalState = response.state;
  assert.equal(finalState.plans[plan].status, 'DEPLOYED'); assert.equal(finalState.configuration_session, null);
  assert.equal(Object.keys(finalState.deployments).length, 3);
  for (const [id, immutable] of Object.entries(repaired.deployments)) assert.deepEqual(finalState.deployments[id], immutable);
  assert.deepEqual(finalState.paths, repaired.paths); assert.deepEqual(finalState.plans[plan].mission_ids, [first, second]);
  assert.equal(Object.keys(finalState.executions).length, 0);
  pass('Frozen content/lease and per-step plan ACK guards deploy the same plan despite a real WS heartbeat before every phase');
} catch (error) {
  failure = error.stack ?? String(error); console.error(failure); process.exitCode = 1;
} finally {
  try {finalState = await state();} catch (error) {failure ??= error.stack; process.exitCode = 1;}
  client.close();
  fs.writeFileSync(path.join(out, 'result.json'), JSON.stringify({pass: !failure, synthetic: true,
    evidence_kind: 'HTTP/WS with isolated Mock Backend', native_verified: false, real_hardware_verified: false,
    passed, passed_count: passed.length, failure: failure ?? null}, null, 2));
  for (const [name, value] of Object.entries({traffic: client.traffic, snapshots, 'final-state': finalState,
    events: client.clients.map(c => ({role: c.role, instance_id: c.id, events: c.events}))}))
    fs.writeFileSync(path.join(out, `${name}.json`), JSON.stringify(value ?? null, null, 2));
}
