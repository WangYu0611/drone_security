// Owned loopback-only Backend process. No existing server or hardware is used.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import net from 'node:net';
import dgram from 'node:dgram';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const out=path.join(root,'Saved','MockPositionQA','transport-'+Date.now());fs.mkdirSync(out,{recursive:true});
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function tcpPort(){const socket=net.createServer();await new Promise(r=>socket.listen(0,'127.0.0.1',r));const port=socket.address().port;await new Promise(r=>socket.close(r));return port;}
async function udpSocket(){const socket=dgram.createSocket('udp4');await new Promise(r=>socket.bind(0,'127.0.0.1',r));return socket;}
const httpPort=await tcpPort(),wsPort=await tcpPort(),probeSink=await udpSocket(),reservation=await udpSocket();
const udpPort=reservation.address().port;reservation.close();
const config={server:{http_port:httpPort,ws_port:wsPort,debug:false},drone:{lost_timeout_sec:1},jetson:{host:'127.0.0.1'},
    port_map:{1:{host:'127.0.0.1',send_port:probeSink.address().port,recv_port:udpPort,ros_topic_prefix:''}},
    storage:{path:path.join(out,'drones.json'),video_metadata_path:path.join(out,'metadata')},log:{level:'info',file:path.join(out,'backend.log')}};
