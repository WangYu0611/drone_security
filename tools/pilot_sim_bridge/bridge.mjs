import { randomUUID } from 'node:crypto';
import { createSocket } from 'node:dgram';
import { readFile } from 'node:fs/promises';
import { parseArgs } from 'node:util';

const loopback = '127.0.0.1';
const controlPath = '/api/pilot-control';
const intervalMs = 50;
const sampleAgeMs = 150;
const windowAgeMs = 350;
const actions = new Set(['none', 'takeoff', 'land', 'hold', 'gohome', 'estop']);
const decoder = new TextDecoder('utf-8', { fatal: true });

function report(event, fields = {}) {
    console.log(JSON.stringify({ event, ...fields }));
}

function argumentsForBridge() {
    const { values, tokens } = parseArgs({
        options: {
            'http-port': { type: 'string' },
            'ws-port': { type: 'string' },
            'listen-port': { type: 'string', default: '8899' },
            'pilot-id': { type: 'string' },
            'uav-id': { type: 'string' },
            activate: { type: 'boolean' },
        },
        strict: true,
        tokens: true,
    });
    const optionNames = tokens.map(token => token.name);
    if (new Set(optionNames).size !== optionNames.length) throw new Error('Duplicate option');
    const port = (name, minimum = 1) => {
        const value = values[name];
        if (!/^\d{1,5}$/.test(value) || Number(value) < minimum || Number(value) > 65535) {
            throw new Error('Invalid port');
        }
        return Number(value);
    };
    const pilotId = values['pilot-id'];
    const uavId = values['uav-id'];
    if (values.activate !== true || !pilotId?.trim() || Buffer.byteLength(pilotId) > 128 ||
        !/^UAV-\d+$/.test(uavId) || uavId.length > 128) {
        throw new Error('Missing opt-in or invalid identity');
    }
    return { pilotId, uavId, httpPort: port('http-port'), wsPort: port('ws-port'), listenPort: port('listen-port', 0) };
}

function decodeStick(packet) {
    if (packet.length > 512) return;
    let message;
    try { message = JSON.parse(decoder.decode(packet)); } catch { return; }
    if (message?.type !== 'stick' || typeof message.session !== 'string' || !message.session.trim() ||
        Buffer.byteLength(message.session) > 32 || !Number.isSafeInteger(message.seq) || message.seq < 0 ||
        !Number.isFinite(message.t) || !actions.has(message.action)) return;
    if (Object.hasOwn(message, 'heading_deg') && (!Number.isFinite(message.heading_deg) ||
        message.heading_deg < 0 || message.heading_deg >= 360)) return;
    const axes = { forward: message.forward, right: message.right, vertical: message.up, yaw: message.yaw };
    if (!Object.values(axes).every(value => Number.isFinite(value) && value >= -1 && value <= 1)) return;
    // Heading remains informational. The Mock Backend rotates body axes using its simulated heading.
    return { session: message.session, sequence: message.seq, axes, action: message.action, receivedAt: performance.now() };
}

function isNeutral(frame) {
    return frame?.action === 'none' && Object.values(frame.axes).every(value => value === 0);
}

async function pilotAuthorization(pilotId) {
    const document = JSON.parse(await readFile(process.env.DRONE_PILOT_CONTROL_CREDENTIALS, 'utf8'));
    const matches = document.credentials.filter(entry => entry?.role === 'Pilot' && entry.id === pilotId);
    const token = matches[0]?.token;
    if (matches.length !== 1 || typeof token !== 'string' || !/^[!-~]{1,512}$/.test(token)) {
        throw new Error('Invalid Pilot credential');
    }
    return `Bearer ${token}`;
}

