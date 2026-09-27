import fs from 'node:fs';
const base='http://127.0.0.1:20580';const request=JSON.parse(fs.readFileSync(process.argv[2],'utf8').replace(/^\uFEFF/,''));
const instance_id=crypto.randomUUID();
const api=async(path,body)=>{const r=await fetch(base+path,body?{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});return {status:r.status,data:await r.json()};};
await api('/api/context/clients',{instance_id,client_id:'FIX02-Supplementary-API',client_role:'Command',hostname:'ISOLATED-QA',app_version:'FIX02'});
const socket=new WebSocket('ws://127.0.0.1:20581/ws');await new Promise(r=>socket.onopen=r);const ready=new Promise(r=>socket.onmessage=r);socket.send(JSON.stringify({type:'subscribe_context',instance_id}));await ready;
try {const state=(await api('/api/security-plans')).data;const result=await api('/api/security-plans',{instance_id,expected_version:state.version,...request});
fs.writeFileSync(process.argv[3],JSON.stringify(result,null,2));console.log(JSON.stringify({status:result.status,code:result.data.code,params:result.data.params,plan_id:result.data.plan_id,mission_id:result.data.mission_id}));}finally{socket.close();}
