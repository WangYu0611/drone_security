// Run against a newly built Backend with DRONE_BACKEND_EXE set when needed.
// The test owns its Backend, registry, synthetic credentials, and Mock aircraft.
import assert from 'node:assert/strict';
import { createHash, randomUUID } from 'node:crypto';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../../', import.meta.url));
const executable = process.env.DRONE_BACKEND_EXE ??
    path.join(root, 'Backend', 'build', 'Release', 'DroneBackend.exe');
const resultsDirectory = path.join(root, 'Saved', 'PilotControlQA');
fs.mkdirSync(resultsDirectory, { recursive: true });
const runDirectory = fs.mkdtempSync(path.join(resultsDirectory, 'protocol-'));
const credentialPath = path.join(runDirectory, 'credentials.json');
const configPath = path.join(runDirectory, 'config.json');
const registryPath = path.join(runDirectory, 'drones.json');
const mockPath = path.join(runDirectory, 'mock_execution.json');
const controlPath = '/api/pilot-control';
const sleep = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));
const aircraftIds = ['UAV-01', 'UAV-02', 'UAV-03'];
const [firstAircraft, secondAircraft, unassignedAircraft] = aircraftIds;
const command = { token: randomUUID(), role: 'Command', id: 'command-qa' };
const pilot = { token: randomUUID(), role: 'Pilot', id: 'pilot-a-qa' };
const otherPilot = { token: randomUUID(), role: 'Pilot', id: 'pilot-b-qa' };
const controlSockets = [];
const passed = [];
const processIds = [];
let child;

async function reservePort() {
    const server = net.createServer();
    await new Promise((resolve, reject) => {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', resolve);
    });
    return server;
}

const [httpReservation, wsReservation] = await Promise.all([reservePort(), reservePort()]);
const httpPort = httpReservation.address().port;
const wsPort = wsReservation.address().port;
await Promise.all([httpReservation, wsReservation].map(server =>
    new Promise(resolve => server.close(resolve))));
const httpUrl = `http://127.0.0.1:${httpPort}`;
const wsUrl = `ws://127.0.0.1:${wsPort}`;

async function until(read, accept = Boolean, description = 'expected state') {
    const deadline = Date.now() + 7000;
    do {
        const value = await read();
        if (accept(value)) return value;
        await sleep(40);
    } while (Date.now() < deadline);
    assert.fail(`Timed out waiting for ${description}`);
}

async function request(endpoint, { actor, body, status = 200, code } = {}) {
    const headers = {};
    if (actor) headers.Authorization = `Bearer ${actor.token}`;
    if (body) headers['Content-Type'] = 'application/json';
    const response = await fetch(httpUrl + endpoint, {
        headers,
        method: body ? 'POST' : 'GET',
        body: body && JSON.stringify(body),
        signal: AbortSignal.timeout(4000),
    });
    const result = await response.json();
    assert.equal(response.status, status, `${endpoint}: ${JSON.stringify(result)}`);
    if (code) assert.equal(result.code, code, JSON.stringify(result));
    return result;
}

function snapshot(actor = command) {
    return request(controlPath, { actor });
}

function change(actor, action, fields, expected = {}) {
    const body = { action, request_id: randomUUID(), ...fields };
    return request(controlPath, { actor, body, ...expected });
}

function aircraft(state, id) {
    return state.aircraft.find(row => row.uav_id === id);
}

async function assignedState(id, actor = command) {
    const row = aircraft(await snapshot(actor), id);
    assert(row, `Missing ${id} from ${actor.id} snapshot`);
    return row;
}

function send(client, body) {
    assert.equal(client.socket.readyState, WebSocket.OPEN);
    client.socket.send(JSON.stringify(body));
}

async function openSocket(url, actor) {
    const socket = actor
        ? new WebSocket(url, { headers: { Authorization: `Bearer ${actor.token}` } })
        : new WebSocket(url);
    const client = { actor, socket, events: [] };
    controlSockets.push(client);
    socket.addEventListener('message', event => client.events.push(JSON.parse(event.data)));
    const open = await new Promise(resolve => {
        const deadline = setTimeout(() => {
            socket.close();
            resolve(false);
        }, 4000);
        function finish(value) {
            clearTimeout(deadline);
            resolve(value);
        }
        socket.addEventListener('open', () => finish(true), { once: true });
        socket.addEventListener('error', () => finish(false), { once: true });
    });
    return { client, open };
}

