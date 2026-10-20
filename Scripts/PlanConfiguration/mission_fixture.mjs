// Isolated test seed: real Backend role checks, route-save ACKs and deployment.
// No execution is started. This is protocol setup, not native UI evidence.
import assert from 'node:assert/strict';
const base=process.env.P1_HTTP,ws=process.env.P1_WS;
if(!base || !ws)throw Error('Explicit isolated P1_HTTP and P1_WS endpoints are required');
const clients=[];
async function api(path,body){
  const response=await fetch(base+path,body?{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});
  const value=await response.json();if(!response.ok)throw Error(JSON.stringify(value));return value;
}
async function connect(role){
  const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'MissionActions-'+role,client_role:role,hostname:'ISOLATED-AUTOMATION',app_version:'MissionActions'});
  const socket=new WebSocket(ws);clients.push(socket);
  await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
  const ready=new Promise((resolve,reject)=>{socket.onmessage=event=>{if(JSON.parse(event.data).type==='context_subscribed')resolve();};socket.onerror=reject;});
  socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await ready;return id;
}
try{
  const command=await connect('Command'),map=await connect('Map');
  const send=async(instance_id,action,extra={})=>api('/api/security-plans',{instance_id,action,expected_version:(await api('/api/security-plans')).version,...extra});
  const {plan_id}=await send(command,'create_plan',{name:'Mission actions deployed fixture',description:'Two independently selectable tasks'});
  const missions=[];
  for(let index=0;index<2;index++){
    const {mission_id}=await send(command,'add_mission',{plan_id,name:index?'South task':'North task'});missions.push(mission_id);
    await send(command,'assign',{plan_id,mission_id,assigned_uav_id:`UAV-0${index+1}`});
    const {edit_session_id}=await send(map,'begin_route_edit',{plan_id,mission_id,content_revision:(await api('/api/security-plans')).plans[plan_id].content_revision,workflow:true});
    const path={pathId:index+1,bClosedLoop:false,waypoints:[0,1,2].map(point=>({sequence:point+1,latitude:39.9807+index*.002+point*.0001,longitude:116.34703+index*.002,altitude:30,altitude_reference:'Ellipsoid',segmentSpeed:5,waitTime:0}))};
    await send(map,'save_route',{plan_id,mission_id,edit_session_id,path});
  }
  await send(command,'validate',{plan_id});await send(command,'review',{plan_id});
  const deployed=await send(command,'deploy',{plan_id});
  assert.equal(deployed.state.plans[plan_id].status,'DEPLOYED');
  assert.deepEqual(deployed.state.plans[plan_id].mission_ids,missions);
  assert.equal(Object.keys(deployed.state.executions).length,0);
  await send(command,'select',{plan_id,mission_id:missions[1]});
  console.log(JSON.stringify({plan_id,mission_ids:missions,deployment_id:deployed.state.plans[plan_id].deployment_id,executions:0}));
}finally{for(const socket of clients)socket.close();}
