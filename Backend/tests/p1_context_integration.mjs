// Run against a dedicated local Backend: node Backend/tests/p1_context_integration.mjs
import assert from 'node:assert/strict';
const base=process.env.P1_HTTP ?? 'http://127.0.0.1:18080';
const wsUrl=process.env.P1_WS ?? 'ws://127.0.0.1:18081/ws';
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,method='GET',body) {
 const r=await fetch(base+path,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined});
 const data=await r.json(); return {status:r.status,data};
}
async function until(predicate,label) {const t=Date.now();while(!predicate()){assert(Date.now()-t<5000,label);await pause(20);}}
async function client(role,previous) {
 const instance_id=previous?.instance_id ?? crypto.randomUUID(),client_id='P1-TEST-'+role;
 assert.equal((await api('/api/context/clients','POST',{instance_id,client_id,client_role:role,hostname:'local-test',app_version:'P1-test'})).status,200);
 const snapshot=(await api('/api/context')).data;
 const c={instance_id,client_id,role,version:snapshot.context_version,context:snapshot,events:[],applies:0};
 c.socket=new WebSocket(wsUrl);
 c.socket.onmessage=e=>{const msg=JSON.parse(e.data);c.events.push(msg);if(['OperationalContextChanged','context_subscribed'].includes(msg.type)){
  if(msg.payload.context_version>c.version){c.version=msg.payload.context_version;c.context=msg.payload;c.applies++;}
  if(msg.type==='context_subscribed')c.ready=true;
 }};
 await new Promise((resolve,reject)=>{c.socket.onopen=resolve;c.socket.onerror=reject;});
 c.socket.send(JSON.stringify({type:'subscribe_context',instance_id}));await until(()=>c.ready,'subscription');return c;
}
let clients=[];
try {
 const command=await client('Command'),map=await client('Map'),video=await client('Video');clients=[command,map,video];
 const update=(c,patch)=>api('/api/context','PATCH',{instance_id:c.instance_id,patch});
 for(const [c,id] of [[command,'UAV-02'],[map,'UAV-03'],[video,'UAV-01']]){
  const t=Date.now();const r=await update(c,{active_uav_id:id});assert.equal(r.status,200);
  await until(()=>clients.every(c=>c.context.active_uav_id===id),'three-client broadcast');
  console.log(`PASS P1.${c.role}Sync ${id} ${Date.now()-t} ms`);
 }
 const version=command.version;await update(command,{active_uav_id:'UAV-01'});await pause(150);
 assert(clients.every(c=>c.version===version));console.log('PASS P1.Noop no version bump or feedback loop');
 const alert=(await api('/api/context/debug/alert','POST',{drone_id:3})).data;
 await until(()=>clients.every(c=>c.events.some(e=>e.alert_id===alert.alert_id)),'alert broadcast');
 await update(command,{active_uav_id:'UAV-03',active_alert_id:alert.alert_id,operation_mode:'ALERT_RESPONSE'});
 await until(()=>clients.every(c=>c.context.active_alert_id===alert.alert_id&&c.context.active_uav_id==='UAV-03'),'atomic alert');
 console.log('PASS P1.AlertSync atomic alert/UAV/mode');
 const before=command.version;
 assert.equal((await update(command,{active_area_id:'AREA-A',operation_mode:'INVALID'})).status,400);
 assert.equal((await update(video,{active_mission_id:'MISSION-X'})).status,403);
 assert.equal((await api('/api/context')).data.context_version,before);console.log('PASS P1.ValidationAndRolePolicy');
 await update(command,{active_mission_id:'MISSION-2',active_security_plan_id:'PLAN-A',active_area_id:'AREA-B',operation_mode:'MISSION_EXECUTION'});
 await until(()=>clients.every(c=>c.context.active_area_id==='AREA-B'&&c.context.active_mission_id==='MISSION-2'),'all context fields');
 console.log('PASS P1.MissionPlanAreaMode');
 map.socket.close();await until(()=>command.events.some(e=>e.type==='client_disconnected'&&e.payload.instance_id===map.instance_id),'disconnect event');
 await update(command,{active_uav_id:'UAV-02'});
 const joined=await client('Map');clients.push(joined);assert.equal(joined.context.active_uav_id,'UAV-02');
 console.log('PASS P1.LateJoinRestart snapshot restores UAV-02');
 const records=(await api('/api/context/clients')).data;
 assert.equal(records.find(c=>c.instance_id===map.instance_id).state,'OFFLINE');
 assert.equal(records.find(c=>c.instance_id===joined.instance_id).state,'ONLINE');console.log('PASS P1.ClientConnectionState');
 const reconnected=await client('Map',map);clients.push(reconnected);
 assert.equal(reconnected.context.active_uav_id,'UAV-02');
 const newRecords=(await api('/api/context/clients')).data;
 assert(newRecords.find(c=>c.instance_id===map.instance_id).client_version>records.find(c=>c.instance_id===map.instance_id).client_version);
 assert.equal(newRecords.find(c=>c.instance_id===map.instance_id).state,'ONLINE');
 console.log('PASS P1.Reconnect same instance, fresh snapshot and newer presence version');
 const beforeQuiet=(await api('/api/context')).data.context_version;await pause(500);
 assert.equal((await api('/api/context')).data.context_version,beforeQuiet);console.log('PASS P1.NoEventLoop');
}finally{for(const c of clients)c.socket.close();}