async function connect(actor) {
    const { client, open } = await openSocket(`${wsUrl}/ws/pilot-control`, actor);
    assert(open, `Control WebSocket failed to open for ${actor.id}`);
    send(client, { type: 'subscribe_pilot_control' });
    const subscribed = await until(() => client.events.find(event =>
        event.type === 'pilot_control_subscribed'), Boolean, `${actor.id} subscription`);
    assert(subscribed.connection_id, `${actor.id} subscription lacks connection ID`);
    assert(subscribed.payload.server_epoch);
    client.connectionId = subscribed.connection_id;
    return client;
}

async function connectMap() {
    const instanceId = randomUUID();
    await request('/api/context/clients', { body: {
        instance_id: instanceId,
        client_role: 'Map',
        client_id: 'pilot-control-regression-map',
        hostname: 'OWNED-LOOPBACK-QA',
        app_version: 'test',
    } });
    const { client, open } = await openSocket(`${wsUrl}/ws`);
    assert(open, 'Map WebSocket failed to open');
    send(client, { type: 'subscribe_context', instance_id: instanceId });
    await until(() => client.events.some(event => event.type === 'context_subscribed'), Boolean,
        'Map subscription');
    return instanceId;
}

async function mapMove(instanceId, id, target, expected = {}) {
    const plans = await request('/api/security-plans');
    return request('/api/security-plans', {
        body: {
            instance_id: instanceId,
            action: 'execution_group_move',
            request_id: randomUUID(),
            expected_version: plans.version,
            assigned_uav_ids: [id],
            target,
        },
        ...expected,
    });
}

function activate(client, row, expected) {
    return change(client.actor, 'activate', {
        uav_id: row.uav_id,
        grant_id: row.grant_id,
        expected_revision: row.revision,
        connection_id: client.connectionId,
    }, expected);
}

async function inputWindow(client, id) {
    return until(() => client.events.findLast(event => event.type === 'pilot_input_window' &&
        event.control_session_id === id), Boolean, 'input window');
}

async function sessionFor(client, id, previous = '') {
    return until(() => {
        const snapshots = client.events.filter(event => event.payload?.aircraft);
        const latest = snapshots.at(-1);
        const row = latest && aircraft(latest.payload, id);
        if (row?.control_session_id !== previous) return row?.control_session_id;
    }, Boolean, `active session for ${id}`);
}

function input(client, id, sessionId, windowId, sequence, forward = 1) {
    send(client, {
        type: 'pilot_input',
        uav_id: id,
        control_session_id: sessionId,
        window_id: windowId,
        seq: sequence,
        axes: { forward, right: 0, vertical: 0, yaw: 0 },
    });
}

async function pose(id) {
    const slot = Number(id.split('-')[1]);
    const row = (await request('/api/drones')).find(drone => Number(drone.slot) === slot);
    assert(row, `Missing Mock drone telemetry for ${id}`);
    assert.equal(row.simulation, true, `${id} must have simulated telemetry`);
    const coordinates = [row.gps_lat, row.gps_lon, row.gps_alt];
    assert(coordinates.every(Number.isFinite));
    return coordinates;
}

async function move(client, id, activeSessionId) {
    const start = await pose(id);
    let sequence = 0;
    return until(async () => {
        const window = await inputWindow(client, activeSessionId);
        input(client, id, activeSessionId, window.window_id, ++sequence);
        const position = await pose(id);
        return start.some((coordinate, index) => Math.abs(coordinate - position[index]) > 1e-10);
    }, Boolean, `movement of ${id} by authenticated pilot`);
}

async function startBackend() {
    const processLog = fs.openSync(path.join(runDirectory, 'process.log'), 'a');
    try {
        child = spawn(executable, [configPath], {
            cwd: root,
            env: { ...process.env, DRONE_PILOT_CONTROL_CREDENTIALS: credentialPath },
            stdio: ['ignore', processLog, processLog],
            windowsHide: true,
        });
    } finally {
        fs.closeSync(processLog);
    }
    const started = child;
    await new Promise((resolve, reject) => {
        started.once('error', reject);
        started.once('spawn', resolve);
    });
    processIds.push(started.pid);
    await until(async () => {
        assert.equal(started.exitCode, null, 'Owned Backend exited before it became ready');
        try {
            return await request('/');
        } catch {
            return null;
        }
    }, Boolean, 'owned Backend readiness');
}

async function stopBackend() {
    for (const { socket } of controlSockets.splice(0)) socket.close();
    if (child?.pid && child.exitCode === null && child.signalCode === null) {
        const exited = new Promise(resolve => child.once('exit', resolve));
        child.kill();
        await exited;
    }
    child = undefined;
}

