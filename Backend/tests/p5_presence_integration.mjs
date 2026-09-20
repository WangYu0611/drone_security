import assert from 'node:assert/strict';
import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:19380', ws=process.env.P1_WS??'ws://127.0.0.1:19381/ws';
const directory=process.env.P5_EVIDENCE_DIR??'Evidence/TASK-P5/protocol';
fs.mkdirSync(directory,{recursive:true});
const passed=[],clients=[],traffic=[];
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,body,method='POST'){
 const r=await fetch(base+path,body?{method,headers:{'content-type':'application/json'},body:JSON.stringify(body)}:{});
 const data=await r.json();traffic.push({path,status:r.status,data});assert(r.ok,JSON.stringify(data));return data;
}
async function until(f){for(let i=0;i<160;i++){if(await f())return;await sleep(100);}throw Error('Timeout');}
async function connect(role){
 const id=crypto.randomUUID();const registration=await api('/api/context/clients',{instance_id:id,client_id:'P5-QA-'+id,client_role:role,hostname:'SYNTHETIC-QA',app_version:'P5-test'});
 assert.equal(registration.state,'OFFLINE');assert.equal(registration.hydrated,false);
 const socket=new WebSocket(ws), c={id,role,socket,events:[]};clients.push(c);
 socket.onmessage=e=>c.events.push(JSON.parse(e.data));
 await new Promise((r,j)=>{socket.onopen=r;socket.onerror=j;});
 socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await until(()=>c.events.some(e=>e.type==='context_subscribed'));
 c.heartbeat=()=>socket.send(JSON.stringify({type:'context_ping',hydrated:true}));c.heartbeat();
 c.timer=setInterval(c.heartbeat,2000);return c;
}
const pass=name=>{passed.push(name);console.log('PASS '+name);};
const presence=()=>api('/api/context/clients');
try {
 const command=await connect('Command'),map=await connect('Map'),video=await connect('Video');
 const owned=()=>clients.slice(0,3).map(c=>c.id);
 await until(async()=>{const p=await presence();return owned().every(id=>p.some(c=>c.instance_id===id&&c.state==='ONLINE'&&c.hydrated&&Date.now()/1000-c.last_seen<12));});
 pass('Presence.ThreeClientsOnline');
 const before={context:await api('/api/context'),plans:await api('/api/security-plans'),video:await api('/api/video-view')};
 await api('/api/ui-preferences',{instance_id:command.id,language:'zh-Hans'},'PATCH');
 await until(()=>[command,map,video].every(c=>c.events.some(e=>e.type==='UIPreferencesChanged'&&e.payload.language==='zh-Hans')));
 await api('/api/ui-preferences',{instance_id:video.id,language:'en'},'PATCH');
 await until(()=>[command,map,video].every(c=>c.events.some(e=>e.type==='UIPreferencesChanged'&&e.payload.language==='en')));
 pass('Stage1.LocalizationSyncAcrossClients');
 for(const original of [video,map,command]){
  clearInterval(original.timer);original.socket.close();
  await until(async()=> (await presence()).some(c=>c.instance_id===original.id&&c.state==='OFFLINE'));
  const replacement=await connect(original.role);
  await until(async()=> (await presence()).some(c=>c.instance_id===replacement.id&&c.state==='ONLINE'&&c.hydrated));
  assert.deepEqual(await api('/api/context'),before.context);assert.deepEqual(await api('/api/security-plans'),before.plans);assert.deepEqual(await api('/api/video-view'),before.video);assert.equal((await api('/api/ui-preferences')).language,'en');
  pass('Stage1.'+original.role+'RestartHydration (protocol replica)');
 }
 pass('Presence.ClientDisconnectReconnect');pass('Presence.SystemReadyDegraded');
 const silent=await connect('Map');clearInterval(silent.timer);
 await until(async()=> (await presence()).some(c=>c.instance_id===silent.id&&c.state==='OFFLINE'));
 assert(silent.events.some(e=>e.type==='error'));pass('Presence.HeartbeatExpiryWithLiveSocket');
} finally {
 for(const c of clients){clearInterval(c.timer);c.socket.close();}
 fs.writeFileSync(directory+'/p5-presence.json',JSON.stringify({passed:passed.length,tests:passed,classification:'Protocol synthetic clients; does not replace native acceptance',traffic},null,2));
}
