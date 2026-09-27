// Synthetic, isolated Backend fixture. UI acceptance opens this saved plan; it does not use P55.QA actors.
const base=process.env.P1_HTTP??'http://127.0.0.1:20480',ws=process.env.P1_WS??'ws://127.0.0.1:20481/ws';
const api=async(path,body)=>{const r=await fetch(base+path,body?{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});const j=await r.json();if(!r.ok)throw Error(JSON.stringify(j));return j;};
const clients=[];
async function connect(role){const id=crypto.randomUUID();await api('/api/context/clients',{instance_id:id,client_id:'P55FIX-FIXTURE-'+role,client_role:role,hostname:'SYNTHETIC',app_version:'P55FIX'});const socket=new WebSocket(ws);await new Promise(r=>socket.onopen=r);const ready=new Promise(r=>socket.onmessage=r);socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await ready;clients.push(socket);return id;}
try{
const command=await connect('Command'),map=await connect('Map');
const send=async(instance_id,action,extra={})=>api('/api/security-plans',{instance_id,action,expected_version:(await api('/api/security-plans')).version,...extra});
const p=await send(command,'create_plan',{name:'P55 FIX Route V2 Lifecycle'}),plan_id=p.plan_id;let first='';
for(let m=0;m<3;m++){
 const {mission_id}=await send(command,'add_mission',{plan_id,name:'Route '+String.fromCharCode(65+m)});if(!first)first=mission_id;
 await send(command,'assign',{plan_id,mission_id,assigned_uav_id:'UAV-0'+(m+1)});
 const {edit_session_id}=await send(map,'begin_route_edit',{plan_id,mission_id,content_revision:(await api('/api/security-plans')).plans[plan_id].content_revision,workflow:true});
 const path={pathId:m+1,bClosedLoop:m===0,waypoints:[[0,0],[0,.0006],[.0005,.0006],[.0005,.0002],[.00025,0]].map(([lat,lon],i)=>({sequence:i+1,latitude:39.9807+m*.001+lat,longitude:116.34703+lon,altitude:30+[0,40,70,20,10][i]+m*15,altitude_reference:'Ellipsoid',segmentSpeed:[4,6,12,8,10][i],waitTime:0}))};
 await send(map,'save_route',{plan_id,mission_id,edit_session_id,path,keep_editing:true});await send(map,'finish_route_edit',{plan_id,mission_id,edit_session_id});
}
await send(command,'select',{plan_id,mission_id:first});console.log(JSON.stringify({plan_id,mission_id:first}));
}finally{for(const c of clients)c.close();}
