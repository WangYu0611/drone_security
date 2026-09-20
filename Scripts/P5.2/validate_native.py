"""Validate read-only native observations; never counts as UI interaction."""
import gzip,json,sys,urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=root/'Evidence/TASK-P5.2/native'
if '--offline' in sys.argv:state=json.loads((out/'final-state.json').read_text(encoding='utf-8'))
else:
    with urllib.request.urlopen('http://127.0.0.1:19780/api/security-plans') as r:state=json.load(r)
(out/'final-state.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
observations=out/'observations.jsonl'
raw=observations.read_text(encoding='utf-8') if observations.exists() else gzip.decompress((out/'observations.jsonl.gz').read_bytes()).decode('utf-8')
rows=[json.loads(x) for x in raw.splitlines() if x.strip()]
active={'CREATED','PREFLIGHT','STARTING','EXECUTING','PAUSED','RETURNING'}
routes={};paused={};aborted={};moving={}
for row in rows:
    seen=set()
    for key,e in row['executions'].items():
        if e['state'] in active:
            assert e['uav_id'] not in seen,('conflict',row['observed_at']);seen.add(e['uav_id'])
        signature=json.dumps([e['route_snapshot'],e['route_revision']],sort_keys=True)
        assert routes.setdefault(key,signature)==signature,('route mutated',key)
        if e['state']=='EXECUTING':moving.setdefault(key,set()).add(json.dumps(e['position'],sort_keys=True))
        if e['state']=='PAUSED':paused.setdefault(key,[]).append((row['observed_at'],e['position'],e['progress']))
        if e['state']=='ABORTED':aborted.setdefault(key,[]).append(e['position'])
for key,values in paused.items():
    for a,b in zip(values,values[1:]):
        if b[0]-a[0]<3:assert a[1:]==b[1:],('pause moved',key)
for key,values in aborted.items():assert all(v==values[0] for v in values),('abort moved',key)
completed=[e for e in state['executions'].values() if e['completion_reason']=='ROUTE_FINISHED']
returned=[e for e in state['executions'].values() if e['completion_reason']=='RETURNED_HOME']
assert completed and returned and aborted and paused and any(len(v)>10 for v in moving.values())
for e in completed:assert e['progress']==1 and e['completed_waypoints']==e['total_waypoints']
for e in returned:assert e['position']==e['home_position'] and any(e['home_position'][k]!=e['route_snapshot']['waypoints'][0][k] for k in ('latitude','longitude','altitude'))
timeline=[e for e in state['events'] if 'execution_snapshot' in e]
(out/'execution-timeline.json').write_text(json.dumps(timeline,indent=2),encoding='utf-8')
for complete_id in ('execution-18','execution-22'):
    complete_timeline=[e for e in timeline if e['target_id']==complete_id]
    assert [e['event_type'] for e in complete_timeline]==['EXECUTION_CREATED','PREFLIGHT_STARTED','PREFLIGHT_PASSED','MISSION_STARTING','MISSION_STARTED',*(['WAYPOINT_REACHED']*4),'MISSION_COMPLETED']
    (out/f'uninterrupted-{complete_id}.json').write_text(json.dumps(complete_timeline,indent=2),encoding='utf-8')
before=json.loads((out/'backend-restart-before.json').read_text(encoding='utf-8-sig'))
after=json.loads((out/'backend-restart-after.json').read_text(encoding='utf-8-sig'))
for key,e in before['executions'].items():
    recovered=after['executions'][key]
    assert recovered['route_snapshot']==e['route_snapshot'] and recovered['deployment_id']==e['deployment_id']
    assert recovered['elapsed_seconds']>=e['elapsed_seconds']
summary={'native_read_only_oracle':True,'observations':len(rows),'immutable_routes':len(routes),'moving_executions':{k:len(v) for k,v in moving.items()},'paused_observations':{k:len(v) for k,v in paused.items()},'aborted_observations':{k:len(v) for k,v in aborted.items()},'completed':[e['execution_id'] for e in completed],'returned_home':[e['execution_id'] for e in returned],'one_active_per_uav':True}
(out/'validation.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary))
