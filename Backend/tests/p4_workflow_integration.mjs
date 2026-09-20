import assert from 'node:assert/strict';import fs from 'node:fs';
const base=process.env.P1_HTTP??'http://127.0.0.1:19280',ws=process.env.P1_WS??'ws://127.0.0.1:19281/ws';
const dir=process.env.P4_EVIDENCE_DIR??'Evidence/TASK-P4/protocol';fs.mkdirSync(dir,{recursive:true});const traffic=[],events=[],passed=[];const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,method='GET',body){const response=await fetch(base+path,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined});const data=await response.json();traffic.push({path,method,body,status:response.status,data});return {status:response.status,data};}
const until=async f=>{for(let i=0;i<160;i++){if(f())return;await sleep(50);}throw Error('broadcast timeout');};
function pass(name){passed.push(name);console.log('PASS '+name);}
const clients=[];
async function client(role){const id=crypto.randomUUID();assert.equal((await api('/api/context/clients','POST',{instance_id:id,client_id:'P4-QA-'+role,client_role:role,hostname:'SYNTHETIC-QA',app_version:'P4'})).status,200);
 const c={id,role,events:[],socket:new WebSocket(ws)};clients.push(c);c.socket.onmessage=e=>{const m=JSON.parse(e.data);c.events.push(m);events.push({role,message:m});if(m.type==='context_subscribed')c.ready=true;};await new Promise((r,j)=>{c.socket.onopen=r;c.socket.onerror=j;});c.socket.send(JSON.stringify({type:'subscribe_context',instance_id:id}));await until(()=>c.ready);c.timer=setInterval(()=>{if(c.socket.readyState===1)c.socket.send(JSON.stringify({type:'context_ping'}));},3000);return c;}
