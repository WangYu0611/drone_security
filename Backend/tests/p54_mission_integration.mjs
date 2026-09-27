import assert from 'node:assert/strict';
import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:19980',ws=process.env.P1_WS??'ws://127.0.0.1:19981/ws';
const out=process.env.P54_EVIDENCE_DIR??'Evidence/TASK-P5.4/protocol';fs.mkdirSync(out,{recursive:true});
const clients=[],passed=[],traffic=[];const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,body,code,method='POST'){const r=await fetch(base+path,body?{method,headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});const data=await r.json();traffic.push({at:new Date().toISOString(),path,status:r.status,data});if(code)assert.equal(data.code,code,JSON.stringify(data));else assert(r.ok,JSON.stringify(data));return data;}
async function connect(role){const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'P54-QA-'+role,client_role:role,hostname:'SYNTHETIC-QA',app_version:'P5.4'});const c={id,role,socket:new WebSocket(ws),events:[]};clients.push(c);c.socket.onmessage=e=>c.events.push(JSON.parse(e.data));await new Promise((r,j)=>{c.socket.onopen=r;c.socket.onerror=j;});c.socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));for(let i=0;i<100&&!c.events.some(e=>e.type==='context_subscribed');i++)await sleep(50);assert(c.events.some(e=>e.type==='context_subscribed'));c.timer=setInterval(()=>c.socket.send(JSON.stringify({type:'context_ping',hydrated:true})),2000);return c;}
const pass=name=>{passed.push(name);console.log('PASS '+name);};let failure;
try{
 const command=await connect('Command'),map=await connect('Map');const state=()=>api('/api/security-plans');let plan='',mission='';
 const send=async(c,action,extra={},code)=>api('/api/security-plans',{instance_id:c.id,action,plan_id:plan,mission_id:mission,expected_version:(await state()).version,...extra},code);
 if(process.argv.includes('--resume')){
  const fixture=JSON.parse(fs.readFileSync(out+'/schedule.json'));plan=fixture.plan;mission=fixture.mission;let r=await state();const e=r.executions[fixture.execution];assert(e);assert.equal(e.scheduled_start_at,fixture.when);pass('Backend restart retains scheduled execution and immutable route');
  const due=Date.parse(fixture.when);const samples=[];let running=false;
  while(Date.now()<due+15000){r=await state();const e=r.executions[fixture.execution];samples.push({at:Date.now(),state:e.state,position:e.position});if(Date.now()<due-500)assert.equal(e.state,'SCHEDULED');if(e.state==='EXECUTING'){running=true;assert(Date.now()-due<10000);break;}await sleep(250);}
  assert(running,'Scheduled mission did not start');pass('UTC plus two minutes schedule starts on time after Backend restart');
  const runningExecution=r.executions[fixture.execution];await send(command,'execution_abort',{execution_id:fixture.execution,request_id:crypto.randomUUID(),control_version:runningExecution.control_version});
  fs.writeFileSync(out+'/schedule-samples.json',JSON.stringify(samples,null,2));
 }else{
  const videoBefore=await api('/api/video-view');
  const members=['UAV-01','UAV-02','UAV-03','UAV-04'];
  await api('/api/context',{instance_id:command.id,patch:{active_uav_id:members[0],selected_uav_ids:members}},undefined,'PATCH');await sleep(150);
  assert(map.events.some(e=>e.type==='OperationalContextChanged' && e.payload.selected_uav_ids?.length===4));assert.deepEqual(await api('/api/video-view'),videoBefore);pass('Four-UAV selection synchronizes over WebSocket without switching Video');
  let r=await send(command,'create_plan',{name:'P5.4 Protocol Patrol',default_mission_name:'Four-aircraft patrol'});plan=r.plan_id;mission=r.mission_id;
  await send(command,'assign',{assigned_uav_ids:members,formation:'Line',spacing_m:3});
  let session=(await send(map,'begin_route_edit',{content_revision:(await state()).plans[plan].content_revision})).edit_session_id;
  const route={pathId:1,bClosedLoop:false,waypoints:[40,80,120,60].map((altitude,i)=>({sequence:i+1,latitude:39.9808+i*.00005,longitude:116.347075,altitude,altitude_reference:'Ellipsoid',waitTime:i===1?5:i===2?10:0,segmentSpeed:50,location:{x:i*1000,y:0,z:altitude*100}}))};
  const bad=structuredClone(route);bad.waypoints[0].altitude_reference='AGL';await send(map,'save_route',{edit_session_id:session,path:bad},'ALTITUDE_DATUM_UNRESOLVED');pass('Unresolved AGL fails visibly without a fabricated terrain datum');
  await send(map,'save_route',{edit_session_id:session,path:route,keep_editing:true});await send(map,'finish_route_edit',{edit_session_id:session});await send(command,'validate');await send(command,'review');r=await send(command,'deploy');assert.equal(r.state.plans[plan].status,'DEPLOYED');pass('Four-UAV plan validates and deploys with a separate central route');
  r=await send(command,'execution_start',{request_id:crypto.randomUUID()});let eid=r.execution_id;const group=r.state.executions[eid].group_id;const executions=Object.values(r.state.executions).filter(e=>e.group_id===group);assert.equal(executions.length,4);assert.equal(new Set(executions.map(e=>JSON.stringify(e.route_snapshot))).size,4);assert(executions.every(e=>JSON.stringify(e.plan_route_snapshot)===JSON.stringify(executions[0].plan_route_snapshot)));pass('Runtime creates four distinct immutable trajectories');
  const samples=[];let hover=false,complete=false;for(let i=0;i<180;i++){r=await state();const groupStates=Object.values(r.executions).filter(e=>e.group_id===group);samples.push(groupStates.map(e=>({id:e.uav_id,at:Date.now(),state:e.state,phase:e.phase,wait:e.wait_remaining,position:e.position})));hover ||= groupStates.some(e=>e.phase==='HOVER'&&e.wait_remaining>0);
   for(let a=0;a<4;a++)for(let b=a+1;b<4;b++){const p=groupStates[a].position,q=groupStates[b].position;const dx=(p.longitude-q.longitude)*Math.PI/180*6371000*Math.cos(p.latitude*Math.PI/180),dy=(p.latitude-q.latitude)*Math.PI/180*6371000,dz=p.altitude-q.altitude;assert(Math.hypot(dx,dy,dz)>=1.4999);}
   assert(!groupStates.some(e=>e.state==='FAILED'||e.failure_reason==='TRAJECTORY_CONFLICT'));if(groupStates.every(e=>e.state==='COMPLETED')){complete=true;break;}await sleep(250);}
  assert(complete);assert(hover);pass('Four aircraft complete 3D route with hover and at least 1.5 m separation');fs.writeFileSync(out+'/four-uav-samples.json',JSON.stringify(samples,null,2));
  r=await send(command,'execution_group_move',{request_id:crypto.randomUUID(),assigned_uav_ids:members,target:{latitude:39.9811,longitude:116.34703,altitude:80},target_area_radius_m:.1},'TRAJECTORY_CONFLICT');pass('Insufficient target space blocks group movement');
  r=await send(command,'execution_group_move',{request_id:crypto.randomUUID(),assigned_uav_ids:members,target:{latitude:39.9811,longitude:116.34703,altitude:80},target_area_radius_m:100});eid=r.execution_id;
  for(let i=0;i<60;i++){r=await state();if(r.executions[eid].state==='EXECUTING')break;await sleep(100);}await send(command,'execution_pause',{execution_id:eid,request_id:crypto.randomUUID(),control_version:r.executions[eid].control_version});r=await state();const gid=r.executions[eid].group_id;assert(Object.values(r.executions).filter(e=>e.group_id===gid).every(e=>e.state==='PAUSED'));await send(command,'execution_abort',{execution_id:eid,request_id:crypto.randomUUID(),control_version:r.executions[eid].control_version});pass('Group movement and controls use one atomic execution group');
  const when=new Date(Date.now()+120000).toISOString().replace(/\.\d{3}Z$/,'Z');r=await send(command,'execution_start',{request_id:crypto.randomUUID(),scheduled_start_at:when});eid=r.execution_id;assert.equal(r.state.executions[eid].state,'SCHEDULED');fs.writeFileSync(out+'/schedule.json',JSON.stringify({plan,mission,execution:eid,when},null,2));pass('Scheduled execution persists a UTC plus two minutes deadline');
 }
}catch(e){failure=String(e.stack??e);console.error(failure);process.exitCode=1;}
finally{for(const c of clients){clearInterval(c.timer);c.socket.close();}fs.writeFileSync(out+(process.argv.includes('--resume')?'/resume-result.json':'/result.json'),JSON.stringify({synthetic:true,passed,failure,traffic},null,2));}
