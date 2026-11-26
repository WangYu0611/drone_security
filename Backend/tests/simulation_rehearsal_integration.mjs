// Exercises the real HTTP/WS interfaces on an owned Backend with only a loopback UDP sink.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createSocket} from 'node:dgram';
import {once} from 'node:events';
import fs from 'node:fs';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import {setTimeout as sleep} from 'node:timers/promises';
import {fileURLToPath} from 'node:url';

const root = fileURLToPath(new URL('../..', import.meta.url));
const executable = path.resolve(process.argv[2] ?? path.join(root, 'Backend/build/Release/DroneBackend.exe'));
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'drone-rehearsal-'));
const registry = path.join(directory, 'drones.json');
const configuration = path.join(directory, 'backend.json');
const realConfiguration = path.join(directory, 'real_execution.json');
const clients = [];
let backend;
let sink;

const host = '127.0.0.1';
const missionEndpoint = '/api/security-plans';
const origin = {latitude: 1.283, longitude: 103.86, altitude: 15};

async function waitFor(read, accept, description) {
    const deadline = Date.now() + 10000;
    while (Date.now() < deadline) {
        const result = await read();
        if (accept(result)) return result;
        await sleep(50);
    }
    throw new Error('Timed out waiting for ' + description);
}

async function reserveTcpPort() {
    const server = net.createServer();
    server.listen(0, host);
    await once(server, 'listening');
    return server;
}

async function bindUdp() {
    const socket = createSocket('udp4');
    socket.bind(0, host);
    await once(socket, 'listening');
    return socket;
}

let httpPort;
let wsPort;
async function api(route, body, status = 200, method = body === undefined ? 'GET' : 'POST') {
    const response = await fetch('http://' + host + ':' + httpPort + route, {
        method,
        headers: {'content-type': 'application/json'},
        body: JSON.stringify(body),
        signal: AbortSignal.timeout(3000),
    });
    const result = await response.json();
    assert.equal(response.status, status, route + ': ' + JSON.stringify({
        code: result.code, message: result.message, status: response.status,
    }));
    return result;
}

async function openClient(role) {
    const instance = crypto.randomUUID();
    await api('/api/context/clients', {
        instance_id: instance, client_id: 'Simulation-QA-' + role,
        client_role: role, hostname: 'OWNED-LOOPBACK-QA', app_version: 'simulation-qa',
    });
    const socket = new WebSocket('ws://' + host + ':' + wsPort + '/ws');
    const messages = [];
    const client = {instance, role, socket, messages};
    clients.push(client);
    socket.onmessage = event => messages.push(JSON.parse(event.data));
    await new Promise((resolve, reject) => {socket.onopen = resolve; socket.onerror = reject;});
    socket.send(JSON.stringify({type: 'subscribe_context', instance_id: instance}));
    await waitFor(() => messages, rows => rows.some(row => row.type === 'context_subscribed'), role + ' subscription');
    client.timer = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify({type: 'context_ping', hydrated: true}));
    }, 2000);
    return client;
}

async function startBackend() {
    const log = fs.openSync(path.join(directory, 'backend-output.log'), 'a');
    try {
        backend = spawn(executable, [configuration], {cwd: directory, stdio: ['ignore', log, log], windowsHide: true});
    } finally {
        fs.closeSync(log);
    }
    await once(backend, 'spawn');
    const health = await waitFor(async () => {
        assert.equal(backend.exitCode, null, 'Owned Backend exited before accepting requests');
        try { return await api('/'); } catch { return null; }
    }, Boolean, 'owned Backend startup');
    assert.equal(health.http_port, httpPort);
    assert.equal(health.ws_port, wsPort);
    assert(health.capabilities.includes('simulation_rehearsal'), 'Backend needs simulation rehearsal capability');
}

