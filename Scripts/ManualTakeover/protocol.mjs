// Protocol/Mock evidence only. Run via verify.py; no native UI or real-flight claim.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {testClient, sleep, until} from './client.mjs';
import {deployedFixture} from './fixture.mjs';

assert(process.env.MANUAL_TAKEOVER_EVIDENCE, 'A fresh MANUAL_TAKEOVER_EVIDENCE directory is required');
const out = path.resolve(process.env.MANUAL_TAKEOVER_EVIDENCE);
fs.mkdirSync(out, {recursive: true});
assert(!fs.existsSync(path.join(out, 'result.json')), 'Do not overwrite previous evidence');
const client = testClient(), passed = [], snapshots = {};
const {api, connect, send, state} = client;
const pass = name => {passed.push(name); console.log('PASS ' + name);};
const request = (e, target, extra = {}) => ({request_id: crypto.randomUUID(), execution_id: e.execution_id,
  uav_id: e.uav_id, control_version: e.control_version, target, ...extra});
const groupItems = (s, group) => Object.values(s.executions).filter(e => e.group_id === group);
const executionFields = s => ({executions: s.executions, execution_requests: s.execution_requests,
  execution_version: s.execution_version});
let failure, finalState;
try {
  const command = await connect('Command'), map = await connect('Map'), video = await connect('Video');
  const fixture = await deployedFixture(client, command, map, ['UAV-01', 'UAV-02', 'UAV-03', 'UAV-04'], 6);
  snapshots.deployed = await state();
  const originalMission = structuredClone(snapshots.deployed.missions[fixture.mission]);
  const originalDeployment = structuredClone(snapshots.deployed.deployments);
  pass('Four-UAV task is deployed without automatically starting');

  // Scheduled work cannot be stolen before it begins.
  let response = await send(command, 'execution_start', {...fixture.ids, request_id: crypto.randomUUID(),
    scheduled_start_at: new Date(Date.now() + 300000).toISOString().replace(/\.\d{3}Z$/, 'Z')});
  let source = response.state.executions[response.execution_id];
  const scheduledBefore = await state();
  let rejected = await send(map, 'execution_takeover_move', request(source, source.position), 409);
  assert.equal(rejected.code, 'EXECUTION_STATE_INVALID');
  assert.deepEqual(executionFields(await state()), executionFields(scheduledBefore));
  await send(command, 'execution_cancel', {request_id: crypto.randomUUID(), execution_id: source.execution_id,
    control_version: source.control_version});
  pass('Scheduled deployment rejects takeover without changing execution state');

  response = await send(command, 'execution_start', {...fixture.ids, request_id: crypto.randomUUID()});
  const group = response.state.executions[response.execution_id].group_id;
  let snapshot = await until(state, s => groupItems(s, group).length === 4 &&
    groupItems(s, group).every(e => e.state === 'EXECUTING'), 'Four-UAV mission starts');
  source = groupItems(snapshot, group).find(e => e.uav_id === 'UAV-01');
  const sourceId = source.execution_id;
  const peerId = groupItems(snapshot, group).find(e => e.uav_id === 'UAV-02').execution_id;
  const fourthId = groupItems(snapshot, group).find(e => e.uav_id === 'UAV-04').execution_id;
  const videoBefore = await api('/api/video-view');
  await api('/api/context', {instance_id: map.id,
    patch: {active_uav_id: 'UAV-01', selected_uav_ids: ['UAV-01']}}, 200, 'PATCH');
  await until(async () => command.events, events => events.some(e => e.type === 'OperationalContextChanged' &&
    e.payload?.selected_uav_ids?.length === 1 && e.payload.active_uav_id === 'UAV-01'), 'Selection broadcast');
  assert.deepEqual(await api('/api/video-view'), videoBefore);
  pass('Single-UAV selection is shared over WS and does not change the Video target');

  source = (await state()).executions[sourceId];
  let target = {...source.position, longitude: source.position.longitude - .0005, altitude: source.position.altitude + 10};
  rejected = await send(map, 'execution_group_move', {request_id: crypto.randomUUID(),
    assigned_uav_ids: ['UAV-01'], target}, 409);
  assert.equal(rejected.code, 'DRONE_ACTIVE_PLAN_CONFLICT');
  assert.equal(rejected.params.active_execution_id, sourceId);
  await send(video, 'execution_takeover_move', request(source, target), 403);
  assert.equal((await state()).executions[sourceId].state, 'EXECUTING');
  pass('Ordinary movement cannot steal the assigned UAV and Video cannot request takeover');

  await send(command, 'execution_pause', {request_id: crypto.randomUUID(), execution_id: sourceId,
    control_version: source.control_version});
  snapshot = await state();
  assert(groupItems(snapshot, group).every(e => e.state === 'PAUSED'));
  source = snapshot.executions[sourceId];
  async function rejectedUnchanged(extra, code) {
    const before = await state();
    const result = await send(map, 'execution_takeover_move', extra, 409);
    assert.equal(result.code, code, JSON.stringify(result));
    assert.deepEqual(executionFields(await state()), executionFields(before), `${code} changed execution state`);
  }
  await rejectedUnchanged(request(source, target, {control_version: source.control_version - 1}), 'EXECUTION_STALE');
  const missingVersion = request(source, target); delete missingVersion.control_version;
  await rejectedUnchanged(missingVersion, 'EXECUTION_STALE');
  await rejectedUnchanged(request(source, target, {uav_id: 'UAV-02'}), 'EXECUTION_UAV_MISMATCH');
  await rejectedUnchanged(request(source, target, {assigned_uav_ids: ['UAV-01', 'UAV-02']}), 'INVALID_UAV');
  pass('Stale version, missing version, mismatched UAV and multi-UAV takeover fail atomically');

  await rejectedUnchanged(request(source, {latitude: target.latitude, longitude: target.longitude}), 'INVALID_WAYPOINT');
  await rejectedUnchanged(request(source, {...target, latitude: 81}), 'INVALID_WAYPOINT');
  await rejectedUnchanged(request(source, {...target, latitude: source.position.latitude + 1}), 'TRAJECTORY_DOMAIN_UNSUPPORTED');
  await rejectedUnchanged(request(source, target, {target_area_radius_m: 0}), 'INVALID_SEPARATION');
  await rejectedUnchanged(request(source, target, {target_area_radius_m: 'wide'}), 'INVALID_SEPARATION');
  pass('Invalid coordinates, unsupported range and invalid target radius leave the original task untouched');

  const collisionTarget = snapshot.executions[peerId].position;
  await rejectedUnchanged(request(source, collisionTarget), 'TRAJECTORY_CONFLICT');
  pass('A path ending on a paused peer is rejected before aborting the original execution');

  await send(command, 'begin_configuration');
  await rejectedUnchanged(request(source, target), 'CONFIGURATION_IN_PROGRESS');
  await send(command, 'end_configuration');
  pass('Configuration mode blocks takeover without changing the existing paused task');

  await send(command, 'execution_resume', {request_id: crypto.randomUUID(), execution_id: sourceId,
    control_version: source.control_version});
  snapshot = await state(); source = snapshot.executions[sourceId];
  assert(groupItems(snapshot, group).every(e => e.state === 'EXECUTING'));
  target = {...source.position, longitude: source.position.longitude - .0005, altitude: source.position.altitude + 10};
  const takeoverRequest = request(source, target);
  const executionCount = Object.keys(snapshot.executions).length;
  response = await send(map, 'execution_takeover_move', takeoverRequest);
  const moveId = response.execution_id, old = response.state.executions[sourceId], move = response.state.executions[moveId];
  snapshots.takeover = response.state;
  assert.equal(old.state, 'ABORTED'); assert.equal(old.completion_reason, 'MANUAL_TAKEOVER');
  assert.equal(move.control_kind, 'GROUP_MOVE'); assert.equal(move.source_execution_id, sourceId);
  assert.equal(move.uav_id, 'UAV-01'); assert.notEqual(move.group_id, group);
  assert.equal(groupItems(response.state, move.group_id).length, 1);
  assert.deepEqual(move.position, old.position);
  for (const axis of ['latitude', 'longitude', 'altitude']) {
    assert.equal(move.route_snapshot.waypoints[0][axis], old.position[axis]);
    assert(Math.abs(move.route_snapshot.waypoints.at(-1)[axis] - target[axis]) < 1e-9);
  }
  assert.equal(Object.keys(response.state.executions).length, executionCount + 1);
  assert(groupItems(response.state, group).filter(e => e.execution_id !== sourceId).every(e => e.state === 'EXECUTING'));
  assert.deepEqual(response.state.missions[fixture.mission], originalMission);
  assert.deepEqual(response.state.deployments, originalDeployment);
  pass('Map takeover aborts only the selected member and starts one independent move at its acknowledged position');

  const replay = await send(map, 'execution_takeover_move', takeoverRequest);
  assert.equal(replay.execution_id, moveId);
  assert.equal(Object.keys(replay.state.executions).length, executionCount + 1);
  assert.deepEqual(replay.state.executions[sourceId], old);
  rejected = await send(map, 'execution_takeover_move', {...takeoverRequest,
    target: {...target, altitude: target.altitude + 1}}, 409);
  assert.equal(rejected.code, 'REQUEST_ID_REUSED');
  pass('Retrying the same request returns the same move; changed content cannot reuse its request ID');

  await until(async () => command.events, events => events.some(e => e.type === 'SecurityPlansChanged' &&
    e.payload?.executions?.[sourceId]?.state === 'ABORTED' &&
    e.payload?.executions?.[moveId]?.source_execution_id === sourceId), 'Atomic takeover WS snapshot');
  for (const c of [command, map, video]) {
    await until(async () => c.events, events => events.some(e => e.type === 'SecurityPlansChanged' && e.payload?.executions?.[moveId]),
      `${c.role} receives new move`);
    for (const event of c.events.filter(e => e.type === 'SecurityPlansChanged' && e.payload?.executions?.[moveId]))
      assert.equal(event.payload.executions[sourceId].state, 'ABORTED');
  }
  pass('Command, Map and Video receive a committed WS snapshot with source termination and new movement together');

  snapshot = await until(state, s => s.executions[moveId].state === 'EXECUTING', 'Manual movement starts');
  await send(map, 'execution_pause', {request_id: crypto.randomUUID(), execution_id: moveId,
    control_version: snapshot.executions[moveId].control_version});
  snapshot = await state(); const pausedMove = structuredClone(snapshot.executions[moveId]);
  await send(command, 'execution_pause', {request_id: crypto.randomUUID(), execution_id: peerId,
    control_version: snapshot.executions[peerId].control_version});
  snapshot = await state();
  // Address the old selected execution: only its still-active old group members may resume.
  await send(command, 'execution_resume', {request_id: crypto.randomUUID(), execution_id: sourceId,
    control_version: snapshot.executions[sourceId].control_version});
  await sleep(350); snapshot = await state();
  assert.deepEqual(snapshot.executions[moveId], pausedMove);
  assert.deepEqual(snapshot.executions[sourceId], old);
  assert(groupItems(snapshot, group).filter(e => e.execution_id !== sourceId).every(e => e.state === 'EXECUTING'));
  pass('Resuming the old group cannot resume the new paused movement or revive the taken-over source');

  await send(map, 'execution_resume', {request_id: crypto.randomUUID(), execution_id: moveId,
    control_version: pausedMove.control_version});
  const peerProgress = snapshot.executions[peerId].distance_travelled_m;
  snapshot = await until(state, s => s.executions[moveId].state === 'COMPLETED', 'Manual movement reaches target', 20000);
  assert.equal(snapshot.executions[moveId].completion_reason, 'ROUTE_FINISHED');
  for (const axis of ['latitude', 'longitude', 'altitude'])
    assert(Math.abs(snapshot.executions[moveId].position[axis] - target[axis]) < 1e-8);
  assert.deepEqual(snapshot.executions[sourceId], old);
  assert(snapshot.executions[peerId].distance_travelled_m > peerProgress);
  assert.equal(snapshot.executions[peerId].state, 'EXECUTING');
  pass('Manual movement completes at its target while the other mission members keep progressing');

  await send(command, 'execution_pause', {request_id: crypto.randomUUID(), execution_id: peerId,
    control_version: snapshot.executions[peerId].control_version});
  snapshot = await state(); const fourth = snapshot.executions[fourthId];
  assert.equal(fourth.state, 'PAUSED');
  const secondTarget = {...fourth.position, longitude: fourth.position.longitude + .0005, altitude: fourth.position.altitude + 10};
  response = await send(command, 'execution_takeover_move', request(fourth, secondTarget));
  const secondId = response.execution_id;
  assert.equal(response.state.executions[fourthId].completion_reason, 'MANUAL_TAKEOVER');
  assert.equal(response.state.executions[secondId].source_execution_id, fourthId);
  assert.equal(response.state.executions[peerId].state, 'PAUSED');
  pass('Command can take over a paused deployed member without changing its paused peers');

  snapshot = await until(state, s => s.executions[secondId].state === 'EXECUTING', 'Second manual movement starts');
  await send(map, 'execution_pause', {request_id: crypto.randomUUID(), execution_id: secondId,
    control_version: snapshot.executions[secondId].control_version});
  snapshot = await state();
  await rejectedUnchanged(request(snapshot.executions[secondId], secondTarget), 'EXECUTION_STATE_INVALID');
  await rejectedUnchanged(request(snapshot.executions[sourceId], target), 'EXECUTION_STATE_INVALID');
  pass('A manual movement and a terminal original execution cannot be taken over as deployed tasks');

  const secondPaused = structuredClone(snapshot.executions[secondId]);
  await send(command, 'execution_abort', {request_id: crypto.randomUUID(), execution_id: peerId,
    control_version: snapshot.executions[peerId].control_version});
  snapshot = await state(); assert.deepEqual(snapshot.executions[secondId], secondPaused);
  await send(map, 'execution_abort', {request_id: crypto.randomUUID(), execution_id: secondId,
    control_version: secondPaused.control_version});
  assert.deepEqual(await api('/api/video-view'), videoBefore);
  finalState = await state();
  assert.deepEqual(finalState.missions[fixture.mission], originalMission);
  assert.deepEqual(finalState.deployments, originalDeployment);
  assert(Object.values(finalState.executions).every(e => ['COMPLETED', 'ABORTED', 'CANCELLED'].includes(e.state)));
  pass('Old-group abort remains isolated; manual controls release occupancy without rewriting deployed routes or Video');
} catch (error) {
  failure = error.stack ?? String(error); console.error(failure); process.exitCode = 1;
} finally {
  try {finalState ??= await state();} catch (error) {failure ??= error.stack; process.exitCode = 1;}
  client.close();
  const timeline = client.clients.flatMap(c => c.events.filter(e => e.type === 'SecurityPlansChanged').map(e => ({
    role: c.role, version: e.payload.version, execution_version: e.payload.execution_version,
    executions: Object.values(e.payload.executions ?? {}).map(x => ({execution_id: x.execution_id,
      uav_id: x.uav_id, source_execution_id: x.source_execution_id, group_id: x.group_id,
      state: x.state, control_version: x.control_version, completion_reason: x.completion_reason,
      position: x.position, progress: x.progress})),
  })));
  fs.writeFileSync(path.join(out, 'result.json'), JSON.stringify({pass: !failure, synthetic: true,
    evidence_kind: 'HTTP/WS with isolated Mock backend', native_verified: false, real_hardware_verified: false,
    passed, passed_count: passed.length, failure: failure ?? null}, null, 2));
  for (const [name, value] of Object.entries({traffic: client.traffic, snapshots, timeline, 'final-state': finalState}))
    fs.writeFileSync(path.join(out, `${name}.json`), JSON.stringify(value ?? null, null, 2));
}