async function run({ pilotId, uavId, httpPort, wsPort, listenPort }, authorization) {
    const httpUrl = `http://${loopback}:${httpPort}`;
    const headers = { Authorization: authorization, 'X-Drone-Pilot-Sim-Bridge': '1' };
    const capabilitiesResponse = await fetch(httpUrl, { redirect: 'error', signal: AbortSignal.timeout(4000) });
    const capabilities = capabilitiesResponse.ok && (await capabilitiesResponse.json()).capabilities;
    if (!Array.isArray(capabilities) || !capabilities.includes('pilot_sim_bridge') ||
        !capabilities.includes('pilot_control_actions')) throw new Error('Mock bridge capability unavailable');

    const socket = new WebSocket(`ws://${loopback}:${wsPort}/ws/pilot-control`, { headers });
    const udp = createSocket('udp4');
    const abort = new AbortController();
    let closed = false;
    let connectionId, serverEpoch, aircraft, activation, sessionId, window;
    let peer, latest, level;
    const pendingActions = [];
    let active = false, lastSequence = -1, sequence = 0, nextSendAt = 0, sendTimer, controlTimer;

    const subscribeTimer = setTimeout(() => stop('SUBSCRIPTION_TIMEOUT', 1), 4000);
    function stop(reason, exitCode = 0) {
        if (closed) return;
        closed = true;
        clearTimeout(subscribeTimer);
        clearTimeout(sendTimer);
        clearTimeout(controlTimer);
        abort.abort();
        udp.close();
        socket.close();
        process.exitCode = exitCode;
        report('stopped', { reason });
    }

    function expectControlWindow() {
        clearTimeout(controlTimer);
        controlTimer = setTimeout(() => stop('CONTROL_WINDOW_LOST', 1), 1250);
    }

    function flush() {
        clearTimeout(sendTimer);
        sendTimer = undefined;
        if (closed || !active || socket.readyState !== WebSocket.OPEN) return;
        const now = performance.now();
        if (now - window.receivedAt > windowAgeMs) return;
        while (pendingActions.length && now - pendingActions[0].receivedAt > sampleAgeMs) pendingActions.shift();
        if (level && now - level.receivedAt > sampleAgeMs) level = undefined;
        if (!pendingActions.length && !level) return;
        if (now < nextSendAt) {
            sendTimer = setTimeout(flush, Math.ceil(nextSendAt - now));
            return;
        }
        if (socket.bufferedAmount > 0) return;
        const frame = pendingActions.shift() ?? level;
        if (level?.sequence <= frame.sequence) level = undefined;
        nextSendAt = now + intervalMs;
        socket.send(JSON.stringify({
            type: 'pilot_input', uav_id: uavId, control_session_id: sessionId,
            window_id: window.id, seq: ++sequence, axes: frame.axes, action: frame.action,
        }));
    }

    function ready() {
        if (!activation?.confirmed || !sessionId || window?.sessionId !== sessionId ||
            performance.now() - window.receivedAt > windowAgeMs) return;
        if (!active) {
            active = true;
            report('active', { uav_id: uavId });
            if (isNeutral(latest)) level = latest;
        }
        flush();
    }

    async function activate() {
        if (closed || activation || !connectionId || aircraft?.control_state !== 'IDLE' ||
            aircraft.activation_reason !== '' || !isNeutral(latest) ||
            performance.now() - latest.receivedAt > sampleAgeMs) return;
        const requestId = randomUUID();
        activation = { grantId: aircraft.grant_id, confirmed: false };
        expectControlWindow();
        try {
            const response = await fetch(httpUrl + controlPath, {
                method: 'POST', headers: { ...headers, 'Content-Type': 'application/json' },
                redirect: 'error', signal: AbortSignal.any([abort.signal, AbortSignal.timeout(4000)]),
                body: JSON.stringify({
                    action: 'activate', request_id: requestId, uav_id: uavId, grant_id: activation.grantId,
                    expected_revision: aircraft.revision, connection_id: connectionId,
                }),
            });
            const reply = await response.json();
            const result = reply.results?.[0];
            if (!response.ok || reply.request_id !== requestId || reply.server_epoch !== serverEpoch ||
                !result?.ok || result.uav_id !== uavId || result.code !== 'ACTIVE') {
                stop('ACTIVATION_REJECTED', 1);
                return;
            }
            activation.confirmed = true;
            if (!closed) ready();
        } catch {
            stop('ACTIVATION_FAILED', 1);
        }
    }

    function snapshot(payload) {
        if (typeof payload?.server_epoch !== 'string' || !payload.server_epoch ||
            (serverEpoch && payload.server_epoch !== serverEpoch) || !Array.isArray(payload.aircraft)) {
            stop('INVALID_SNAPSHOT', 1);
            return;
        }
        serverEpoch = payload.server_epoch;
        aircraft = payload.aircraft.find(row => row?.uav_id === uavId && row.pilot_id === pilotId &&
            row.assignment_state === 'GRANTED' && typeof row.grant_id === 'string' && row.grant_id &&
            Number.isSafeInteger(row.revision));

        if (!activation) {
            void activate();
            return;
        }
        if (aircraft?.grant_id !== activation.grantId) {
            stop('GRANT_CHANGED');
            return;
        }
        if (sessionId && (aircraft.control_state !== 'ACTIVE' || aircraft.control_session_id !== sessionId)) {
            const timedOut = payload.last_exit?.uav_id === uavId && payload.last_exit.reason === 'INPUT_TIMEOUT';
            stop(timedOut ? 'INPUT_TIMEOUT' : 'SESSION_ENDED');
            return;
        }
        if (aircraft.control_state === 'ACTIVE' && typeof aircraft.control_session_id === 'string') {
            sessionId = aircraft.control_session_id;
            ready();
        }
    }

    socket.addEventListener('open', () => {
        if (!closed) socket.send(JSON.stringify({ type: 'subscribe_pilot_control' }));
    }, { once: true });
    socket.addEventListener('message', event => {
        if (closed) return;
        let message;
        try { message = JSON.parse(event.data); } catch { stop('INVALID_BACKEND_MESSAGE', 1); return; }
        switch (message?.type) {
            case 'pilot_control_subscribed':
                if (connectionId || typeof message.connection_id !== 'string' || !message.connection_id) {
                    stop('INVALID_SUBSCRIPTION', 1);
                    return;
                }
                connectionId = message.connection_id;
                clearTimeout(subscribeTimer);
                snapshot(message.payload);
                break;
            case 'pilot_control_changed':
                if (connectionId) snapshot(message.payload);
                break;
            case 'pilot_input_window':
                if (!activation || (sessionId && message.control_session_id !== sessionId) ||
                    typeof message.control_session_id !== 'string' || !Number.isSafeInteger(message.window_id) ||
                    message.window_id <= 0 || message.window_id <= (window?.id ?? 0)) return;
                window = { sessionId: message.control_session_id, id: message.window_id, receivedAt: performance.now() };
                expectControlWindow();
                ready();
                break;
            case 'error':
            case 'pilot_input_rejected':
                stop('BACKEND_REJECTED', 1);
                break;
        }
    });
    socket.addEventListener('error', () => stop('WEBSOCKET_ERROR', 1), { once: true });
    socket.addEventListener('close', () => stop('WEBSOCKET_CLOSED', 1), { once: true });
    udp.on('error', () => stop('UDP_ERROR', 1));
    udp.on('listening', () => report('listening', { port: udp.address().port }));
    udp.on('message', (packet, sender) => {
        if (closed || sender.address !== loopback || (peer && sender.port !== peer.port)) return;
        const frame = decodeStick(packet);
        if (!frame || (peer && frame.session !== peer.session) || frame.sequence <= lastSequence) return;
        peer ??= { port: sender.port, session: frame.session };
        lastSequence = frame.sequence;
        latest = frame;
        if (!active) {
            void activate();
            return;
        }
        if (frame.action === 'none') level = frame;
        else if (frame.action === 'estop') pendingActions.splice(0, pendingActions.length, frame);
        else if (pendingActions.length < 8) pendingActions.push(frame);
        else { stop('INPUT_OVERLOAD', 1); return; }
        flush();
    });
    process.once('SIGINT', () => stop('OPERATOR_STOP'));
    process.once('SIGTERM', () => stop('OPERATOR_STOP'));
    udp.bind({ port: listenPort, address: loopback, exclusive: true });
}

let options;
try { options = argumentsForBridge(); } catch {
    report('error', { reason: 'INVALID_ARGUMENTS', usage: 'node tools/pilot_sim_bridge/bridge.mjs --http-port N --ws-port N --pilot-id ID --uav-id UAV-NN --activate [--listen-port N]' });
    process.exitCode = 2;
}
if (options) {
    let authorization;
    try { authorization = await pilotAuthorization(options.pilotId); } catch {
        report('error', { reason: 'INVALID_PILOT_CREDENTIALS' });
        process.exitCode = 1;
    }
    if (authorization) {
        try { await run(options, authorization); } catch {
            report('error', { reason: 'MOCK_BRIDGE_START_FAILED' });
            process.exitCode = 1;
        }
    }
}