function pass(name) {
    passed.push(name);
    console.log(`PASS ${name}`);
}

let failure, executableSha256;
try {
    executableSha256 = createHash('sha256').update(fs.readFileSync(executable)).digest('hex');
    fs.writeFileSync(credentialPath, JSON.stringify({ credentials: [command, pilot, otherPilot] }));
    fs.writeFileSync(configPath, JSON.stringify({
        server: { http_port: httpPort, ws_port: wsPort, debug: false },
        port_map: {},
        storage: { path: registryPath, video_metadata_path: path.join(runDirectory, 'metadata') },
        log: { level: 'info', file: path.join(runDirectory, 'backend.log') },
    }));
    fs.writeFileSync(registryPath, JSON.stringify(aircraftIds.map((id, index) => ({
        id: `d${index + 1}`,
        slot: index + 1,
        name: `Synthetic ${id}`,
        model: 'Mock',
        ip: '127.0.0.1',
        port: 0,
        video_url: '',
    }))));
    fs.copyFileSync(path.join(root, 'Backend', 'mock_execution.json'), mockPath);
    await startBackend();

    await request(controlPath, { status: 401, code: 'UNAUTHENTICATED' });
    await request(controlPath, { actor: { token: randomUUID() }, status: 401, code: 'UNAUTHENTICATED' });
    await request(controlPath, {
        body: { action: 'grant', request_id: randomUUID(), pilot_id: pilot.id, uav_ids: [firstAircraft] },
        status: 401,
        code: 'UNAUTHENTICATED',
    });
    assert.equal((await openSocket(`${wsUrl}/ws/pilot-control`)).open, false);
    const commandSocket = await connect(command);
    let pilotSocket = await connect(pilot);
    const otherSocket = await connect(otherPilot);
    const secondPilotConnection = await connect(pilot);
    pass('Unauthenticated HTTP and WebSocket requests are rejected');

    const original = await snapshot();
    const idsToGrant = [firstAircraft, secondAircraft, 'UAV-99'];
    const revisions = Object.fromEntries(idsToGrant.map(id => [id, aircraft(original, id)?.revision ?? 0]));
    const grant = {
        action: 'grant',
        request_id: randomUUID(),
        pilot_id: pilot.id,
        uav_ids: idsToGrant,
        expected_revisions: revisions,
    };
    const result = await request(controlPath, { actor: command, body: grant });
    for (const id of [firstAircraft, secondAircraft]) {
        assert.equal(result.results.find(item => item.uav_id === id)?.ok, true);
    }
    assert.equal(result.results.find(item => item.uav_id === 'UAV-99')?.ok, false);
    const repeat = await request(controlPath, { actor: command, body: grant });
    assert.deepEqual(repeat.results, result.results);
    await request(controlPath, {
        actor: command,
        body: { ...grant, pilot_id: otherPilot.id },
        status: 409,
        code: 'REQUEST_ID_REUSED',
    });
    const firstGrant = await assignedState(firstAircraft, pilot);
    const secondGrant = await assignedState(secondAircraft, pilot);
    assert(firstGrant.grant_id && secondGrant.grant_id && firstGrant.grant_id !== secondGrant.grant_id);
    await until(() => pilotSocket.events.some(event => event.type === 'pilot_control_changed' &&
        aircraft(event.payload, firstAircraft)?.grant_id === firstGrant.grant_id), Boolean, 'pilot grant event');
    assert.equal((await snapshot(otherPilot)).aircraft.length, 0);
    await until(() => otherSocket.events.some(event => event.payload?.version >= result.version),
        Boolean, 'other pilot snapshot for the grant revision');
    assert(!otherSocket.events.some(event => event.payload?.aircraft?.some(row => row.grant_id === firstGrant.grant_id)));
    pass('Batch grants are independent, idempotent, and visible only to the authorized pilot');

    const mapInstance = await connectMap();
    await mapMove(mapInstance, firstAircraft, { latitude: 1.279545, longitude: 103.862188, altitude: 25 },
        { status: 409, code: 'PILOT_CONTROL_ASSIGNED' });
    pass('An existing Map move cannot claim an aircraft already granted to a pilot');

    await change(pilot, 'grant', {
        pilot_id: pilot.id,
        uav_ids: [unassignedAircraft],
        expected_revisions: { [unassignedAircraft]: aircraft(original, unassignedAircraft).revision },
    }, { status: 403, code: 'ROLE_FORBIDDEN' });
    await activate(otherSocket, firstGrant, { status: 403 });
    await activate(commandSocket, firstGrant, { status: 403, code: 'ROLE_FORBIDDEN' });
    await change(otherPilot, 'revoke', { uav_id: firstAircraft, grant_id: firstGrant.grant_id },
        { status: 403, code: 'ROLE_FORBIDDEN' });
    pass('Pilots cannot grant or revoke and unrelated principals cannot activate a grant');

    const secondBefore = await pose(secondAircraft);
    await activate(pilotSocket, firstGrant);
    const firstSession = await sessionFor(pilotSocket, firstAircraft);
    const firstWindow = await inputWindow(pilotSocket, firstSession);
    await activate(pilotSocket, secondGrant, { status: 409, code: 'PILOT_BUSY' });
    await activate(secondPilotConnection, secondGrant, { status: 409, code: 'PILOT_BUSY' });
    input(secondPilotConnection, firstAircraft, firstSession, firstWindow.window_id, 9000);
    await until(() => secondPilotConnection.events.some(event =>
        event.type === 'pilot_input_rejected' && event.code === 'INPUT_REJECTED'), Boolean,
    'input rejection from another connection of the same pilot');
    await move(pilotSocket, firstAircraft, firstSession);
    assert.deepEqual(await pose(secondAircraft), secondBefore);
    await change(pilot, 'stop', { control_session_id: firstSession, reason: 'SWITCH' });
    assert.equal((await assignedState(firstAircraft, pilot)).control_state, 'IDLE');
    const firstStopped = await pose(firstAircraft);
    input(pilotSocket, firstAircraft, firstSession, firstWindow.window_id, 10000);

    await activate(pilotSocket, await assignedState(secondAircraft, pilot));
    const secondSession = await sessionFor(pilotSocket, secondAircraft);
    assert.notEqual(firstSession, secondSession);
    await move(pilotSocket, secondAircraft, secondSession);
    assert.deepEqual(await pose(firstAircraft), firstStopped);
    pass('Only one aircraft moves at a time and switching invalidates the previous input session');

    const secondWindow = await inputWindow(pilotSocket, secondSession);
    await change(command, 'revoke', { uav_id: secondAircraft, grant_id: secondGrant.grant_id });
    const revokedPose = await pose(secondAircraft);
    const previousEvents = pilotSocket.events.length;
    for (let seq = 20000; seq < 20004; ++seq) {
        input(pilotSocket, secondAircraft, secondSession, secondWindow.window_id, seq);
        await sleep(70);
        assert.deepEqual(await pose(secondAircraft), revokedPose);
    }
    await until(() => pilotSocket.events.slice(previousEvents).some(event =>
        event.type === 'pilot_input_rejected' && event.code === 'INPUT_REJECTED'), Boolean,
    'revoked input rejection');
    assert.equal(aircraft(await snapshot(pilot), secondAircraft), undefined);
    assert.equal((await assignedState(secondAircraft)).control_state, 'IDLE');
    pass('Revocation fences old inputs and stops Mock motion');

    const [latitude, longitude, altitude] = revokedPose;
    const mapHandoff = await mapMove(mapInstance, secondAircraft, {
        latitude: latitude + 0.00005, longitude, altitude: altitude + 5,
    });
    const mapExecution = mapHandoff.state.executions[mapHandoff.execution_id];
    const routeStart = mapExecution.route_snapshot.waypoints[0];
    assert.deepEqual([routeStart.latitude, routeStart.longitude, routeStart.altitude], revokedPose);
    await until(async () => (await request('/api/security-plans')).executions[mapHandoff.execution_id],
        execution => execution?.state === 'COMPLETED', 'Map move completion after handoff');
    pass('Map movement after revocation starts at the last Pilot pose');

    await change(command, 'grant', {
        pilot_id: otherPilot.id,
        uav_ids: [secondAircraft],
        expected_revisions: { [secondAircraft]: (await assignedState(secondAircraft)).revision },
    });
    const replacementGrant = await assignedState(secondAircraft, otherPilot);
    assert.notEqual(replacementGrant.grant_id, secondGrant.grant_id);
    await change(command, 'revoke', { uav_id: secondAircraft, grant_id: secondGrant.grant_id },
        { status: 409, code: 'GRANT_CHANGED' });
    assert.equal((await assignedState(secondAircraft, otherPilot)).grant_id, replacementGrant.grant_id);
    pass('A stale revocation cannot revoke a replacement grant');

    await activate(pilotSocket, await assignedState(firstAircraft, pilot));
    const beforeDisconnect = await sessionFor(pilotSocket, firstAircraft, firstSession);
    const disconnectWindow = await inputWindow(pilotSocket, beforeDisconnect);
    const previousConnection = pilotSocket.connectionId;
    pilotSocket.socket.close();
    await until(() => assignedState(firstAircraft), state => state.control_state === 'IDLE', 'disconnect cleanup');
    pilotSocket = await connect(pilot);
    assert.notEqual(pilotSocket.connectionId, previousConnection);
    const restored = await assignedState(firstAircraft, pilot);
    assert.equal(restored.grant_id, firstGrant.grant_id);
    assert.equal(restored.control_state, 'IDLE');
    const disconnectedPose = await pose(firstAircraft);
    input(pilotSocket, firstAircraft, beforeDisconnect, disconnectWindow.window_id, 30000);
    await sleep(200);
    assert.deepEqual(await pose(firstAircraft), disconnectedPose);
    await activate(pilotSocket, restored);
    const afterReconnect = await sessionFor(pilotSocket, firstAircraft);
    assert.notEqual(afterReconnect, beforeDisconnect);
    pass('Pilot reconnect restores the grant and requires a fresh activation session');

    const unusedWindow = await inputWindow(pilotSocket, afterReconnect);
    await until(() => snapshot(pilot), state => state.last_exit?.reason === 'INPUT_TIMEOUT' &&
        aircraft(state, firstAircraft)?.control_state === 'IDLE', 'idle input timeout');
    assert.equal(pilotSocket.socket.readyState, WebSocket.OPEN);
    const timedOutPose = await pose(firstAircraft);
    const beforeExpiredInput = pilotSocket.events.length;
    input(pilotSocket, firstAircraft, afterReconnect, unusedWindow.window_id, 40000);
    await until(() => pilotSocket.events.slice(beforeExpiredInput).some(event =>
        event.type === 'pilot_input_rejected'), Boolean, 'expired input rejection');
    assert.deepEqual(await pose(firstAircraft), timedOutPose);
    await activate(pilotSocket, await assignedState(firstAircraft, pilot));
    assert.notEqual(await sessionFor(pilotSocket, firstAircraft, afterReconnect), afterReconnect);
    pass('An open WebSocket cannot preserve expired control or reactivate on late input');

    const previousEpoch = (await snapshot()).server_epoch;
    await stopBackend();
    await startBackend();
    pilotSocket = await connect(pilot);
    const restarted = await snapshot(pilot);
    assert.notEqual(restarted.server_epoch, previousEpoch);
    assert.equal(aircraft(restarted, firstAircraft).grant_id, firstGrant.grant_id);
    assert.equal(aircraft(restarted, firstAircraft).control_state, 'IDLE');
    assert(!aircraft(restarted, firstAircraft).control_session_id);
    assert.equal(aircraft(restarted, secondAircraft), undefined);
    assert.equal((await assignedState(secondAircraft, otherPilot)).grant_id, replacementGrant.grant_id);
    const replayAfterRestart = await request(controlPath, { actor: command, body: grant });
    assert.deepEqual(replayAfterRestart.results, result.results);
    assert.equal((await assignedState(secondAircraft, otherPilot)).grant_id, replacementGrant.grant_id);
    pass('Backend restart retains durable grants and revocations while expiring active sessions');

    await stopBackend();
    const inactiveMockPath = `${mockPath}.inactive`;
    fs.renameSync(mockPath, inactiveMockPath);
    try {
        await startBackend();
        assert(!(await request('/')).capabilities.includes('pilot_control_mock'));
        await request(controlPath, { actor: command, status: 404, code: 'PILOT_CONTROL_DISABLED' });
    } finally {
        fs.renameSync(inactiveMockPath, mockPath);
    }
    pass('Configured credentials do not enable Pilot control without an explicit Mock adapter');
} catch (error) {
    failure = String(error.stack ?? error);
    console.error(failure);
    process.exitCode = 1;
} finally {
    await stopBackend();
    fs.rmSync(credentialPath, { force: true });
    fs.writeFileSync(path.join(runDirectory, 'result.json'), JSON.stringify({
        scope: 'Owned Backend with synthetic Mock aircraft and protocol clients',
        executableSha256,
        httpPort,
        wsPort,
        processIds,
        passed,
        result: failure ? 'FAIL' : 'PASS',
        failure,
    }, null, 2));
    console.log(`Evidence: ${runDirectory}`);
}
