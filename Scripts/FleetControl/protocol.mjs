// HTTP/WS integration evidence; separate from native mouse validation.
import assert from 'node:assert/strict';
import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:20980';
const ws=process.env.P1_WS??'ws://127.0.0.1:20981/ws';
const out=process.env.FLEET_EVIDENCE??'Evidence/FleetControl/protocol';fs.mkdirSync(out,{recursive:true});
const clients=[],traffic=[],passed=[];const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,body,expected=200,method='POST') {
 const r=await fetch(base+path,body?{method,headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});
 const data=await r.json();traffic.push({path,status:r.status,data});assert.equal(r.status,expected,JSON.stringify(data));return data;
}
async function client(role) {
 const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'Fleet-QA-'+role,client_role:role,hostname:'ISOLATED-MOCK',app_version:'FleetControl'});
 const socket=new WebSocket(ws),events=[];socket.onmessage=e=>events.push(JSON.parse(e.data));await new Promise((r,j)=>{socket.onopen=r;socket.onerror=j;});socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));
 for(let i=0;i<100&&!events.some(e=>e.type==='context_subscribed');i++)await sleep(50);
 assert(events.some(e=>e.type==='context_subscribed'));
 const timer=setInterval(()=>socket.send(JSON.stringify({type:'context_ping',hydrated:true})),2000);const c={id,socket,timer,events};clients.push(c);return c;
}
const pass=name=>{passed.push(name);console.log('PASS '+name);};let failure;
try {
 const command=await client('Command'),map=await client('Map'),video=await client('Video');
 const state=()=>api('/api/security-plans');
 const send=async(c,action,extra={},status=200)=>api('/api/security-plans',{instance_id:c.id,expected_version:(await state()).version,action,...extra},status);
 const videoBefore=await api('/api/video-view');const members=['UAV-02','UAV-01','UAV-03','UAV-04'];
 await api('/api/context',{instance_id:map.id,patch:{active_uav_id:members[0],selected_uav_ids:members}},200,'PATCH');
 assert.deepEqual((await api('/api/context')).selected_uav_ids,members);assert.deepEqual(await api('/api/video-view'),videoBefore);pass('Map multi-selection is authoritative and leaves Video target unchanged');
 await send(map,'create_plan',{name:'forbidden'},403);await send(map,'execution_start',{},403);await send(video,'execution_group_move',{},403);pass('Map plan creation/start and Video execution remain forbidden');
 let r=await send(command,'create_plan',{name:'Fleet occupancy protocol',default_mission_name:'Patrol'});const plan=r.plan_id,mission=r.mission_id;
 await send(command,'assign',{plan_id:plan,mission_id:mission,assigned_uav_id:'UAV-01'});
 const session=(await send(map,'begin_route_edit',{plan_id:plan,mission_id:mission,content_revision:(await state()).plans[plan].content_revision})).edit_session_id;
 const home=(await state()).mock_execution.uavs['UAV-01'].home_position;
 const path={pathId:1,bClosedLoop:false,waypoints:[0,.00015].map((dy,i)=>({sequence:i+1,...home,latitude:home.latitude+dy,segmentSpeed:8,waitTime:0}))};
 await send(map,'save_route',{plan_id:plan,mission_id:mission,edit_session_id:session,path});
 for(const action of ['validate','review','deploy'])await send(command,action,{plan_id:plan,mission_id:mission});
 const draft=await send(command,'copy_plan',{plan_id:plan});
 r=await send(command,'execution_start',{plan_id:plan,mission_id:mission,request_id:crypto.randomUUID()});const planExecution=r.execution_id;
 const target={...home,latitude:home.latitude+.0001,longitude:home.longitude+.00003};
 r=await send(map,'execution_group_move',{request_id:crypto.randomUUID(),assigned_uav_ids:members,target},409);
 assert.equal(r.code,'DRONE_ACTIVE_PLAN_CONFLICT');assert.equal(r.params.active_plan_id,plan);pass('Map fleet move cannot steal an active Security Plan UAV');
 for(let i=0;i<100;i++){r=await state();if(r.executions[planExecution].state==='EXECUTING')break;await sleep(50);}assert.equal(r.executions[planExecution].state,'EXECUTING');
 await send(map,'execution_abort',{execution_id:planExecution,request_id:crypto.randomUUID(),control_version:(await state()).executions[planExecution].control_version},403);
 await send(command,'execution_abort',{execution_id:planExecution,request_id:crypto.randomUUID(),control_version:(await state()).executions[planExecution].control_version});pass('Map controls cannot abort a Security Plan execution');
 const requestId=crypto.randomUUID();const move={request_id:requestId,assigned_uav_ids:members,target,target_area_radius_m:10000};
 r=await send(map,'execution_group_move',move);const execution=r.execution_id,group=r.state.executions[execution].group_id;
 const groupItems=s=>Object.values(s.executions).filter(e=>e.group_id===group);
 let groupState=groupItems(r.state);assert.equal(groupState.length,4);assert(groupState.every(e=>e.formation==='RelativeOffset'));
 const anchor=groupState.find(e=>e.uav_id==='UAV-02').route_snapshot.waypoints;
 for(const e of groupState){const p=e.route_snapshot.waypoints;assert(Math.abs((p[1].longitude-p[0].longitude)-(anchor[1].longitude-anchor[0].longitude))<1e-9);assert(Math.abs((p[1].latitude-p[0].latitude)-(anchor[1].latitude-anchor[0].latitude))<1e-9);assert.equal(p[1].altitude,p[0].altitude);}
 await send(map,'execution_group_move',move);assert.equal(groupItems(await state()).length,4);pass('Map creates one idempotent four-UAV group preserving the chosen primary anchor and formation');
 for(let i=0;i<100;i++){r=await state();if(r.executions[execution].state==='EXECUTING')break;await sleep(50);}assert.equal(r.executions[execution].state,'EXECUTING');
 await send(map,'execution_pause',{request_id:crypto.randomUUID(),execution_id:execution,control_version:r.executions[execution].control_version});
 r=await state();assert(groupItems(r).every(e=>e.state==='PAUSED'));
 let conflict=await send(command,'assign',{plan_id:draft.plan_id,mission_id:draft.mission_id,assigned_uav_id:'UAV-01'},409);assert.equal(conflict.code,'DRONE_ACTIVE_PLAN_CONFLICT');pass('Paused fleet blocks Security Plan assignment');
 await send(map,'execution_resume',{request_id:crypto.randomUUID(),execution_id:execution,control_version:r.executions[execution].control_version});
 const samples=[];let completed=false;
 for(let i=0;i<150;i++){r=await state();const group=groupItems(r);samples.push(group.map(e=>({uav:e.uav_id,state:e.state,position:e.position})));
  for(let a=0;a<group.length;a++)for(let b=a+1;b<group.length;b++){const p=group[a].position,q=group[b].position;const d=Math.hypot((p.latitude-q.latitude)*Math.PI/180*6371000,(p.longitude-q.longitude)*Math.PI/180*6371000*Math.cos(p.latitude*Math.PI/180),p.altitude-q.altitude);assert(d>=1.4999);}
  if(group.every(e=>e.state==='COMPLETED')){completed=true;break;}await sleep(100);
 }
 assert(completed);assert(groupItems(r).every(e=>!e.failure_reason));fs.writeFileSync(out+'/movement-samples.json',JSON.stringify(samples,null,2));
 await send(command,'assign',{plan_id:draft.plan_id,mission_id:draft.mission_id,assigned_uav_id:'UAV-01'});pass('Map pause/resume controls the entire group, all four complete safely, and occupancy releases');
} catch(e){failure=String(e.stack??e);console.error(failure);process.exitCode=1;}
finally {for(const c of clients){clearInterval(c.timer);c.socket.close();}fs.writeFileSync(out+'/result.json',JSON.stringify({synthetic:true,passed,failure,traffic},null,2));}
