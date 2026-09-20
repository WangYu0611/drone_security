import assert from 'node:assert/strict';import fs from 'node:fs';
const before=JSON.parse(fs.readFileSync('Evidence/TASK-P4/protocol/restart-before.json','utf8'));const after={};
for(const [key,path] of Object.entries({plans:'security-plans',context:'context',language:'ui-preferences',video:'video-view'}))after[key]=await (await fetch('http://127.0.0.1:19280/api/'+path)).json();
fs.writeFileSync('Evidence/TASK-P4/protocol/restart-after.json',JSON.stringify(after,null,2));assert.deepEqual(after,before);console.log('PASS exact Backend restart recovery: plans, deployments, language, video, selection');