fs.writeFileSync(path.join(out,'config.json'),JSON.stringify(config));
fs.writeFileSync(path.join(out,'drones.json'),JSON.stringify([{id:'d1',slot:1,name:'Owned Mock GPS test',model:'PX4',ip:'127.0.0.1',port:probeSink.address().port,video_url:''}]));
fs.copyFileSync(path.join(root,'Backend','mock_execution.json'),path.join(out,'mock_execution.json'));
const home=JSON.parse(fs.readFileSync(path.join(out,'mock_execution.json'))).uavs['UAV-01'].home_position;
const base=`http://127.0.0.1:${httpPort}`;let child,socket,udp,events=[],session;
const passed=[],evidence={synthetic:true,scope:'owned loopback transport; not real hardware',httpPort,wsPort,udpPort,passed};
async function api(url,body,expected=200){const response=await fetch(base+url,{method:body?'POST':'GET',headers:{'content-type':'application/json'},body:body?JSON.stringify(body):undefined});const value=await response.json();assert.equal(response.status,expected,JSON.stringify(value));return value;}
async function until(read,predicate,timeout=10000){const start=Date.now();for(;;){const value=await read();if(predicate(value))return value;assert(Date.now()-start<timeout,'Timed out waiting for expected state');await sleep(60);}}
const pass=name=>{passed.push(name);console.log('PASS '+name);};
async function start(){
    const log=fs.openSync(path.join(out,'process.log'),'a');child=spawn(path.join(root,'Backend','build','Release','DroneBackend.exe'),[path.join(out,'config.json')],{cwd:root,windowsHide:true,stdio:['ignore',log,log]});fs.closeSync(log);
    evidence.process_ids??=[];evidence.process_ids.push(child.pid);
    await until(async()=>{assert(child.exitCode===null,'Owned Backend exited');try{return await api('/api/drones');}catch{return null;}},Boolean);
    events=[];socket=new WebSocket(`ws://127.0.0.1:${wsPort}/ws`);socket.onmessage=e=>events.push(JSON.parse(e.data));
    await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
}
async function stop(){if(socket){socket.close();socket=null;}if(child && child.exitCode===null){const ended=new Promise(r=>child.once('exit',r));child.kill();await ended;}child=null;}
async function connect(role='Command'){
    session=crypto.randomUUID();await api('/api/context/clients',{instance_id:session,client_id:'MockPosition-'+role,client_role:role,hostname:'OWNED-LOOPBACK-QA',app_version:'test'});
    socket.send(JSON.stringify({type:'subscribe_context',instance_id:session}));await until(async()=>events,e=>e.some(x=>x.type==='context_subscribed'));
}
async function send(action,extra={},expected=200){const state=await api('/api/security-plans');return api('/api/security-plans',{action,instance_id:session,expected_version:state.version,request_id:crypto.randomUUID(),...extra},expected);}
async function drone(){return (await api('/api/drones'))[0];}
const gps=p=>({latitude:p.gps_lat,longitude:p.gps_lon,altitude:p.gps_alt});
const sameConfiguredPosition=(actual,expected)=>{for(const key of ['latitude','longitude','altitude'])assert(Math.abs(actual[key]-expected[key])<1e-9,`Configured ${key} differs: ${actual[key]} vs ${expected[key]}`);};
try {
    await start();assert((await api('/')).capabilities.includes('telemetry_receive_time'));pass('Backend advertises server-side telemetry receive time for real-time startup compatibility');
    const initial=await drone();sameConfiguredPosition(gps(initial),home);assert.equal(initial.telemetry_source,'mock_home');assert.equal(initial.simulation,true);assert.equal(initial.status,'offline');assert.equal(initial.local_position_valid,false);assert.equal(initial.battery,-1);assert(!Object.hasOwn(initial,'telemetry_received_at'));pass('Natural REST startup has fixed Mock home and no hardware liveness or received timestamp');
    const wsHome=await until(async()=>events.find(e=>e.type==='telemetry'&&e.telemetry_source==='mock_home'),Boolean);assert.deepEqual(gps(wsHome),gps(initial));assert.equal(wsHome.status,'offline');sameConfiguredPosition(wsHome.mock_position,home);assert(!Object.hasOwn(wsHome,'telemetry_received_at'));pass('WebSocket and REST expose the same fixed location and provenance without invented received time');
    await api('/api/drones/refresh',{});await sleep(1300);assert.deepEqual(gps(await drone()),gps(initial));assert.equal((await drone()).status,'offline');pass('Refresh and idle telemetry do not move the Mock home or invent Online');
    await connect();const target={...home,latitude:home.latitude+0.0001};
    const moved=await send('execution_group_move',{assigned_uav_ids:['UAV-01'],target});const id=moved.execution_id;
    const blocked=await send('execution_group_move',{assigned_uav_ids:['UAV-01'],target},409);assert.equal(blocked.code,'DRONE_ACTIVE_PLAN_CONFLICT');pass('Singleton starts through normal controller and repeated occupancy rejects');
    const completed=await until(async()=>(await api('/api/security-plans')).executions[id],e=>e.state==='COMPLETED');
    assert(Math.abs(completed.position.latitude-target.latitude)<1e-8);assert.equal((await drone()).telemetry_source,'mock_execution');evidence.completed=completed.position;
    await stop();await start();const restored=await drone();assert.deepEqual(gps(restored),completed.position);assert.equal(restored.telemetry_source,'mock_execution');await sleep(1200);assert.deepEqual(gps(await drone()),completed.position);pass('Owned restart restores completed execution position and stays there');
    const resetRequest={request_id:crypto.randomUUID(),simulation:true,startup_formation:JSON.parse(fs.readFileSync(path.join(root,'Backend','mock_execution.json'))).startup_formation};
    const reset=await api('/api/mock-executions/reset',resetRequest);
    assert.equal(reset.reset_epoch,1);assert.deepEqual(reset.state.executions[id],completed);
    const resetDrone=await drone();sameConfiguredPosition(gps(resetDrone),resetRequest.startup_formation.origin);
    assert.equal(resetDrone.telemetry_source,'mock_reset');assert.equal(resetDrone.mock_epoch,1);assert.equal(resetDrone.status,'offline');
    const wsReset=await until(async()=>events.find(e=>e.type==='telemetry'&&e.telemetry_source==='mock_reset'&&e.mock_epoch===1),Boolean);
    assert(!Object.hasOwn(resetDrone,'telemetry_received_at'));assert(!Object.hasOwn(wsReset,'telemetry_received_at'));
    assert.deepEqual(gps(wsReset),gps(resetDrone));pass('Explicit reset preserves history and shares the acknowledged epoch across REST and WS');
    const backups=fs.readdirSync(path.join(out,'mock-reset-history'));assert.equal(backups.length,1);
    const archived=JSON.parse(fs.readFileSync(path.join(out,'mock-reset-history',backups[0])));
    assert.deepEqual(archived.executions[id],completed);assert(!archived.mock_position_baseline);
    pass('Reset writes a complete pre-reset history snapshot before acknowledging');
    await connect();const afterResetMove=await send('execution_group_move',{assigned_uav_ids:['UAV-01'],target});
    const afterResetCompleted=await until(async()=>(await api('/api/security-plans')).executions[afterResetMove.execution_id],e=>e.state==='COMPLETED');
    assert.equal(afterResetCompleted.mock_epoch,1);assert.deepEqual(afterResetMove.state.executions[afterResetMove.execution_id].home_position,reset.state.mock_position_baseline.positions['UAV-01']);
    const beforeReplay=await api('/api/security-plans');const replay=await api('/api/mock-executions/reset',resetRequest);
    assert.equal(replay.replayed,true);assert.deepEqual(replay.state,beforeReplay);assert.deepEqual(gps(await drone()),afterResetCompleted.position);
    assert.equal(fs.readdirSync(path.join(out,'mock-reset-history')).length,1);
    pass('A later same-epoch move starts from formation and reset retry never rewinds it');
    await stop();await start();assert.deepEqual(gps(await drone()),afterResetCompleted.position);assert.equal((await drone()).mock_epoch,1);
    pass('Backend-only restart preserves the current reset epoch and subsequent movement');
    await connect('Video');const forbidden=await send('execution_group_move',{assigned_uav_ids:['UAV-01'],target},403);assert.equal(forbidden.code,'ROLE_FORBIDDEN');pass('Singleton does not bypass client role authorization');
    udp=await udpSocket();
    const received={gps_lat:40.1,gps_lon:116.4,gps_alt:75,gps_fix:true,local_position:[0,0,-30],local_position_valid:true,battery:81,telemetry_received_at:1};
    const duringFirstPacket=Promise.all(Array.from({length:30},()=>drone()));
    const sentAt=Date.now()/1000;
    udp.send(Buffer.from(JSON.stringify(received)),udpPort,'127.0.0.1');
    for(const row of await duringFirstPacket)if(row.telemetry_source==='received'){
        assert.deepEqual(gps(row),gps(received),'A received source must be paired with its actual callback payload');
        assert(Number.isFinite(row.telemetry_received_at),'Received provenance must include the same callback receipt time');
        assert(row.telemetry_received_at>=sentAt&&row.telemetry_received_at<=Date.now()/1000,'Receipt time must be assigned when the packet arrives');
    }
    pass('Concurrent first-packet REST hydration never labels old or empty GPS received');
    const real=await until(drone,d=>d.telemetry_source==='received');assert.equal(real.simulation,false);assert.deepEqual(gps(real),gps(received));assert.equal(real.local_position_valid,true);
    const wsReal=await until(async()=>events.find(e=>e.type==='telemetry'&&e.telemetry_source==='received'),Boolean);assert.deepEqual(gps(wsReal),gps(received));assert.equal(wsReal.status,'online');
    assert(Number.isFinite(real.telemetry_received_at));assert(real.telemetry_received_at>=sentAt&&real.telemetry_received_at<=Date.now()/1000);
    assert.equal(wsReal.telemetry_received_at,real.telemetry_received_at);pass('Received UDP has one server receipt time shared atomically by REST and WebSocket');
    const lost=await until(drone,d=>d.status==='lost');assert.equal(lost.telemetry_source,'received');assert.deepEqual(gps(lost),gps(received));
    assert.equal(lost.telemetry_received_at,real.telemetry_received_at);
    await sleep(150);const readAgain=await drone();assert.equal(readAgain.status,'lost');assert.equal(readAgain.telemetry_received_at,real.telemetry_received_at);
    pass('Connection loss replaces cached Online while GET and loss detection preserve the last packet receipt time');
    const afterLost=events.length;udp.send(Buffer.from(JSON.stringify({...received,gps_fix:false,local_position_valid:false,gps_lat:0,gps_lon:0})),udpPort,'127.0.0.1');
    const invalid=await until(drone,d=>d.gps_fix===false);assert.equal(invalid.telemetry_source,'received');assert.equal(invalid.status,'lost');
    assert(invalid.telemetry_received_at>real.telemetry_received_at);
    await sleep(1300);assert.equal((await drone()).telemetry_source,'received');assert(!events.slice(afterLost).some(e=>e.type==='telemetry'&&e.simulation));
    const wsInvalid=events.slice(afterLost).find(e=>e.type==='telemetry'&&!e.gps_fix);assert(wsInvalid);assert.equal(wsInvalid.status,'lost');assert.equal(wsInvalid.telemetry_received_at,invalid.telemetry_received_at);assert.equal((await drone()).telemetry_received_at,invalid.telemetry_received_at);pass('Only a new UDP packet advances receipt time; lost hardware and invalid GPS never fall back to Mock');
    const protectedReset=await api('/api/mock-executions/reset',{...resetRequest,request_id:crypto.randomUUID()});
    assert.equal(Object.keys(protectedReset.positions).length,0);assert.deepEqual(protectedReset.skipped_uav_ids,['UAV-01']);
    assert.equal((await drone()).telemetry_source,'received');assert.equal((await drone()).gps_fix,false);
    pass('Reset excludes received hardware even after connection loss and invalid GPS');
    evidence.final=await drone();evidence.result='PASS';
} catch(error){evidence.result='FAIL';evidence.error=String(error.stack??error);process.exitCode=1;console.error(error);}
finally {await stop();if(udp)udp.close();probeSink.close();fs.writeFileSync(path.join(out,'result.json'),JSON.stringify(evidence,null,2));console.log('Evidence: '+out);}
