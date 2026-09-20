import assert from 'node:assert/strict';
import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:19180';
const url=process.env.P1_WS??'ws://127.0.0.1:19181/ws';
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const api=async(path,method='GET',body)=>{const r=await fetch(base+path,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined});return {status:r.status,data:await r.json()};};
const until=async f=>{for(let i=0;i<250;i++){if(f())return;await sleep(20);}throw Error('broadcast timeout');};
async function client(role){const id=crypto.randomUUID();await api('/api/context/clients','POST',{instance_id:id,client_id:'P3-QA-'+role,client_role:role,hostname:'SYNTHETIC-QA',app_version:'P3'});
const c={id,events:[],socket:new WebSocket(url)};c.socket.onmessage=e=>{const m=JSON.parse(e.data);c.events.push(m);if(m.type==='context_subscribed')c.ready=true;if(m.type==='OperationalContextChanged'||m.type==='context_subscribed')c.context=m.payload;};await new Promise((r,j)=>{c.socket.onopen=r;c.socket.onerror=j;});c.socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await until(()=>c.ready);return c;}
const clients=[];let results=[];
function pass(name){results.push(name);console.log('PASS '+name);}
try{
const map=await client('Map'),command=await client('Command'),video=await client('Video');clients.push(map,command,video);
const send=(c,action,body={})=>api('/api/security-plans','POST',{instance_id:c.id,action,...body});
let r=await send(map,'create_plan',{name:'P3 automated QA',description:'SYNTHETIC QA'});assert.equal(r.status,200);const p=r.data.plan_id;assert.equal(r.data.state.plans[p].status,'DRAFT');pass('P3.SecurityPlan.Create');
await until(()=>clients.every(c=>c.context?.active_security_plan_id===p));pass('P3.SecurityPlan.ActiveSelection');
const beforeUav=(await api('/api/context')).data.active_uav_id;
r=await send(map,'add_mission',{plan_id:p,name:'Mission-01'});const m=r.data.mission_id;assert(r.data.state.missions[m]);pass('P3.Mission.Create');
const m2=(await send(map,'add_mission',{plan_id:p,name:'Mission-02'})).data.mission_id;
for(const [mission_id,assigned_uav_id] of [[m,'UAV-01'],[m2,'UAV-02']])assert.equal((await send(map,'assign',{plan_id:p,mission_id,assigned_uav_id})).status,200);pass('P3.Mission.Assignment');
const unchanged=(await api('/api/security-plans')).data;
r=await send(map,'assign',{plan_id:p,mission_id:m2,assigned_uav_id:'UAV-01'});assert.equal(r.status,409);assert.equal(r.data.code,'UAV_ALREADY_ASSIGNED');assert.deepEqual((await api('/api/security-plans')).data,unchanged);pass('P3.Mission.DuplicateUavRejected');
r=await send(command,'deploy',{plan_id:p});assert.equal(r.status,409);assert.equal(r.data.code,'PLAN_NOT_READY');pass('P3.Plan.ValidationFailure');
const path={pathId:1,bClosedLoop:false,waypoints:[0,1,2].map((i)=>({sequence:i+1,latitude:39.98+i*.001,longitude:116.34+i*.001,altitude:60,location:{x:i*100,y:0,z:0},segmentSpeed:i?5:0,waitTime:0}))};
for(const mid of [m,m2])assert.equal((await send(map,'save_route',{plan_id:p,mission_id:mid,path})).status,200);pass('P3.Route.Create');
let snapshot=(await api('/api/security-plans')).data;for(const [i,wp] of snapshot.paths[snapshot.missions[m].route_id].waypoints.entries()){assert(Math.abs(wp.latitude-path.waypoints[i].latitude)<1e-10);assert(Math.abs(wp.longitude-path.waypoints[i].longitude)<1e-10);assert.equal(wp.sequence,i+1);assert.equal(wp.segmentSpeed,path.waypoints[i].segmentSpeed);assert.equal(wp.altitude,60);}pass('P3.Route.Persistence');
r=await send(map,'validate',{plan_id:p});assert.equal(r.data.state.plans[p].status,'READY');pass('P3.Plan.Ready');
assert.equal((await send(map,'deploy',{plan_id:p})).status,403);
r=await send(command,'deploy',{plan_id:p});assert.equal(r.status,200);assert.equal(r.data.state.plans[p].status,'DEPLOYED');assert([m,m2].every(id=>r.data.state.missions[id].status==='DEPLOYED'));pass('P3.Plan.Deploy');
await until(()=>clients.every(c=>c.events.some(e=>e.type==='SecurityPlansChanged'&&e.payload.plans[p]?.status==='DEPLOYED')));
await send(command,'select',{plan_id:p,mission_id:m});await until(()=>clients.every(c=>c.context?.active_mission_id===m));pass('P3.CrossClient.PlanSelection');
assert.equal((await api('/api/context')).data.active_uav_id,beforeUav);assert.equal((await send(video,'create_plan',{name:'forbidden'})).status,403);pass('P3.Video.NoPlanSideEffects.Protocol');
const late=await client('Map');clients.push(late);assert.equal(late.context.active_mission_id,m);snapshot=(await api('/api/security-plans')).data;assert.equal(snapshot.plans[p].status,'DEPLOYED');pass('P3.Plan.ClientRestartRecovery');
fs.writeFileSync('Evidence/TASK-P3/protocol-state.json',JSON.stringify(snapshot,null,2));fs.writeFileSync('Evidence/TASK-P3/protocol-context.json',JSON.stringify((await api('/api/context')).data,null,2));
fs.writeFileSync('Evidence/TASK-P3/protocol-results.json',JSON.stringify({executed:results.length,passed:results.length,tests:results,note:'Real HTTP/WS; simulated protocol clients, not native UE acceptance'},null,2));
}finally{for(const c of clients)c.socket.close();}