// Owns an isolated EMPTY-fleet Backend; never connects to the user's running host.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import net from 'node:net';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const executable=path.resolve(process.argv[2]??path.join(root,'Backend/build/Release/DroneBackend.exe'));
const output=path.join(root,'Saved/ConfigurationWorkflow',`protocol-${Date.now()}`);
const httpPort=23180,wsPort=23181,base=`http://127.0.0.1:${httpPort}`;
for(const port of [httpPort,wsPort])await new Promise((resolve,reject)=>{
  const probe=net.createServer();probe.once('error',reject);probe.listen(port,'127.0.0.1',()=>probe.close(resolve));
});
fs.mkdirSync(path.join(output,'data'),{recursive:true});fs.mkdirSync(path.join(output,'Logs'));
fs.writeFileSync(path.join(output,'data/drones.json'),'[]');
fs.writeFileSync(path.join(output,'data/mock_execution.json'),JSON.stringify({default_speed_mps:5,uavs:{}}));
fs.writeFileSync(path.join(output,'backend.yaml'),`server:\n  http_port: ${httpPort}\n  ws_port: ${wsPort}\n  debug: false\nport_map: {}\nstorage:\n  path: "data/drones.json"\n  video_metadata_path: "data/video"\nlog:\n  level: "info"\n  file: "Logs/backend.log"\n`);
const log=fs.openSync(path.join(output,'stdio.log'),'w');
const backend=spawn(executable,[path.join(output,'backend.yaml')],{cwd:output,stdio:['ignore',log,log],windowsHide:true});
let stopped=false;const exited=new Promise(resolve=>backend.on('exit',()=>{stopped=true;resolve();}));
const clients=[],checks=[],traffic=[];const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function api(route,method='GET',body){
  const response=await fetch(base+route,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined,signal:AbortSignal.timeout(5000)});
  const data=await response.json();traffic.push({route,method,body,status:response.status,data});return {status:response.status,data};
}
async function client(role){
  const instance=crypto.randomUUID();assert.equal((await api('/api/context/clients','POST',{instance_id:instance,client_id:`CONFIG-QA-${role}`,client_role:role,hostname:'ISOLATED-QA',app_version:'configuration-workflow'})).status,200);
  const socket=new WebSocket(`ws://127.0.0.1:${wsPort}/ws`);const c={instance,socket,ready:false};clients.push(c);
  socket.onmessage=e=>{if(JSON.parse(e.data).type==='context_subscribed')c.ready=true;};
  await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
  socket.send(JSON.stringify({type:'subscribe_context',instance_id:instance}));
  for(let i=0;!c.ready && i<100;++i)await sleep(20);assert(c.ready);
  c.timer=setInterval(()=>{if(socket.readyState===1)socket.send(JSON.stringify({type:'context_ping',hydrated:true}));},3000);return c;
}
const send=(c,action,body={})=>api('/api/security-plans','POST',{instance_id:c.instance,action,...body});
function passed(text){checks.push(text);console.log('PASS '+text);}
let failure;
try {
  for(let i=0;i<100;++i){try{const health=await api('/');assert(health.data.capabilities.includes('plan_configuration'));break;}catch(error){if(stopped || i===99)throw error;await sleep(100);}}
  const command=await client('Command'),other=await client('Command'),map=await client('Map'),video=await client('Video');
  const original=await send(command,'create_plan',{name:'Existing plan',default_mission_name:'Existing mission'});
  assert.equal(original.status,200);const otherPlan=original.data.plan_id;
  for(const c of [map,video])assert.equal((await send(c,'begin_configuration')).status,403);
  assert.equal((await send(command,'begin_configuration')).status,200);
  assert.equal((await send(other,'begin_configuration')).data.code,'CONFIGURATION_IN_PROGRESS');
  passed('Only Command owns configuration; concurrent owner is rejected');
  const created=await send(command,'create_plan',{name:'Configuration plan',default_mission_name:'Configured mission'});
  assert.equal(created.status,200);const plan=created.data.plan_id,mission=created.data.mission_id;
  assert.equal(created.data.state.configuration_session.plan_id,plan);
  assert.equal((await send(command,'begin_configuration',{plan_id:plan})).status,200);
  passed('New plan binds its configuration owner and repeat begin is idempotent');
  for(const action of ['execution_start','execution_start_now','execution_group_move','begin_plan_move']){
    const c=action==='begin_plan_move'?map:command;
    assert.equal((await send(c,action,{plan_id:otherPlan,request_id:crypto.randomUUID()})).data.code,'CONFIGURATION_IN_PROGRESS');
  }
  assert.equal((await send(command,'request_map_plan_move',{plan_id:plan,mission_id:mission})).data.code,'CONFIGURATION_IN_PROGRESS');
  passed('Both clients reject new movement during configuration');
  assert.equal((await send(other,'update_plan',{plan_id:plan,name:'Unowned change'})).data.code,'CONFIGURATION_IN_PROGRESS');
  assert.equal((await send(map,'select',{plan_id:otherPlan})).data.code,'CONFIGURATION_IN_PROGRESS');
  assert.equal((await send(map,'select',{plan_id:plan,mission_id:mission})).data.code,'CONFIGURATION_IN_PROGRESS');
  assert.equal((await send(other,'select',{plan_id:plan,mission_id:mission})).data.code,'CONFIGURATION_IN_PROGRESS');
  const patch=(c,fields)=>api('/api/context','PATCH',{instance_id:c.instance,patch:fields});
  assert.equal((await patch(other,{active_security_plan_id:otherPlan,active_mission_id:null})).data.code,'CONFIGURATION_IN_PROGRESS');
  assert.equal((await patch(other,{active_security_plan_id:plan,active_mission_id:mission})).data.code,'CONFIGURATION_IN_PROGRESS');
  assert.equal((await patch(command,{active_uav_id:'UAV-01',active_alert_id:'ALERT-QA'})).status,200);
  assert.equal((await api('/api/security-plans')).status,200);assert.equal((await api('/api/context')).status,200);
  passed('Alternate context selection cannot escape plan; alert/UAV selection and reads remain available');
  const state=(await api('/api/security-plans')).data;
  const edit=await send(map,'begin_route_edit',{plan_id:plan,mission_id:mission,content_revision:state.plans[plan].content_revision});
  assert.equal(edit.status,200);const edit_session_id=edit.data.edit_session_id;
  assert.equal((await send(map,'mark_route_dirty',{plan_id:plan,mission_id:mission,edit_session_id})).status,200);
  assert.equal((await send(command,'end_configuration',{plan_id:plan})).data.code,'PLAN_EDIT_IN_PROGRESS');
  assert.equal((await send(map,'discard_route_edit',{plan_id:plan,mission_id:mission,edit_session_id})).status,200);
  assert.equal((await send(command,'end_configuration',{plan_id:plan})).status,200);
  assert.equal((await api('/api/security-plans')).data.configuration_session,null);
  passed('Map route editing is retained; configuration exit requires explicit route finish/discard');
  assert.equal((await send(map,'select',{plan_id:otherPlan})).status,200);
  assert.equal((await send(other,'begin_configuration',{plan_id:otherPlan})).status,200);
  assert.equal((await send(other,'end_configuration')).status,200);
  passed('Released lease permits normal selection and a new configuration owner');
} catch(error){failure=error.stack;console.error(failure);process.exitCode=1;}
finally {
  for(const c of clients){clearInterval(c.timer);c.socket.close();}
  if(!stopped)backend.kill();await exited;fs.closeSync(log);
  fs.writeFileSync(path.join(output,'result.json'),JSON.stringify({pass:!failure,checks,failure:failure??null,pid:backend.pid,owned_backend_stopped:stopped,empty_fleet:true,empty_port_map:true},null,2));
  fs.writeFileSync(path.join(output,'traffic.json'),JSON.stringify(traffic,null,2));console.log(output);
}
