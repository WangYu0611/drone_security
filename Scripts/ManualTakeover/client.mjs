// HTTP/WS test client. Endpoints must be explicitly supplied by the owning runner.
import assert from 'node:assert/strict';

export const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
export async function until(read, predicate, description, timeout = 12000) {
  const deadline = Date.now() + timeout;
  let value;
  do {
    value = await read();
    if (predicate(value)) return value;
    await sleep(50);
  } while (Date.now() < deadline);
  throw new Error(`${description}: ${JSON.stringify(value)}`);
}

export function testClient() {
  assert(process.env.P1_HTTP && process.env.P1_WS,
    'P1_HTTP and P1_WS are required; use verify.py for an owned isolated Backend');
  const base = new URL(process.env.P1_HTTP), ws = new URL(process.env.P1_WS);
  assert(['localhost', '127.0.0.1'].includes(base.hostname) &&
    ['localhost', '127.0.0.1'].includes(ws.hostname), 'Only isolated loopback endpoints are accepted');
  const clients = [], traffic = [];
  async function api(path, body, expected = 200, method = 'POST') {
    const response = await fetch(new URL(path, base), {
      method: body ? method : 'GET',
      headers: {'content-type': 'application/json'},
      body: body ? JSON.stringify(body) : undefined,
      signal: AbortSignal.timeout(8000),
    });
    const data = await response.json();
    // Preserve complete mutations; polling snapshots are retained in dedicated evidence files.
    if (body || response.status !== expected)
      traffic.push({at: new Date().toISOString(), path, method, body, status: response.status, data});
    assert.equal(response.status, expected, `${path}: ${JSON.stringify(data)}`);
    return data;
  }
  async function connect(role) {
    const id = crypto.randomUUID();
    await api('/api/context/clients', {instance_id: id, client_id: `TAKEOVER-QA-${role}`,
      client_role: role, hostname: 'ISOLATED-MOCK', app_version: 'ManualTakeover'});
    const socket = new WebSocket(ws), c = {id, role, socket, events: []};
    clients.push(c);
    socket.onmessage = event => c.events.push(JSON.parse(event.data));
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('WebSocket connection timed out')), 8000);
      socket.onopen = () => {clearTimeout(timeout); resolve();};
      socket.onerror = error => {clearTimeout(timeout); reject(error);};
    });
    socket.send(JSON.stringify({type: 'subscribe_context', instance_id: id}));
    await until(async () => c.events, events => events.some(e => e.type === 'context_subscribed'),
      `Subscribe ${role}`);
    c.timer = setInterval(() => {
      if (socket.readyState === WebSocket.OPEN)
        socket.send(JSON.stringify({type: 'context_ping', hydrated: true}));
    }, 2000);
    return c;
  }
  const state = () => api('/api/security-plans');
  async function send(c, action, extra = {}, expected = 200) {
    return api('/api/security-plans', {instance_id: c.id, expected_version: (await state()).version,
      action, ...extra}, expected);
  }
  function close() {
    for (const c of clients) {clearInterval(c.timer); c.socket.close();}
  }
  return {api, connect, state, send, close, clients, traffic};
}
