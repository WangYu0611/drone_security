// API preparation for protocol or native takeover acceptance; preparation is not native evidence.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {testClient, until} from './client.mjs';

export async function deployedFixture(client, command, map, members = ['UAV-01', 'UAV-02'], speed = 2) {
  const {state, send} = client;
  const initial = await state();
  assert.equal(initial.execution_adapter, 'Mock', 'Fixture requires a Mock Backend');
  assert.equal(Object.keys(initial.executions).length, 0, 'Fixture requires a fresh execution store');
  assert.equal(Object.keys(initial.plans).length, 0, 'Fixture requires a fresh plan store');
  const homes = members.map(id => initial.mock_execution.uavs[id].home_position);
  const center = Object.fromEntries(['latitude', 'longitude', 'altitude'].map(axis =>
    [axis, homes.reduce((sum, p) => sum + p[axis], 0) / homes.length]));
  const scale = Math.PI / 180 * 6371000 * Math.cos(center.latitude * Math.PI / 180);
  const spacing = members.length > 1 ? (homes.at(-1).longitude - homes[0].longitude) * scale / (members.length - 1) : 3;
  assert(spacing >= 1.5, 'Fixture needs homes ordered west to east with safe spacing');
  let response = await send(command, 'create_plan', {
    name: 'Manual takeover acceptance', default_mission_name: 'Long formation patrol',
  });
  const plan = response.plan_id, mission = response.mission_id;
  const ids = {plan_id: plan, mission_id: mission};
  await send(command, 'assign', {...ids, assigned_uav_ids: members, formation: 'Line', spacing_m: spacing});
  response = await send(map, 'begin_route_edit', {...ids, content_revision: (await state()).plans[plan].content_revision});
  const route = {pathId: 1, bClosedLoop: false, waypoints: [0, .008].map((delta, index) => ({
    sequence: index + 1, ...center, latitude: center.latitude + delta,
    altitude_reference: 'Ellipsoid', segmentSpeed: speed, waitTime: 0,
  }))};
  await send(map, 'save_route', {...ids, edit_session_id: response.edit_session_id, path: route});
  for (const action of ['validate', 'review', 'deploy']) response = await send(command, action, ids);
  assert.equal(response.state.plans[plan].status, 'DEPLOYED');
  assert.equal(Object.keys(response.state.executions).length, 0, 'Deployment must not start movement');
  return {plan, mission, members, route, ids};
}

async function main() {
  assert(process.env.MANUAL_TAKEOVER_EVIDENCE, 'Set MANUAL_TAKEOVER_EVIDENCE to a fresh evidence directory');
  const out = path.resolve(process.env.MANUAL_TAKEOVER_EVIDENCE);
  fs.mkdirSync(out, {recursive: true});
  assert(!fs.existsSync(path.join(out, 'fixture.json')), 'Preserve previous fixture evidence; choose a fresh directory');
  const client = testClient();
  try {
    const command = await client.connect('Command'), map = await client.connect('Map');
    const fixture = await deployedFixture(client, command, map);
    const started = await client.send(command, 'execution_start', {...fixture.ids, request_id: crypto.randomUUID()});
    const execution = started.execution_id, group = started.state.executions[execution].group_id;
    const snapshot = await until(client.state, s => Object.values(s.executions).filter(e => e.group_id === group)
      .every(e => e.state === 'EXECUTING'), 'Fixture formation starts');
    const result = {...fixture, execution, group,
      executions: Object.fromEntries(Object.values(snapshot.executions).filter(e => e.group_id === group)
        .map(e => [e.uav_id, e.execution_id])),
      synthetic_preparation: true, native_verified: false, backend_left_running: true};
    fs.writeFileSync(path.join(out, 'fixture.json'), JSON.stringify(result, null, 2));
    fs.writeFileSync(path.join(out, 'fixture-state.json'), JSON.stringify(snapshot, null, 2));
    console.log(JSON.stringify(result, null, 2));
  } finally {
    client.close();
    fs.writeFileSync(path.join(out, 'fixture-traffic.json'), JSON.stringify(client.traffic, null, 2));
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch(error => {console.error(error.stack); process.exitCode = 1;});
}
