// Isolated Backend protocol checks. This is neither native mouse nor real flight evidence.
// --seed-only creates one deployed plan without executions for a separately owned QA host.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
const base=process.env.P1_HTTP, ws=process.env.P1_WS, seedOnly=process.argv.includes('--seed-only');
for(const value of [base,ws])assert(value && ['127.0.0.1','localhost'].includes(new URL(value).hostname),'Explicit loopback P1_HTTP/P1_WS required');
const out=process.env.PLAN_LIFECYCLE_OUT;
if(!seedOnly)assert(out && process.env.PLAN_LIFECYCLE_ISOLATED==='1','Use the isolated plan_lifecycle_verify.py owner');
const clients=[],passed=[],sleep=ms=>new Promise(r=>setTimeout(r,ms));
const record=name=>{passed.push(name);console.log('PASS '+name);};
async function request(url,body,method='POST'){
  const response=await fetch(base+url,body?{method,headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});
  return {status:response.status,body:await response.json()};
}
async function api(url,body,method){const response=await request(url,body,method);assert(response.status<300,JSON.stringify(response.body));return response.body;}
const snapshot=()=>api('/api/security-plans');
async function connect(role){
  const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'PlanLifecycle-'+role,client_role:role,hostname:'ISOLATED-QA',app_version:'PlanLifecycle'});
  const socket=new WebSocket(ws),client={id,socket};clients.push(client);
  await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
  const ready=new Promise((resolve,reject)=>{socket.onmessage=e=>{if(JSON.parse(e.data).type==='context_subscribed')resolve();};socket.onerror=reject;});
  socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await ready;
  client.timer=setInterval(()=>socket.send(JSON.stringify({type:'context_ping',hydrated:true})),5000);return client;
}
async function send(client,action,extra={},expectedCode=''){
  let response;
  for(let retry=0;retry<3;retry++){
    response=await request('/api/security-plans',{instance_id:client.id,action,expected_version:(await snapshot()).version,...extra});
    if(response.body.code!=='VERSION_CONFLICT' || Object.hasOwn(extra,'expected_version') || expectedCode)break;
  }
  if(expectedCode)assert.equal(response.body.code,expectedCode,JSON.stringify(response.body));
  else assert(response.status<300,JSON.stringify(response.body));return response.body;
}
async function seed(command,map){
  const created=await send(command,'create_plan',{name:'Plan lifecycle deployed fixture',default_mission_name:'Original task',description:'Isolated same-ID edit and named deletion QA'});
  const plan_id=created.plan_id,mission_id=created.mission_id;
  await send(command,'assign',{plan_id,mission_id,assigned_uav_id:'UAV-01'});
  const state=await snapshot(),home=state.mock_execution.uavs['UAV-01'].home_position;
  const edit=await send(map,'begin_route_edit',{plan_id,mission_id,content_revision:state.plans[plan_id].content_revision,workflow:true});
  const route={pathId:1,bClosedLoop:false,waypoints:[0,1,2].map(i=>({sequence:i+1,latitude:home.latitude+i*.00015,longitude:home.longitude,altitude:home.altitude,altitude_reference:'Ellipsoid',segmentSpeed:5,waitTime:0}))};
  await send(map,'save_route',{plan_id,mission_id,edit_session_id:edit.edit_session_id,path:route});
  for(const action of ['validate','review','deploy'])await send(command,action,{plan_id});
  const deployed=await snapshot();assert.equal(deployed.plans[plan_id].status,'DEPLOYED');assert.equal(deployed.configuration_session,null);
  await send(command,'select',{plan_id,mission_id});return {plan_id,mission_id,deployment_id:deployed.plans[plan_id].deployment_id};
}
const consent=(state,id)=>({plan_id:id,confirmation_name:state.plans[id].name,expected_content_revision:state.plans[id].content_revision});
try{
  const command=await connect('Command'),map=await connect('Map');
  const ids=await seed(command,map);console.log(JSON.stringify({seed:ids,executions_started:0}));
  if(!seedOnly){
    fs.mkdirSync(out,{recursive:true});const {plan_id,mission_id,deployment_id}=ids;
    const before=await snapshot(),oldDeployment=before.deployments[deployment_id],routeId=before.missions[mission_id].route_id;
    const videoBefore=await api('/api/video-view');
    let revision=before.plans[plan_id].content_revision;
    await send(command,'reopen_plan',{plan_id,expected_content_revision:revision},'CONFIGURATION_REQUIRED');
    const forbidden=await request('/api/security-plans',{instance_id:map.id,action:'reopen_plan',plan_id,expected_version:before.version,expected_content_revision:revision});assert.equal(forbidden.status,403);
    await send(command,'begin_configuration',{plan_id});
    const other=await connect('Command');await send(other,'reopen_plan',{plan_id,expected_content_revision:revision},'CONFIGURATION_IN_PROGRESS');
    await send(command,'reopen_plan',{plan_id,expected_content_revision:revision-1},'CONTENT_REVISION_CONFLICT');
    await send(command,'reopen_plan',{plan_id,expected_content_revision:revision});
    let state=await snapshot();assert.equal(Object.keys(state.plans).length,Object.keys(before.plans).length);assert.deepEqual(state.plans[plan_id].mission_ids,[mission_id]);
    assert.equal(state.missions[mission_id].route_id,routeId);assert.deepEqual(state.paths[routeId],before.paths[routeId]);assert.deepEqual(state.deployments, before.deployments);
    await send(command,'update_plan',{plan_id,name:'Original plan edited in place',description:'Same identity, new contents'});
    for(const action of ['validate','review','deploy'])await send(command,action,{plan_id});
    state=await snapshot();assert.equal(state.plans[plan_id].status,'DEPLOYED');assert.equal(Object.keys(state.plans).length,Object.keys(before.plans).length);
    assert.equal(Object.keys(state.deployments).length,Object.keys(before.deployments).length+1);assert.deepEqual(state.deployments[deployment_id],oldDeployment);
    record('same ID / task / route survives edit-save-review-redeploy; old deployment unchanged');

    const scheduled=await send(command,'execution_start',{plan_id,mission_id,request_id:crypto.randomUUID(),scheduled_start_at:new Date(Date.now()+120000).toISOString().replace(/\.\d{3}Z$/,'Z')});
    let execution=scheduled.state.executions[scheduled.execution_id];assert.equal(execution.state,'SCHEDULED');
    await send(command,'delete_plan',consent(await snapshot(),plan_id),'PLAN_EXECUTING');
    await send(command,'begin_configuration',{plan_id},'PLAN_EXECUTING');assert.equal((await snapshot()).configuration_session,null);
    await send(command,'reopen_plan',{plan_id,expected_content_revision:(await snapshot()).plans[plan_id].content_revision},'CONFIGURATION_REQUIRED');
    await send(command,'execution_cancel',{plan_id,mission_id,execution_id:execution.execution_id,request_id:crypto.randomUUID(),control_version:execution.control_version});
    record('scheduled task blocks editing and deletion until explicit cancellation');

    const started=await send(command,'execution_start',{plan_id,mission_id,request_id:crypto.randomUUID()});
    for(let tries=0;tries<100;tries++){execution=(await snapshot()).executions[started.execution_id];if(execution.state==='EXECUTING')break;await sleep(50);}
    assert.equal(execution.state,'EXECUTING');await send(command,'delete_plan',consent(await snapshot(),plan_id),'PLAN_EXECUTING');
    await send(command,'execution_abort',{plan_id,mission_id,execution_id:execution.execution_id,request_id:crypto.randomUUID(),control_version:execution.control_version});
    record('actual Mock execution blocks deletion; abort remains independent');

    const copied=await send(command,'copy_plan',{plan_id});const copy=copied.plan_id,copyMission=copied.mission_id;assert.notEqual(copy,plan_id);
    const copyBefore=await snapshot();assert.deepEqual(copyBefore.deployments[deployment_id],oldDeployment);
    await send(command,'begin_configuration',{plan_id:copy});await send(command,'delete_plan',consent(await snapshot(),copy),'CONFIGURATION_IN_PROGRESS');await send(command,'end_configuration',{plan_id:copy});
    const edit=await send(map,'begin_route_edit',{plan_id:copy,mission_id:copyMission,content_revision:(await snapshot()).plans[copy].content_revision});
    await send(command,'delete_plan',consent(await snapshot(),copy),'PLAN_EDIT_IN_PROGRESS');await send(map,'discard_route_edit',{plan_id:copy,mission_id:copyMission,edit_session_id:edit.edit_session_id});
    record('explicit copy is separate; configuration and route-edit leases block deletion');

    let frozen=consent(await snapshot(),copy);
    await send(command,'update_plan',{plan_id:copy,name:'Copy renamed after confirmation'});await send(command,'delete_plan',frozen,'DELETE_CONFIRMATION_REQUIRED');
    frozen=consent(await snapshot(),copy);await send(command,'update_plan',{plan_id:copy,name:frozen.confirmation_name,description:'Updated after consent'});await send(command,'delete_plan',frozen,'CONTENT_REVISION_CONFLICT');
    await send(command,'delete_plan',{...consent(await snapshot(),copy),expected_version:0},'VERSION_CONFLICT');
    record('name / content / document version changes invalidate frozen delete consent');
    await send(command,'select',{plan_id,mission_id});const contextBefore=await api('/api/context');
    const deleteConsent=consent(await snapshot(),copy);await send(command,'delete_plan',deleteConsent);state=await snapshot();
    assert(!state.plans[copy] && state.plans[plan_id]);assert(state.deleted_plans[copy]);assert.deepEqual(await api('/api/context'),contextBefore);assert.deepEqual(await api('/api/video-view'),videoBefore);
    await send(command,'delete_plan',deleteConsent,'PLAN_NOT_FOUND');assert.deepEqual((await snapshot()).deleted_plans,state.deleted_plans);
    record('deleting unselected plan preserves other plan/context/video; duplicate delete is safe');

    const history=state.executions,deployments=state.deployments;
    await send(command,'delete_plan',consent(state,plan_id));state=await snapshot();assert(!state.plans[plan_id]);assert.deepEqual(state.executions,history);assert.deepEqual(state.deployments,deployments);
    const cleared=await api('/api/context');assert.equal(cleared.active_security_plan_id,null);assert.equal(cleared.active_mission_id,null);
    assert.equal(cleared.active_uav_id,contextBefore.active_uav_id);assert.deepEqual(cleared.selected_uav_ids,contextBefore.selected_uav_ids);
    const stale=await request('/api/context',{instance_id:command.id,patch:{active_security_plan_id:plan_id,active_mission_id:mission_id}},'PATCH');
    assert(stale.status>=400);assert.deepEqual(await api('/api/context'),cleared);assert.deepEqual(await api('/api/video-view'),videoBefore);
    record('current deletion clears only plan/task context; stale context patch cannot revive deleted IDs; history remains');
    fs.writeFileSync(path.join(out,'before-restart.json'),JSON.stringify(state,null,2));
    fs.writeFileSync(path.join(out,'result.json'),JSON.stringify({pass:true,kind:'isolated protocol and Mock only',passed,ids},null,2));
  }
}catch(error){
  if(out){fs.mkdirSync(out,{recursive:true});fs.writeFileSync(path.join(out,'result.json'),JSON.stringify({pass:false,passed,error:String(error.stack??error)},null,2));}throw error;
}finally{for(const client of clients){clearInterval(client.timer);client.socket.close();}}
