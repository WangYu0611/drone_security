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
const aircraftIds = ['UAV-01', 'UAV-02', 'UAV-03', 'UAV-04'];
const [firstAircraft, secondAircraft, unassignedAircraft, wingAircraft] = aircraftIds;
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

async function connectContext(role, handoffEnabled = true) {
    const instanceId = randomUUID();
    await request('/api/context/clients', { body: {
        instance_id: instanceId,
        client_role: role,
        client_id: `pilot-control-regression-${role.toLowerCase()}`,
        hostname: 'OWNED-LOOPBACK-QA',
        app_version: 'test',
    } });
    const { client, open } = await openSocket(`${wsUrl}/ws`);
    assert(open, `${role} WebSocket failed to open`);
    send(client, { type: 'subscribe_context', instance_id: instanceId });
    const subscribed = await until(() => client.events.find(event => event.type === 'context_subscribed'),
        Boolean, `${role} subscription`);
    assert.equal(subscribed.pilot_handoff_requests, handoffEnabled);
    return { ...client, instanceId };
}

async function connectMap() {
    return (await connectContext('Map')).instanceId;
}

function requestPilotHandoff(client, id, requestId = randomUUID(), expected = {}, extra = {}) {
    return request('/api/security-plans', {
        body: { instance_id: client.instanceId, action: 'request_pilot_handoff', uav_id: id, request_id: requestId, ...extra },
        ...expected,
    });
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

async function startGroupedHandoffMission(commandClient, mapClient) {
    let planId, missionId;
    async function planAction(client, action, fields = {}) {
        const current = await request('/api/security-plans');
        return request('/api/security-plans', { body: {
            instance_id: client.instanceId,
            action,
            plan_id: planId,
            mission_id: missionId,
            expected_version: current.version,
            ...fields,
        } });
    }
    const created = await planAction(commandClient, 'create_plan', {
        name: 'Pilot handoff isolation test', default_mission_name: 'Two-aircraft mission',
    });
    ({ plan_id: planId, mission_id: missionId } = created);
    await planAction(commandClient, 'assign', {
        assigned_uav_ids: [unassignedAircraft, wingAircraft], formation: 'Line', spacing_m: 3,
    });
    const setup = await request('/api/security-plans');
    const editing = await planAction(mapClient, 'begin_route_edit', {
        content_revision: setup.plans[planId].content_revision, workflow: true,
    });
    const [latitude, longitude, altitude] = await pose(unassignedAircraft);
    const [, wingLongitude] = await pose(wingAircraft);
    const route = {
        pathId: 1,
        bClosedLoop: false,
        waypoints: [0.00002, 0.002, 0.004].map((offset, index) => ({
            sequence: index + 1,
            latitude: latitude + offset,
            longitude: (longitude + wingLongitude) / 2,
            altitude: altitude + 1,
            segmentSpeed: 2,
            waitTime: 0,
        })),
    };
    await planAction(mapClient, 'save_route', { edit_session_id: editing.edit_session_id, path: route, keep_editing: true });
    await planAction(mapClient, 'finish_route_edit', { edit_session_id: editing.edit_session_id });
    await planAction(commandClient, 'validate');
    await planAction(commandClient, 'review');
    await planAction(commandClient, 'deploy');
    const started = await planAction(commandClient, 'execution_start', { request_id: randomUUID() });
    return Object.values(started.state.executions).filter(row => row.mission_id === missionId);
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

function input(client, id, sessionId, windowId, sequence, axes = { forward: 1 }, action) {
    send(client, {
        type: 'pilot_input',
        uav_id: id,
        control_session_id: sessionId,
        window_id: windowId,
        seq: sequence,
        axes: { forward: 0, right: 0, vertical: 0, yaw: 0, ...axes },
        ...(action === undefined ? {} : { action }),
    });
}

function pilotInputStream(client, id, sessionId) {
    let sequence = 0;
    return async (axes = {}, action) => {
        const window = await inputWindow(client, sessionId);
        input(client, id, sessionId, window.window_id, ++sequence, axes, action);
    };
}

async function sustainInput(sendFrame, durationMs, axes = {}) {
    const deadline = performance.now() + durationMs;
    do {
        await sendFrame(axes);
        await sleep(60);
    } while (performance.now() < deadline);
}

async function mockDrone(id) {
    const slot = Number(id.split('-')[1]);
    const row = (await request('/api/drones')).find(drone => Number(drone.slot) === slot);
    assert(row, `Missing Mock drone telemetry for ${id}`);
    assert.equal(row.simulation, true, `${id} must have simulated telemetry`);
    return row;
}

async function pose(id) {
    const row = await mockDrone(id);
    const coordinates = [row.gps_lat, row.gps_lon, row.gps_alt];
    assert(coordinates.every(Number.isFinite));
    return coordinates;
}

function offsetMetres([homeLat, homeLon, homeAlt], [latitude, longitude, altitude]) {
    const metresPerDegree = 6371000 * Math.PI / 180;
    return [
        (latitude - homeLat) * metresPerDegree,
        (longitude - homeLon) * metresPerDegree * Math.cos(homeLat * Math.PI / 180),
        altitude - homeAlt,
    ];
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

    const videoContext = await connectContext('Video');
    const videoRequestId = randomUUID();
    await requestPilotHandoff(videoContext, firstAircraft, videoRequestId,
        { status: 409, code: 'PILOT_HANDOFF_NO_COMMAND' });
    const commandContext = await connectContext('Command');
    const mapContext = await connectContext('Map');
    const beforeHandoffRequests = await snapshot();
    const beforeHandoffPlans = await request('/api/security-plans');
    await requestPilotHandoff(commandContext, firstAircraft, randomUUID(), { status: 403, code: 'ROLE_FORBIDDEN' });
    await requestPilotHandoff(videoContext, 'UAV-1', randomUUID(), { status: 409, code: 'PILOT_HANDOFF_UNAVAILABLE' });
    await requestPilotHandoff(videoContext, firstAircraft, 'malformed', { status: 400, code: 'REQUEST_REJECTED' });
    const videoReceipt = await requestPilotHandoff(videoContext, firstAircraft, videoRequestId, {},
        { client_role: 'Command', pilot_id: pilot.id, grant_id: randomUUID() });
    assert.deepEqual(videoReceipt.request, { uav_id: firstAircraft, request_id: videoRequestId, source_role: 'Video' });
    assert.deepEqual(await requestPilotHandoff(videoContext, firstAircraft, videoRequestId), videoReceipt);
    await requestPilotHandoff(videoContext, secondAircraft, videoRequestId,
        { status: 409, code: 'REQUEST_ID_REUSED' });
    await requestPilotHandoff(videoContext, firstAircraft, randomUUID(),
        { status: 409, code: 'PILOT_HANDOFF_RATE_LIMITED' });
    const mapReceipt = await requestPilotHandoff(mapContext, secondAircraft);
    assert.equal(mapReceipt.request.source_role, 'Map');
    const notices = client => client.events.filter(event => event.type === 'PilotHandoffRequested');
    await until(() => notices(commandContext).some(event => event.payload.request_id === mapReceipt.request.request_id),
        Boolean, 'Command handoff-request notification');
    assert.deepEqual(notices(commandContext).map(event => event.payload), [videoReceipt.request, mapReceipt.request]);
    for (const client of [videoContext, mapContext, commandSocket, pilotSocket, otherSocket]) {
        assert.deepEqual(notices(client), []);
    }
    await request(controlPath, { body: { action: 'grant', request_id: randomUUID(), pilot_id: pilot.id,
        uav_ids: [firstAircraft], instance_id: videoContext.instanceId, client_role: 'Command' },
    status: 401, code: 'UNAUTHENTICATED' });
    assert.deepEqual(await snapshot(), beforeHandoffRequests);
    assert.deepEqual(await request('/api/security-plans'), beforeHandoffPlans);
    pass('Video and Map handoff requests notify only Command, deduplicate retries, and cannot grant or change plans');

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

    const firstHome = await pose(firstAircraft);
    const secondBefore = await pose(secondAircraft);
    const idleEvents = pilotSocket.events.length;
    input(pilotSocket, firstAircraft, randomUUID(), 1, 1, { forward: 1 }, 'takeoff');
    await until(() => pilotSocket.events.slice(idleEvents).some(event =>
        event.type === 'pilot_input_rejected' && event.code === 'INPUT_REJECTED'), Boolean,
    'input rejection before the assigned pilot activates');
    assert.deepEqual(await pose(firstAircraft), firstHome);

    await activate(pilotSocket, firstGrant);
    const firstSession = await sessionFor(pilotSocket, firstAircraft);
    const firstWindow = await inputWindow(pilotSocket, firstSession);
    await activate(pilotSocket, secondGrant, { status: 409, code: 'PILOT_BUSY' });
    await activate(secondPilotConnection, secondGrant, { status: 409, code: 'PILOT_BUSY' });
    const unauthorizedSources = [
        [commandSocket, 'error', 'ROLE_FORBIDDEN', 'Command connection'],
        [otherSocket, 'pilot_input_rejected', 'INPUT_REJECTED', 'unassigned Pilot identity'],
        [secondPilotConnection, 'pilot_input_rejected', 'INPUT_REJECTED', 'another connection of the assigned Pilot'],
    ];
    await Promise.all(unauthorizedSources.map(async ([client, type, code, label]) => {
        const eventCount = client.events.length;
        input(client, firstAircraft, firstSession, firstWindow.window_id, 9000, { forward: 1 }, 'takeoff');
        await until(() => client.events.slice(eventCount).some(event => event.type === type && event.code === code),
            Boolean, `input rejection from ${label}`);
    }));
    assert.deepEqual(await pose(firstAircraft), firstHome);
    pass('Only the assigned Pilot connection with an active session can send stick or flight actions');
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
        input(pilotSocket, secondAircraft, secondSession, secondWindow.window_id, seq, { forward: 1 }, 'takeoff');
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

    await activate(otherSocket, replacementGrant);
    const envelopeSession = await sessionFor(otherSocket, secondAircraft);
    const envelopeInput = pilotInputStream(otherSocket, secondAircraft, envelopeSession);
    const outward = { forward: 1, vertical: 1 };
    const envelopeStart = await pose(secondAircraft);
    const envelopeStartTime = performance.now();
    let previousSample = envelopeStart;
    const boundedPose = await until(async () => {
        await envelopeInput(outward);
        const sample = await pose(secondAircraft);
        assert(Math.hypot(...offsetMetres(previousSample, sample)) <= 0.5,
            'Mock Pilot translation exceeded the single-step limit');
        previousSample = sample;
        const [north, east, elevation] = offsetMetres(secondBefore, sample);
        assert(Math.hypot(north, east) <= 6.02, 'Mock Pilot crossed the horizontal operating radius');
        assert(elevation <= 6.02, 'Mock Pilot crossed the altitude ceiling');
        return Math.hypot(north, east) >= 5.96 && elevation >= 5.96 ? sample : null;
    }, Boolean, 'both horizontal and vertical Mock operating limits');
    const travelTime = (performance.now() - envelopeStartTime) / 1000;
    assert(Math.hypot(...offsetMetres(envelopeStart, boundedPose)) <= 0.8 * (travelTime + 0.15) + 0.03,
        'Mock Pilot translation exceeded the 0.8 m/s envelope');
    await sustainInput(envelopeInput, 300, outward);
    const [edgeNorth, edgeEast, edgeElevation] = offsetMetres(secondBefore, await pose(secondAircraft));
    assert(Math.hypot(edgeNorth, edgeEast) <= 6.02 && edgeElevation <= 6.02);
    await change(otherPilot, 'stop', { control_session_id: envelopeSession, reason: 'TEST_COMPLETE' });
    pass('Continuous Mock Pilot input respects horizontal, altitude, speed, and step limits');

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
    const actionSession = await sessionFor(pilotSocket, firstAircraft, afterReconnect);
    assert.notEqual(actionSession, afterReconnect);
    pass('An open WebSocket cannot preserve expired control or reactivate on late input');

    const actionInput = pilotInputStream(pilotSocket, firstAircraft, actionSession);
    const drive = (sendFrame, axes, reached, description) => until(async () => {
        await sendFrame(axes);
        return pose(firstAircraft);
    }, reached, description);
    const height = sample => sample[2] - firstHome[2];
    const homeDistance = sample => Math.hypot(...offsetMetres(firstHome, sample).slice(0, 2));
    const actionEvents = pilotSocket.events.length;
    await actionInput({}, 'unrecognized');
    await until(() => pilotSocket.events.slice(actionEvents).some(event =>
        event.type === 'pilot_input_rejected'), Boolean, 'invalid Pilot action rejection');

    await actionInput({}, 'takeoff');
    await drive(actionInput, {}, sample => height(sample) >= 0.35, 'first takeoff ascent');
    await actionInput({}, 'hold');
    await actionInput({}, 'none');
    await sustainInput(actionInput, 180);
    const heldPose = await pose(firstAircraft);
    assert(height(heldPose) >= 0.35 && height(heldPose) < 1.8);
    await sustainInput(actionInput, 320);
    assert.deepEqual(await pose(firstAircraft), heldPose, 'Hold must cancel the remaining takeoff target');

    await actionInput({}, 'takeoff');
    await drive(actionInput, {}, sample => Math.abs(height(sample) - 2) <= 0.03, '2 m takeoff target');
    await drive(actionInput, { forward: 1 }, sample => homeDistance(sample) >= 0.6, 'departure from home');
    await actionInput({}, 'gohome');
    await drive(actionInput, {}, sample => homeDistance(sample) <= 0.03 &&
        Math.abs(height(sample) - 2) <= 0.03, 'return over home at takeoff altitude');

    await actionInput({}, 'land');
    await drive(actionInput, {}, sample => Math.abs(height(sample)) <= 0.03, 'landing at home elevation');
    await sustainInput(actionInput, 300, { vertical: -1 });
    assert(Math.abs(height(await pose(firstAircraft))) <= 0.03, 'Downward input must respect home elevation');
    pass('One-shot takeoff, hold, return home, and land update the simulated position targets');

    await drive(actionInput, { vertical: 1 }, sample => height(sample) >= 0.25, 'motion before emergency stop');
    await actionInput({}, 'estop');
    await sustainInput(actionInput, 180, { forward: 1, vertical: 1 });
    const emergencyPose = await pose(firstAircraft);
    await actionInput({}, 'takeoff');
    await sustainInput(actionInput, 350, { forward: 1, vertical: 1 });
    assert.deepEqual(await pose(firstAircraft), emergencyPose);
    const emergencyState = await assignedState(firstAircraft, pilot);
    assert.equal(emergencyState.control_state, 'ACTIVE');
    assert.equal(emergencyState.input_frozen, true);
    await change(pilot, 'stop', { control_session_id: actionSession, reason: 'TEST_COMPLETE' });
    await activate(pilotSocket, await assignedState(firstAircraft, pilot));
    const recoveredSession = await sessionFor(pilotSocket, firstAircraft, actionSession);
    assert.equal((await assignedState(firstAircraft, pilot)).input_frozen, false);
    const recoveredInput = pilotInputStream(pilotSocket, firstAircraft, recoveredSession);
    await until(async () => {
        await recoveredInput({ yaw: 1 });
        return (await mockDrone(firstAircraft)).yaw;
    }, yaw => yaw >= 85 && yaw <= 105, 'eastward pilot heading');
    const beforeForward = await pose(firstAircraft);
    const afterForward = await drive(recoveredInput, { forward: 1 },
        sample => offsetMetres(beforeForward, sample)[1] >= 0.25, 'eastward body-frame movement');
    assert(Math.abs(offsetMetres(beforeForward, afterForward)[0]) < 0.1);
    pass('Emergency stop ignores subsequent inputs until fresh activation, then movement follows aircraft heading');

    await sleep(480);
    const staleInputPose = await pose(firstAircraft);
    assert.equal((await assignedState(firstAircraft, pilot)).control_state, 'ACTIVE');
    await sleep(100);
    assert.deepEqual(await pose(firstAircraft), staleInputPose, 'Stale input must freeze motion before session expiry');
    pass('Stale Mock Pilot input freezes motion while its session remains active');

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

    const handoffCommand = await connectContext('Command');
    const handoffMap = await connectContext('Map');
    const selectedPilot = await connect(otherPilot);
    const groupMission = await startGroupedHandoffMission(handoffCommand, handoffMap);
    assert.equal(groupMission.length, 2);
    const selectedMission = groupMission.find(row => row.uav_id === unassignedAircraft);
    const wingMission = groupMission.find(row => row.uav_id === wingAircraft);
    assert(selectedMission && wingMission);
    assert.equal(selectedMission.group_id, wingMission.group_id);
    const handoff = await until(() => assignedState(unassignedAircraft), row =>
        row.handoff_execution_id === selectedMission.execution_id, 'active mission handoff candidate');
    const details = {
        uav_id: unassignedAircraft,
        pilot_id: otherPilot.id,
        execution_id: handoff.handoff_execution_id,
        control_version: handoff.handoff_control_version,
        expected_revision: handoff.revision,
    };
    const missionSnapshot = () => request('/api/security-plans').then(state => state.executions);
    await change(pilot, 'handoff_grant', details, { status: 403, code: 'ROLE_FORBIDDEN' });
    await change(command, 'handoff_grant', { ...details, control_version: details.control_version + 1 },
        { status: 409, code: 'HANDOFF_STALE' });
    const offline = await change(command, 'handoff_grant', { ...details, pilot_id: 'offline-pilot' });
    assert.equal(offline.results[0].ok, false);
    assert.equal(offline.results[0].code, 'PILOT_OFFLINE');
    assert.equal((await missionSnapshot())[selectedMission.execution_id].state, 'EXECUTING');
    assert.equal((await assignedState(unassignedAircraft)).grant_id, null);
    pass('An offline pilot, stale execution, or unprivileged actor cannot release the active mission');

    const approval = { action: 'handoff_grant', request_id: randomUUID(), ...details };
    const granted = await request(controlPath, { actor: command, body: approval });
    assert.equal(granted.results[0].code, 'GRANTED');
    assert.deepEqual(await request(controlPath, { actor: command, body: approval }), granted);
    const executionsAfterHandoff = await missionSnapshot();
    const released = executionsAfterHandoff[selectedMission.execution_id];
    const continuing = executionsAfterHandoff[wingMission.execution_id];
    assert.equal(released.state, 'ABORTED');
    assert.equal(released.completion_reason, 'MANUAL_TAKEOVER');
    assert.equal(continuing.state, 'EXECUTING');
    assert.deepEqual(continuing.route_snapshot, wingMission.route_snapshot);
    assert.equal(Object.values(executionsAfterHandoff).filter(row => row.mission_id === selectedMission.mission_id).length, 2);
    await until(missionSnapshot, state => state[wingMission.execution_id].progress > continuing.progress,
        'unhanded-off group member to continue');
    const newGrant = await assignedState(unassignedAircraft, otherPilot);
    assert.equal(newGrant.control_state, 'IDLE');
    assert(!newGrant.control_session_id);
    assert(!aircraft(await snapshot(pilot), unassignedAircraft));
    assert.deepEqual(await pose(unassignedAircraft), [released.position.latitude, released.position.longitude, released.position.altitude]);
    await activate(pilotSocket, newGrant, { status: 403, code: 'ROLE_FORBIDDEN' });
    await activate(selectedPilot, newGrant);
    const handoffSession = await sessionFor(selectedPilot, unassignedAircraft);
    await move(selectedPilot, unassignedAircraft, handoffSession);
    pass('Command hands off only the selected mission member, then only the designated Pilot can activate and move it');

    await stopBackend();
    const inactiveMockPath = `${mockPath}.inactive`;
    fs.renameSync(mockPath, inactiveMockPath);
    try {
        await startBackend();
        assert(!(await request('/')).capabilities.includes('pilot_control_mock'));
        await request(controlPath, { actor: command, status: 404, code: 'PILOT_CONTROL_DISABLED' });
        const unsupportedVideo = await connectContext('Video', false);
        await requestPilotHandoff(unsupportedVideo, firstAircraft, randomUUID(),
            { status: 409, code: 'PILOT_HANDOFF_UNAVAILABLE' });
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
