// Isolated Mock reproduction of the normal Launcher's Map-role permission gate.
import assert from 'node:assert/strict';
import fs from 'node:fs';
const base = process.env.P1_HTTP;
const out = process.env.FLEET_EVIDENCE;
const expected = Number(process.env.EXPECT_GROUP_STATUS ?? 200);
const traffic = [];
let socket;
async function api(path, body) {
  const response = await fetch(base + path, body ? {
    method: 'POST', headers: {'content-type': 'application/json'}, body: JSON.stringify(body)
  } : {});
  const data = await response.json();
  traffic.push({path, status: response.status, data});
  return {status: response.status, data};
}
try {
  const id = crypto.randomUUID();
  assert.equal((await api('/api/context/clients', {
    instance_id: id, client_id: 'Fleet-backend-compatibility-Map', client_role: 'Map',
    hostname: 'ISOLATED-MOCK', app_version: 'FleetBackendCompatibility'
  })).status, 200);
  socket = new WebSocket(process.env.P1_WS);
  const subscribed = new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(Error('context subscription timeout')), 10000);
    socket.onmessage = event => {
      if (JSON.parse(event.data).type === 'context_subscribed') {clearTimeout(timer); resolve();}
    };
    socket.onerror = reject;
  });
  await new Promise((resolve, reject) => {socket.onopen = resolve; socket.onerror = reject;});
  socket.send(JSON.stringify({type: 'subscribe_context', instance_id: id}));
  await subscribed;
  const before = (await api('/api/security-plans')).data;
  const home = before.mock_execution.uavs['UAV-01'].home_position;
  const response = await api('/api/security-plans', {
    instance_id: id, expected_version: before.version, action: 'execution_group_move',
    request_id: crypto.randomUUID(), assigned_uav_ids: ['UAV-01', 'UAV-02'],
    target: {...home, latitude: home.latitude + .0001, altitude: home.altitude + 50}
  });
  assert.equal(response.status, expected, JSON.stringify(response.data));
  if (expected === 403) {
    assert.equal(response.data.code, 'ROLE_FORBIDDEN');
    assert.deepEqual((await api('/api/security-plans')).data.executions, before.executions);
  }
  console.log(JSON.stringify({expected, actual: response.status, code: response.data.code ?? 'ACCEPTED',
    message: response.data.message ?? '', pass: true}));
} finally {
  socket?.close();
  fs.mkdirSync(out, {recursive: true});
  fs.writeFileSync(out + '/traffic.json', JSON.stringify(traffic, null, 2));
}
