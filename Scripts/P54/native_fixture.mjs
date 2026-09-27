import fs from 'node:fs';
const base='http://127.0.0.1:19980',ws='ws://127.0.0.1:19981/ws';
const clients=[];
async function api(path,body){const response=await fetch(base+path,body?{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});const json=await response.json();if(!response.ok)throw Error(JSON.stringify(json));return json;}
async function client(role){const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'P54-NATIVE-FIXTURE-'+role,client_role:role,hostname:'SYNTHETIC-FIXTURE',app_version:'P54'});const socket=new WebSocket(ws);clients.push(socket);await new Promise(r=>socket.onopen=r);socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await new Promise(r=>socket.onmessage=r);return id;}
try{
 const command=await client('Command'),map=await client('Map');let plan_id='',mission_id='';
 const send=async(instance_id,action,extra={})=>api('/api/security-plans',{instance_id,action,plan_id,mission_id,expected_version:(await api('/api/security-plans')).version,...extra});
 let result=await send(command,'create_plan',{name:'P5.4 Native 3D Review',default_mission_name:'Native waypoint review'});plan_id=result.plan_id;mission_id=result.mission_id;
 await send(command,'assign',{assigned_uav_id:'UAV-01'});const session=await send(map,'begin_route_edit',{content_revision:result.state.plans[plan_id].content_revision+1});
 const path={pathId:1,bClosedLoop:false,waypoints:[0,1,2,3].map((i)=>({sequence:i+1,latitude:39.9808+(i>=2?.0003:0),longitude:116.34703+(i===1||i===2?.0003:0),altitude:[40,80,120,60][i],altitude_reference:'Ellipsoid',segmentSpeed:i?8:0,waitTime:[0,5,10,0][i]}))};
 await send(map,'save_route',{edit_session_id:session.edit_session_id,path,keep_editing:true});await send(map,'finish_route_edit',{edit_session_id:session.edit_session_id});await send(command,'select');
 fs.writeFileSync('Evidence/TASK-P5.4/native/fixture.json',JSON.stringify({synthetic_setup:true,plan_id,mission_id,path},null,2));console.log('Native fixture prepared through protocol; this is not native creation evidence.');
}finally{for(const socket of clients)socket.close();}
