import assert from 'node:assert/strict';
import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:19580',ws=process.env.P1_WS??'ws://127.0.0.1:19581/ws';
const out=process.env.P51_EVIDENCE_DIR??'Evidence/TASK-P5.1/automation/p51-protocol';fs.mkdirSync(out,{recursive:true});
const clients=[],passed=[],traffic=[];
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,body){const r=await fetch(base+path,body?{method:path==='/api/ui-preferences'?'PATCH':'POST',headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});const data=await r.json();traffic.push({path,status:r.status,data});assert(r.ok,JSON.stringify(data));return data;}
async function connect(role){const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'P51-QA-'+role,client_role:role,hostname:'SYNTHETIC-QA',app_version:'P5.1'});
 const c={id,role,socket:new WebSocket(ws),events:[]};clients.push(c);c.socket.onmessage=e=>c.events.push(JSON.parse(e.data));await new Promise((r,j)=>{c.socket.onopen=r;c.socket.onerror=j;});
 c.socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));for(let i=0;i<100&&!c.events.some(e=>e.type==='context_subscribed');i++)await sleep(50);
 assert(c.events.some(e=>e.type==='context_subscribed'));c.timer=setInterval(()=>c.socket.send(JSON.stringify({type:'context_ping',hydrated:true})),2000);return c;}
const pass=name=>{passed.push(name);console.log('PASS '+name);};
try{
 const command=await connect('Command'),map=await connect('Map'),video=await connect('Video');
 let plan='',mission='',session='';
 const state=()=>api('/api/security-plans');
 const send=async(c,action,extra={})=>api('/api/security-plans',{instance_id:c.id,action,plan_id:plan,mission_id:mission,expected_version:(await state()).version,...extra});
 let r=await send(command,'create_plan',{name:'P5.1 protocol workflow',default_mission_name:'Task 01'});plan=r.plan_id;mission=r.mission_id;
 assert.equal(r.state.plans[plan].mission_ids.length,1);assert.equal(r.context.active_security_plan_id,plan);assert.equal(r.context.active_mission_id,mission);pass('Create automatically configures one task and selects workspace context');
 await send(command,'set_workflow_step',{workflow_step:'BasicInfo'});assert.equal((await state()).plans[plan].workflow_step,'BasicInfo');
 await send(command,'set_workflow_step',{workflow_step:'TaskConfig'});pass('Basic and task steps persist without changing domain status');
 await send(command,'assign',{assigned_uav_id:'UAV-01'});await send(command,'request_map_route_edit');await sleep(150);
 assert(map.events.some(e=>e.type==='MapRouteEditRequested'&&e.payload.plan_id===plan));pass('Command routes the active task to Map');
 r=await send(map,'begin_route_edit',{content_revision:(await state()).plans[plan].content_revision,workflow:true});session=r.edit_session_id;
 assert.equal(r.state.plans[plan].workflow_step,'RouteEditing');pass('Map editing step is authoritative and recoverable');
 const path={pathId:1,bClosedLoop:false,waypoints:[0,1,2].map(i=>({sequence:i+1,latitude:39.98+i*.001,longitude:116.34+i*.001,altitude:80,segmentSpeed:5,waitTime:0}))};
 await send(map,'mark_route_dirty',{edit_session_id:session});r=await send(map,'save_route',{edit_session_id:session,path,keep_editing:true});
 assert.equal(r.state.edit_sessions[session].state,'EDITING');assert.equal(r.state.plans[plan].workflow_step,'RouteEditing');pass('Save draft keeps editing active');
 const before=(await state()).plans[plan];await api('/api/ui-preferences',{instance_id:command.id,language:'zh-Hans'});await api('/api/ui-preferences',{instance_id:video.id,language:'en'});assert.deepEqual((await state()).plans[plan],before);pass('Both language directions preserve workflow and content');
 const late=await connect('Command');assert(late.events.some(e=>JSON.stringify(e).includes(plan)) || (await state()).plans[plan].workflow_step==='RouteEditing');pass('Fresh Command reads saved editing context');
 r=await send(map,'finish_route_edit',{edit_session_id:session});assert.equal(r.state.plans[plan].workflow_step,'PreDeployReview');assert(!r.state.edit_sessions[session]);pass('Finish ends editing and enters pre-deployment check');
 await send(command,'validate');await send(command,'review');r=await send(command,'deploy');assert.equal(r.state.plans[plan].status,'DEPLOYED');const deployment=r.state.deployments;pass('Checked reviewed configuration deploys');
 const deny=await fetch(base+'/api/security-plans',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({instance_id:command.id,action:'update_plan',plan_id:plan,name:'Forbidden',expected_version:(await state()).version})});assert.equal((await deny.json()).code,'PLAN_DEPLOYED');pass('Deployed plan rejects direct editing');
 r=await send(command,'copy_plan');assert.notEqual(r.plan_id,plan);assert(r.mission_id);assert.equal(r.state.plans[r.plan_id].status,'DRAFT');assert.equal(r.state.plans[r.plan_id].workflow_step,'TaskConfig');assert.deepEqual(r.state.deployments,deployment);pass('New version selects new draft and preserves immutable deployment');
 fs.writeFileSync(out+'/result.json',JSON.stringify({synthetic:true,passed,traffic},null,2));
}finally{for(const c of clients){clearInterval(c.timer);c.socket.close();}}
