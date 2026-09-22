import assert from 'node:assert/strict';
import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:19680',ws=process.env.P1_WS??'ws://127.0.0.1:19681/ws';
const out=process.env.P52_EVIDENCE_DIR??'Evidence/TASK-P5.2/protocol';fs.mkdirSync(out,{recursive:true});
const passed=[],clients=[],timeline=[];const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,body,ok=true){const r=await fetch(base+path,body?{method:path==='/api/ui-preferences'?'PATCH':'POST',headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});const j=await r.json();if(ok)assert(r.ok,JSON.stringify(j));return j;}
async function connect(role){const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'P52-QA-'+role,client_role:role,hostname:'SYNTHETIC-QA',app_version:'P5.2'});
 const c={id,role,socket:new WebSocket(ws),events:[]};clients.push(c);c.socket.onmessage=e=>{const v=JSON.parse(e.data);c.events.push(v);if(v.type==='SecurityPlansChanged')for(const x of Object.values(v.payload.executions??{}))timeline.push({timestamp:Date.now(),id:x.execution_id,state:x.state,position:x.position,progress:x.progress,current_waypoint:x.current_waypoint});};await new Promise((r,j)=>{c.socket.onopen=r;c.socket.onerror=j;});c.socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));for(let i=0;i<100&&!c.events.some(e=>e.type==='context_subscribed');i++)await sleep(50);assert(c.events.some(e=>e.type==='context_subscribed'));c.timer=setInterval(()=>c.socket.send(JSON.stringify({type:'context_ping',hydrated:true})),2000);return c;}
const pass=x=>{passed.push(x);console.log('PASS '+x);};
try{
 const command=await connect('Command'),map=await connect('Map'),video=await connect('Video');
 const snapshot=()=>api('/api/security-plans');let plan,mission,id;
 const send=async(c,action,extra={},ok=true)=>api('/api/security-plans',{instance_id:c.id,action,plan_id:plan,mission_id:mission,expected_version:(await snapshot()).version,...extra},ok);
 let r=await send(command,'create_plan',{name:'P5.2 protocol only',default_mission_name:'Task 01'});plan=r.plan_id;mission=r.mission_id;
 await send(command,'assign',{assigned_uav_id:'UAV-01'});r=await send(map,'begin_route_edit',{content_revision:(await snapshot()).plans[plan].content_revision,workflow:true});
 const path={pathId:1,bClosedLoop:false,waypoints:[0,1,2,3].map(i=>({sequence:i+1,latitude:39.9807+i*.0001,longitude:116.34703+i*.0001,altitude:30,segmentSpeed:8,waitTime:0}))};
 await send(map,'save_route',{edit_session_id:r.edit_session_id,path,keep_editing:true});await send(map,'finish_route_edit',{edit_session_id:r.edit_session_id});await send(command,'validate');await send(command,'review');r=await send(command,'deploy');
 assert.equal(r.state.plans[plan].status,'DEPLOYED');assert.equal(Object.keys(r.state.executions).length,0);pass('Deploy does not automatically start');
 const start=async()=>{const request_id=crypto.randomUUID();const x=await send(command,'execution_start',{request_id});id=x.execution_id;return request_id;};
 const exec=async()=> (await snapshot()).executions[id];
 const wait=async state=>{for(let i=0;i<400;i++){if((await exec()).state===state)return;await sleep(50);}throw Error('Expected '+state);};
 const control=async action=>send(command,'execution_'+action,{request_id:crypto.randomUUID(),execution_id:id,control_version:(await exec()).control_version});
 const request_id=await start();await wait('EXECUTING');pass('Create/preflight/start is backend authoritative');
 const again=await send(command,'execution_start',{request_id});assert.equal(again.execution_id,id);assert.equal(Object.keys(again.state.executions).length,1);pass('Repeated start idempotency');
 assert.equal((await send(command,'execution_start',{request_id:crypto.randomUUID()},false)).code,'EXECUTION_CONFLICT');pass('Concurrent execution rejected');
 await sleep(600);assert((await exec()).progress>0);assert(map.events.some(e=>e.payload?.executions?.[id]?.progress>0));pass('Position and waypoint WS broadcast');
 await control('pause');const paused=await exec();await sleep(700);assert.deepEqual(await exec(),paused);pass('Pause freezes position and progress');
 await control('resume');await sleep(400);assert((await exec()).progress>paused.progress);pass('Resume continues same execution');
 const view=await api('/api/video-view');await api('/api/ui-preferences',{instance_id:video.id,language:'zh-Hans'});await api('/api/ui-preferences',{instance_id:command.id,language:'en'});assert.deepEqual(await api('/api/video-view'),view);assert.equal((await exec()).state,'EXECUTING');pass('Language and video target isolation');
 const before=await exec();await send(command,'copy_plan');assert.deepEqual((await exec()).route_snapshot,before.route_snapshot);pass('New draft preserves immutable route revision');
 const late=await connect('Command');assert(late.events.some(e=>e.type==='context_subscribed'));assert.equal((await exec()).execution_id,id);pass('New client hydrates current execution');
 await control('return');await wait('COMPLETED');assert.equal((await exec()).completion_reason,'RETURNED_HOME');assert.deepEqual((await exec()).position,(await exec()).home_position);pass('Return reaches explicit home');
 await start();await wait('EXECUTING');await control('abort');const aborted=await exec();await sleep(300);assert.deepEqual(await exec(),aborted);pass('Abort is terminal and stationary');
 await start();await wait('EXECUTING');await wait('COMPLETED');assert.equal((await exec()).completed_waypoints,4);assert.equal((await exec()).progress,1);pass('Normal route completion');
 await start();await wait('EXECUTING');await api('/api/mock-executions/pause-all',{request_id:crypto.randomUUID(),simulation:true});assert.equal((await exec()).state,'PAUSED');pass('Unified shutdown pauses durably');
 fs.writeFileSync(out+'/result.json',JSON.stringify({synthetic:true,passed,execution_id:id,plan_id:plan,mission_id:mission},null,2));
 fs.writeFileSync(out+'/timeline.json',JSON.stringify(timeline,null,2));fs.writeFileSync(out+'/final-state.json',JSON.stringify(await snapshot(),null,2));
}finally{for(const c of clients){clearInterval(c.timer);c.socket.close();}}