async function stopBackend() {
    for (const client of clients.splice(0)) {
        clearInterval(client.timer);
        client.socket.close();
    }
    if (backend && backend.pid && backend.exitCode === null && backend.signalCode === null) {
        const exited = once(backend, 'exit');
        backend.kill();
        await exited;
    }
}

const actions = (client, action, fields = {}, status = 200) => api(missionEndpoint, {
    action, instance_id: client.instance, request_id: crypto.randomUUID(), ...fields,
}, status);

async function createMission(command, map, name, members, start, spacing) {
    const created = await actions(command, 'create_plan', {name, default_mission_name: name});
    const {plan_id, mission_id} = created;
    const ids = {plan_id, mission_id};
    const assigned = await actions(command, 'assign', {...ids, assigned_uav_ids: members, spacing_m: spacing});
    const edit = await actions(map, 'begin_route_edit', {
        ...ids, content_revision: assigned.state.plans[plan_id].content_revision,
    });
    const waypoints = [0, .00005].map((step, index) => ({
        ...start, latitude: start.latitude + step, sequence: index + 1,
        altitude_reference: 'Ellipsoid', segmentSpeed: 6, waitTime: 0,
    }));
    await actions(map, 'save_route', {
        ...ids, edit_session_id: edit.edit_session_id, path: {bClosedLoop: false, waypoints},
    });
    return ids;
}

async function finishRehearsal(command, mission, count, beforeReview = () => {}) {
    const started = await actions(command, 'simulation_start', mission);
    const id = started.execution_id;
    const group = started.state.executions[id].group_id;
    const readGroup = async () => Object.values((await api(missionEndpoint)).executions)
        .filter(item => item.group_id === group);
    const completed = await waitFor(readGroup, rows => {
        assert.equal(rows.length, count);
        assert(rows.every(row => row.state !== 'FAILED'), JSON.stringify(rows.map(row => row.failure_reason)));
        return rows.every(row => row.state === 'COMPLETED');
    }, 'simulated route completion');
    for (const row of completed) {
        assert(row.simulation && row.rehearsal);
        assert.equal(row.completion_reason, 'ROUTE_FINISHED');
        assert(row.replay_samples.length >= 2);
        assert.deepEqual(row.replay_samples.at(-1).position, row.position);
        assert.equal(row.replay_samples.at(-1).completed_waypoints, row.completed_waypoints);
    }
    await beforeReview(id);
    const reviewed = await actions(command, 'simulation_review', {execution_id: id});
    assert(reviewed.state.executions[id].rehearsal_review);
    return id;
}