try {
 const command=await client('Command'),map=await client('Map'),video=await client('Video');
 let plan='',mission='',session='';
 const snapshot=async()=> (await api('/api/security-plans')).data;
 const send=async(c,action,body={})=>api('/api/security-plans','POST',{instance_id:c.id,action,plan_id:plan,mission_id:mission,expected_version:(await snapshot()).version,...body});
 let r=await send(command,'create_plan',{name:'P4 原名',description:'Configuration only'});assert.equal(r.status,200);plan=r.data.plan_id;assert.equal(r.data.state.plans[plan].content_revision,1);pass('Command creates plan revision 1');
 for(const action of ['create_plan','update_plan','add_mission','rename_mission','delete_mission','assign','review','deploy','copy_plan'])assert.equal((await send(map,action,{name:'Forbidden'})).status,403);pass('Map business mutations forbidden');
 for(const action of ['begin_route_edit','mark_route_dirty','save_route','discard_route_edit'])assert.equal((await send(command,action)).status,403);pass('Command route mutations forbidden');
 for(const action of ['create_plan','update_plan','add_mission','rename_mission','delete_mission','assign','validate','review','deploy','copy_plan','select','begin_route_edit','mark_route_dirty','save_route','discard_route_edit','request_map_route_edit'])assert.equal((await send(video,action)).status,403);pass('Video all plan actions forbidden');
 r=await send(command,'add_mission',{name:'East Perimeter'});assert.equal(r.status,200);mission=r.data.mission_id;
 const second=(await send(command,'add_mission',{name:'Temporary'})).data.mission_id;
 assert.equal((await send(command,'rename_mission',{name:'Retained original input'})).status,200);
 assert.equal((await send(command,'assign',{assigned_uav_id:'UAV-01'})).status,200);
 assert.equal((await send(command,'assign',{mission_id:second,assigned_uav_id:'UAV-01'})).data.code,'UAV_ALREADY_ASSIGNED');
 assert.equal((await send(command,'assign',{assigned_uav_id:null})).status,200);assert.equal((await send(command,'assign',{assigned_uav_id:'UAV-01'})).status,200);
 assert.equal((await send(command,'delete_mission',{mission_id:second})).status,200);pass('Mission CRUD assignment unassignment duplicate rejection');
 const view=async(c,id)=>api('/api/video-view','PATCH',{instance_id:c.id,video_target_uav_id:id});
 assert.equal((await view(command,'UAV-01')).status,200);let vv=(await api('/api/video-view')).data.video_view_version;
 assert.equal((await view(video,'UAV-01')).data.video_view_version,vv);assert.equal((await view(map,'UAV-99')).data.code,'INVALID_VIDEO_TARGET');pass('Video domain validation and same-value no-op');
 assert.equal((await api('/api/context','PATCH',{instance_id:command.id,patch:{active_uav_id:'UAV-02'}})).status,200);
 assert.equal((await send(command,'select')).status,200);assert.equal((await api('/api/video-view')).data.video_target_uav_id,'UAV-01');pass('Active UAV and mission selection do not change video target');
 await view(video,'UAV-02');assert.equal((await api('/api/context')).data.active_uav_id,'UAV-02');await view(video,'UAV-01');assert.equal((await api('/api/context')).data.active_uav_id,'UAV-02');pass('Explicit feed selection changes video only');
 r=await send(command,'request_map_route_edit');assert.equal(r.status,200);await until(()=>map.events.some(e=>e.type==='MapRouteEditRequested'&&e.payload.request_id===r.data.request.request_id));pass('Explicit Map edit request delivered');
 r=await send(map,'begin_route_edit',{content_revision:(await snapshot()).plans[plan].content_revision});assert.equal(r.status,200);session=r.data.edit_session_id;
 await send(map,'mark_route_dirty',{edit_session_id:session});
 for(const action of ['review','deploy'])assert.equal((await send(command,action)).data.code,'PLAN_EDIT_IN_PROGRESS');pass('Dirty session blocks review and deployment');
 const path={pathId:1,bClosedLoop:false,waypoints:[0,1,2].map(i=>({sequence:i+1,latitude:39.98+i*.001,longitude:116.34+i*.001,altitude:60,segmentSpeed:i?5:0,waitTime:2,location:{x:i*100,y:0,z:60}}))};
 let before=await snapshot();r=await send(map,'save_route',{edit_session_id:session,path,expected_version:0});assert.equal(r.data.code,'VERSION_CONFLICT');assert.deepEqual((await snapshot()).paths,before.paths);assert.equal((await snapshot()).edit_sessions[session].state,'DIRTY');pass('Save failure retains dirty session and authoritative route');
 const rev=before.plans[plan].content_revision;r=await send(map,'save_route',{edit_session_id:session,path});assert.equal(r.status,200);assert.equal(r.data.state.plans[plan].content_revision,rev+1);assert(!r.data.state.edit_sessions[session]);pass('Save ACK increments content revision and closes session');
 await send(command,'validate');let current=await snapshot();assert.equal(current.plans[plan].status,'READY');assert.equal(current.plans[plan].validation.validated_content_revision,current.plans[plan].content_revision);
 assert.equal((await send(command,'deploy')).data.code,'PLAN_REVIEW_REQUIRED');await send(command,'review');current=await snapshot();assert.equal(current.plans[plan].review.content_revision,current.plans[plan].content_revision);pass('Validation and review separate and revision bound');
 const context=(await api('/api/context')).data;const lang=async(c,language)=>api('/api/ui-preferences','PATCH',{instance_id:c.id,language});
 r=await lang(command,'zh-Hans');assert.equal(r.status,200);const lv=r.data.ui_preferences_version;await until(()=>clients.every(c=>c.events.some(e=>e.type==='UIPreferencesChanged'&&e.payload.language==='zh-Hans')));
 assert.equal((await lang(map,'zh-Hans')).data.ui_preferences_version,lv);assert.equal((await lang(video,'xx')).data.code,'INVALID_LANGUAGE');
 await lang(video,'en');await until(()=>clients.every(c=>c.events.some(e=>e.type==='UIPreferencesChanged'&&e.payload.language==='en')));
 assert.deepEqual((await snapshot()).plans,current.plans);assert.deepEqual((await api('/api/context')).data,context);assert.equal((await api('/api/video-view')).data.video_target_uav_id,'UAV-01');pass('Three-client language convergence no-op and business isolation');
 await send(command,'update_plan',{name:'Revised 原名',description:'Changed after review'});current=await snapshot();assert.equal(current.plans[plan].status,'DRAFT');assert.equal(current.plans[plan].review,null);pass('Content change invalidates review');
 await send(command,'validate');await send(command,'review');
 r=await send(map,'begin_route_edit',{content_revision:(await snapshot()).plans[plan].content_revision});session=r.data.edit_session_id;clearInterval(map.timer);map.socket.close();await sleep(800);
 assert.equal((await send(command,'request_map_route_edit')).data.code,'MAP_CLIENT_UNAVAILABLE');pass('Offline Map edit request rejected');
 await sleep(21500);assert(!(await snapshot()).edit_sessions[session]);pass('Finite lease automatically expires without owner heartbeat');
 r=await send(command,'deploy');assert.equal(r.status,200);current=r.data.state;assert.equal(current.plans[plan].status,'DEPLOYED');const deployment=current.plans[plan].deployment_id;assert(current.deployments[deployment]);assert.equal(current.missions[mission].status,'DEPLOYED');
 assert.equal((await send(command,'deploy')).data.code,'PLAN_DEPLOYED');assert.equal(Object.keys((await snapshot()).deployments).length,Object.keys(current.deployments).length);pass('Deployment atomic snapshot and repeated deployment rejection');
 r=await send(command,'copy_plan');assert.equal(r.status,200);const copy=r.data.plan_id;assert.notEqual(copy,plan);assert.equal(r.data.state.plans[copy].content_revision,1);assert.notEqual(r.data.state.plans[copy].mission_ids[0],mission);
 await send(command,'update_plan',{plan_id:copy,name:'Independent copy'});assert.deepEqual((await snapshot()).deployments,current.deployments);pass('Copy uses new identities and preserves deployed snapshot');
 const reconnect=await client('Video');assert.equal((await api('/api/video-view')).data.video_target_uav_id,'UAV-01');assert.equal((await api('/api/ui-preferences')).data.language,'en');pass('New client hydrates latest independent views');
 const persisted={plans:await snapshot(),context:(await api('/api/context')).data,language:(await api('/api/ui-preferences')).data,video:(await api('/api/video-view')).data};fs.writeFileSync(dir+'/restart-before.json',JSON.stringify(persisted,null,2));
 assert.equal(traffic.filter(t=>t.path==='/api/arrays').length,0);pass('Protocol arrays requests zero');
}finally{
 for(const c of clients){clearInterval(c.timer);c.socket.close();}
 fs.writeFileSync(dir+'/p4-http.json',JSON.stringify(traffic,null,2));fs.writeFileSync(dir+'/p4-ws.json',JSON.stringify(events,null,2));fs.writeFileSync(dir+'/p4-results.json',JSON.stringify({passed:passed.length,tests:passed,note:'Real Backend HTTP/WS; synthetic clients, not native UI acceptance'},null,2));
}