try {
    const httpReservation = await reserveTcpPort();
    const wsReservation = await reserveTcpPort();
    httpPort = httpReservation.address().port;
    wsPort = wsReservation.address().port;
    sink = await bindUdp();
    const incoming = [];
    sink.on('message', bytes => incoming.push(bytes));
    const receiveReservation = await bindUdp();
    const receivePort = receiveReservation.address().port;
    await Promise.all([
        new Promise(resolve => httpReservation.close(resolve)),
        new Promise(resolve => wsReservation.close(resolve)),
        new Promise(resolve => receiveReservation.close(resolve)),
    ]);

    fs.writeFileSync(registry, '[]');
    fs.writeFileSync(configuration, JSON.stringify({
        server: {http_port: httpPort, ws_port: wsPort, debug: false},
        jetson: {host},
        port_map: {1: {host, send_port: sink.address().port, recv_port: receivePort, ros_topic_prefix: ''}},
        storage: {path: registry, video_metadata_path: path.join(directory, 'video')},
        log: {level: 'info', file: path.join(directory, 'backend.log')},
    }));

    await startBackend();
    const command = await openClient('Command');
    const map = await openClient('Map');
    assert.deepEqual(await api('/api/drones'), []);
    await api('/api/drones', {slot: 1, name: 'Invalid simulator transport', simulation: true}, 400);
    assert.equal((await actions(map, 'simulation_create', {...origin, count: 3}, 403)).code, 'ROLE_FORBIDDEN');
    const create = {...origin, count: 3, request_id: crypto.randomUUID()};
    await actions(command, 'simulation_create', create);
    await actions(command, 'simulation_create', create);
    await waitFor(() => map.messages, rows => rows.some(row => row.type === 'DroneRegistryChanged'), 'roster event');
    const simulated = await api('/api/drones');
    assert.equal(simulated.length, 3, 'retry must not duplicate simulated aircraft');
    assert.deepEqual(JSON.parse(fs.readFileSync(registry, 'utf8')), []);
    for (const row of simulated) {
        assert(row.simulation && row.telemetry_source === 'mock_rehearsal');
        assert(!Object.hasOwn(row, 'telemetry_received_at'));
        assert.equal(row.bit_index, -1);
        const projected = await waitFor(() => map.messages.find(message =>
            message.type === 'telemetry' && message.drone_id === row.id), Boolean, 'simulated WS telemetry');
        assert.deepEqual(projected.mock_position, row.mock_position);
        assert.equal(projected.telemetry_source, row.telemetry_source);
        assert.equal(projected.simulation, true);
    }
    console.log('PASS empty fleet creates exactly three simulated aircraft with matching REST and WS telemetry');

    const ordered = simulated.sort((a, b) => a.slot - b.slot);
    assert(ordered[0].gps_lon < ordered[1].gps_lon && ordered[1].gps_lon < ordered[2].gps_lon,
        'Numeric assignment order must follow the simulated starting line');
    const formation = await createMission(command, map, 'Three-aircraft rehearsal',
        ordered.map(row => 'UAV-' + row.slot), ordered[1].mock_position, 3);
    const group = await finishRehearsal(command, formation, 3);
    await waitFor(() => map.messages, rows => rows.some(row =>
        row.type === 'SecurityPlansChanged' && row.payload.executions[group]?.state === 'COMPLETED'),
    'MAP websocket completion snapshot');
    assert.equal(incoming.length, 0, 'empty hardware registry must emit no control datagrams');
    console.log('PASS all three aircraft complete a saved MAP route and produce versioned replay samples');

    await api('/api/drones', {slot: 1, name: 'Loopback-only physical fixture'}, 201);
    const mixed = await api('/api/drones');
    assert.equal(mixed.filter(row => row.simulation).length, 3);
    assert.equal(mixed.filter(row => !row.simulation).length, 1);
    assert.equal(JSON.parse(fs.readFileSync(registry, 'utf8')).length, 1);
    const hardware = mixed.find(row => row.slot === 1);
    assert.equal(hardware.telemetry_source, 'unavailable');
    assert.equal((await actions(command, 'simulation_assign_real',
        {execution_id: group, assigned_uav_id: 'UAV-01'}, 409)).code, 'REHEARSAL_MULTI_AIRCRAFT');
    const association = {uav_id: 'UAV-01', drone_id: hardware.id};
    assert.equal((await actions(command, 'simulation_create',
        {...origin, count: 1, association: {...association, drone_id: hardware.id + 100}}, 409)).code,
        'REAL_UAV_REGISTRATION_CHANGED');
    assert.equal((await actions(command, 'simulation_create',
        {...origin, count: 2, association}, 409)).code, 'SIMULATION_BINDING_SINGLE_AIRCRAFT');
    const bound = {...origin, count: 1, association, request_id: crypto.randomUUID()};
    const boundAck = await actions(command, 'simulation_create', bound);
    assert.deepEqual(await actions(command, 'simulation_create', bound).then(result => result.state.simulation_uavs),
        boundAck.state.simulation_uavs);
    const [simulatedId, simulatedRecord] = Object.entries(boundAck.state.simulation_uavs)
        .find(([, row]) => row.real_binding?.drone_id === hardware.id);
    assert.deepEqual(simulatedRecord.real_binding, association);
    assert.equal(simulatedRecord.binding_version, 1);
    const solo = await createMission(command, map, 'Single-aircraft rehearsal',
        [simulatedId], simulatedRecord.home_position, 20);
    const savedCandidate = {...association, simulated_uav_id: simulatedId, binding_version: 1};
    assert.deepEqual((await api(missionEndpoint)).missions[solo.mission_id].planned_real_aircraft, savedCandidate);
    const editAssociation = {simulated_uav_id: simulatedId, expected_binding_version: 1, association: null};
    assert.equal((await actions(map, 'simulation_set_real_binding', editAssociation, 403)).code, 'ROLE_FORBIDDEN');
    const cleared = await actions(command, 'simulation_set_real_binding', editAssociation);
    assert.equal(cleared.state.simulation_uavs[simulatedId].real_binding, null);
    assert.deepEqual(cleared.state.missions[solo.mission_id].planned_real_aircraft, savedCandidate);
    assert.equal((await actions(command, 'simulation_set_real_binding', editAssociation, 409)).code, 'VERSION_CONFLICT');
    const reselected = await actions(command, 'simulation_set_real_binding',
        {...editAssociation, association, expected_binding_version: 2});
    assert.deepEqual(reselected.state.simulation_uavs[simulatedId].real_binding, association);
    assert.deepEqual(reselected.state.missions[solo.mission_id].planned_real_aircraft, savedCandidate);
    const soloRun = await finishRehearsal(command, solo, 1, async execution_id => {
        const rejected = await actions(command, 'simulation_assign_real', {
            execution_id, assigned_uav_id: association.uav_id, expected_assigned_drone_id: association.drone_id,
        }, 409);
        assert.equal(rejected.code, 'REHEARSAL_REVIEW_REQUIRED');
    });
    const updatedCandidate = {...savedCandidate, binding_version: 3};
    const refreshed = await actions(command, 'assign', {
        ...solo, assigned_uav_ids: [simulatedId], formation: 'Line', spacing_m: 20, refresh_real_candidate: true,
    });
    assert.deepEqual(refreshed.state.missions[solo.mission_id].planned_real_aircraft, updatedCandidate);
    assert.equal((await actions(command, 'simulation_assign_real', {execution_id: soloRun,
        assigned_uav_id: 'UAV-01', expected_assigned_drone_id: hardware.id + 1}, 409)).code,
        'REAL_UAV_REGISTRATION_CHANGED');
    const handoff = {execution_id: soloRun, assigned_uav_id: 'UAV-01',
        assigned_drone_id: hardware.id + 99, expected_assigned_drone_id: hardware.id, request_id: crypto.randomUUID()};
    const pending = await actions(command, 'simulation_assign_real', handoff);
    await actions(command, 'select', solo);
    assert.equal((await actions(command, 'simulation_assign_real', handoff)).plan_id, pending.plan_id);
    assert.equal((await api('/api/context')).active_security_plan_id, solo.plan_id);
    assert.equal(pending.state.plans[pending.plan_id].status, 'DRAFT');
    assert.equal(pending.state.plans[pending.plan_id].real_assignment.source_execution_id, soloRun);
    assert.equal(pending.state.plans[pending.plan_id].real_assignment.assigned_drone_id, hardware.id,
        'The Backend must assert the registration ID from its registry');
    assert.equal(pending.state.missions[pending.mission_id].assigned_uav_id, 'UAV-01');
    assert(!Object.hasOwn(pending.state.missions[pending.mission_id], 'planned_real_aircraft'));
    const repeated = await actions(command, 'simulation_assign_real', {...handoff, request_id: crypto.randomUUID()});
    assert.equal(repeated.plan_id, pending.plan_id);
    assert.equal(repeated.existing_assignment, true);
    assert.equal(repeated.state.missions[solo.mission_id].preferred_real_plan_id, pending.plan_id);
    assert(pending.state.plans[pending.plan_id].validation.issues.some(issue => issue.code === 'REAL_EXECUTION_DISABLED'));
    assert(Object.values(pending.state.executions).every(execution => execution.simulation));
    await sleep(250);
    assert.equal(incoming.length, 0, 'mixed-fleet rehearsal and pending assignment must emit no physical datagrams');
    console.log('PASS persistent association, independent task candidate, guarded handoff and deduplication without UDP traffic');

    await stopBackend();
    await startBackend();
    const restored = await api(missionEndpoint);
    assert.equal(restored.plans[pending.plan_id].real_assignment.source_execution_id, soloRun);
    assert.deepEqual(restored.executions[soloRun].replay_samples, pending.state.executions[soloRun].replay_samples);
    assert.deepEqual(restored.simulation_uavs[simulatedId].real_binding, association);
    assert.deepEqual(restored.missions[solo.mission_id].planned_real_aircraft, updatedCandidate);
    assert.equal(restored.missions[solo.mission_id].preferred_real_plan_id, pending.plan_id);
    assert.equal((await api('/api/drones')).filter(row => row.simulation).length, 4);
    console.log('PASS restart retains simulated aircraft, rehearsal replay and pending real assignment');

    await stopBackend();
    const realSettings = {
        enabled: true, default_speed_mps: 6, safety_separation_m: 1.5,
        uavs: {'UAV-01': {drone_id: hardware.id, frame_anchor: {
            ...origin, altitude_reference: 'Ellipsoid', ned_n: 0, ned_e: 0, ned_d: 0,
        }}},
    };
    fs.writeFileSync(realConfiguration, JSON.stringify(realSettings));
    await startBackend();
    const realCommand = await openClient('Command');
    await openClient('Map');
    const checks = await actions(realCommand, 'validate', {plan_id: pending.plan_id});
    const verification = checks.state.plans[pending.plan_id].validation;
    assert.equal(verification.ready, false);
    assert(verification.issues.some(issue => issue.code === 'REAL_ANCHOR_CHANGED_RECALIBRATE'));
    await finishRehearsal(realCommand, solo, 1);
    assert.equal(incoming.length, 0, 'Real adapter must not dispatch pending or simulated tasks');
    console.log('PASS configured Real adapter blocks missing aircraft location and still permits software rehearsal');

    await stopBackend();
    fs.unlinkSync(realConfiguration);
    await startBackend();
    await api('/api/drones/' + hardware.id_str, undefined, 200, 'DELETE');
    await stopBackend();
    await startBackend();
    const replacement = await api('/api/drones', {slot: 1, name: 'Loopback replacement'}, 201);
    assert.notEqual(replacement.id, hardware.id,
        'A pending assignment preserves its registration ID even if the empty registry was reloaded');

    await stopBackend();
    realSettings.uavs['UAV-01'].drone_id = replacement.id;
    fs.writeFileSync(realConfiguration, JSON.stringify(realSettings));
    await startBackend();
    const replacementCommand = await openClient('Command');
    const staleCandidate = await actions(replacementCommand, 'simulation_set_real_binding',
        {...editAssociation, association, expected_binding_version: 3}, 409);
    assert.equal(staleCandidate.code, 'REAL_UAV_REGISTRATION_CHANGED');
    const changed = await actions(replacementCommand, 'validate', {plan_id: pending.plan_id});
    const changedValidation = changed.state.plans[pending.plan_id].validation;
    assert.equal(changedValidation.ready, false);
    assert(changedValidation.issues.some(issue => issue.code === 'REAL_UAV_REGISTRATION_CHANGED'));
    assert.equal(incoming.length, 0, 'A replacement aircraft must not receive commands from a pending assignment');
    console.log('PASS a replacement registered in the same slot after restart does not inherit the real assignment');
} catch (error) {
    console.error('Backend diagnostics retained in ' + directory);
    throw error;
} finally {
    await stopBackend();
    if (sink) sink.close();
}
