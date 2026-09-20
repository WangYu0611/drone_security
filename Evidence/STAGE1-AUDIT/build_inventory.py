"""Read-only source inventory; writes only this audit directory and Docs/Stage1 appendices.
Does not import product code, start services, contact devices, or alter business data.
"""
from pathlib import Path
import collections, hashlib, json, re, subprocess, xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Evidence/STAGE1-AUDIT'
DOC = ROOT / 'Docs/Stage1'
OUT.mkdir(parents=True, exist_ok=True)
DOC.mkdir(parents=True, exist_ok=True)
def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args]).decode('utf-8', errors='replace')
def read(path):
    return (ROOT/path).read_text(encoding='utf-8-sig', errors='replace')
def redact(s):
    s = re.sub(r'\b(?:192\.168|10\.\d+)\.\d+\.\d+\b', '<DEVICE_IP>', s)
    s = re.sub(r'(?i)((?:access[_-]?token|api[_-]?key|secret|password)\s*[=:]\s*)[^\s,;]+', r'\1<REDACTED>', s)
    s = re.sub(r'eyJ[A-Za-z0-9_-]{15,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+', '<REDACTED_TOKEN>', s)
    return s
def write(name, value):
    (OUT/name).write_text(redact(value), encoding='utf-8')
files = git('ls-files', '-z').split('\0')
files = [p for p in files if p and not p.startswith(('Docs/Stage1/', 'Evidence/STAGE1-AUDIT/'))]
write('tracked-inventory.txt', '\n'.join(files)+'\n')
write('baseline.txt', 'Audit date: 2026-09-20\nBranch: '+git('branch','--show-current')+'HEAD: '+git('rev-parse','HEAD')+'Initial status was clean, checked before creating audit outputs.\nNo service/UE/aircraft launched.\n')
write('history.txt', git('log','--all','--format=%h %ad %s','--date=short','--','Jetson','Backend/communication','ue_px4_msgs','Source/UE5DroneControl/RealTimeDroneReceiver.cpp')+'\nDeleted UI evidence:\n'+git('show','8941c55^:Source/UE5DroneControl/UI/LocalPreviewIsolationToggleWidget.h'))
old = git('show','eb44db4:logs/backend.log')
samples=[]
for pattern in [r'\[UdpReceiver\].*recv',r'GPS Anchor',r'\[StateMachine\]',r'\[UdpSender\].*mode=move',r'\[CommandQueue\].*ack']:
    hits=[(i,l) for i,l in enumerate(old.splitlines(),1) if re.search(pattern,l,re.I)]
    samples.append(f'Pattern {pattern}; matches={len(hits)}; first samples:\n'+'\n'.join(f'eb44db4:logs/backend.log:{i}: {l}' for i,l in hits[:4]))
write('historical-telemetry.txt','\n\n'.join(samples)+'\nHistorical transport evidence; sender hardware identity and completed flight are not independently certified.\n')
patterns={
 'real-uav-scan':r'PX4|MAVLink|MAVSDK|MAVROS|QGroundControl|Telemetry|Heartbeat|CommandLong|Takeoff|Disarm|VehicleCommand|TrajectorySetpoint|Serial|RTSP|WebRTC',
 'coordinate-scan':r'GeographicToWorld|WorldToGeographic|Wgs84ToGcj02|Gcj02ToWgs84|ECEF|\bENU\b|\bNED\b|BD-09|GeoidSeparation|gps_to_enu|enu_to_ned',
 'persistence-scan':r'SaveGame|SaveGameToSlot|LoadGameFromSlot|\.json|\.yaml|sqlite|storage_path|SaveStringToFile',
 'localization-candidates':r'FText::FromString\(TEXT\(|SetText\(TEXT\(|LOCTEXT|NSLOCTEXT|ProductText::',
 'blueprint-references':r'ConstructorHelpers|LoadClass|LoadObject|/Game/|Blueprint',
}
source_files=[p for p in files if (p.startswith(('Source/','Backend/','Jetson/','ue_px4_msgs/','Launcher/','Scripts/','Config/','tools/')) or '/' not in p) and Path(p).suffix in {'.cpp','.h','.inl','.py','.pyw','.cs','.ini','.yaml','.yml','.json'} and '/deps/' not in p]
for name,pat in patterns.items():
    rx=re.compile(pat,re.I if name=='real-uav-scan' else 0)
    hits=[]
    for p in source_files:
        for i,line in enumerate(read(p).splitlines(),1):
            if rx.search(line): hits.append(f'{p}:{i}: {line.strip()}')
    write(name+'.txt','\n'.join(hits)+'\n')
# Extract exact HTTP dispatcher and full implementations with numbered source lines.
http=read('Backend/http/http_server.cpp')
hs=http.splitlines()
write('http-source.txt','\n'.join(f'{i}: {s}' for i,s in enumerate(hs,1))+'\n')
routes=[]
for i,line in enumerate(hs,1):
    m=re.search(r'method\s*==\s*"(GET|POST|PUT|PATCH|DELETE)"\s*&&\s*path\s*==\s*"([^"]+)"',line)
    if m: routes.append({'method':m[1],'path':m[2],'line':i})
    m=re.search(r'method\s*==\s*"(GET|POST|PUT|PATCH|DELETE)"\s*&&\s*PathMatch\(path,\s*"([^"]+)",\s*"([^"]*)"',line)
    if m: routes.append({'method':m[1],'path':m[2]+'{id}'+m[3],'line':i})
for path in ['/api/ui-preferences','/api/video-view']:
    for method in ['GET','PATCH']: routes.append({'method':method,'path':path,'line':415})
routes.sort(key=lambda r:(r['line'],r['path'],r['method']))
write('http-routes.json',json.dumps(routes,ensure_ascii=False,indent=2)+'\n')
# These are actual wire types, not domain event_type values or hypothetical uppercase types.
server_types=['telemetry','event','alert','assembling','assembly_complete','assembly_timeout','assignment_result','drone_task_state','command_ack','error','OperationalContextChanged','client_connected','client_disconnected','context_subscribed','context_pong','SecurityPlansChanged','UIPreferencesChanged','VideoViewChanged','MapRouteEditRequested']
client_types=['subscribe_context','context_ping','pause','resume']
write('ws-types.json',json.dumps({'server_to_client':server_types,'client_to_server':client_types,'rejected_type':'move (use mode instead)','mode_discriminator':['move','scout','patrol','attack']},indent=2))
state=json.loads(read('Evidence/TASK-P5.2/native/final-state.json'))
examples={key:next(iter(state[key].values())) for key in ['plans','missions','paths','deployments','executions']}
examples['events']=next(e for e in state['events'] if 'execution_snapshot' in e)
for key,value in examples.items(): write('example-'+key+'.json',json.dumps(value,ensure_ascii=False,indent=2)+'\n')
# Full source-declared UE structs, rather than inferred telemetry defaults.
structtext=read('Source/UE5DroneControl/DroneOps/Core/DroneOpsTypes.h')
selected=[]
for name in ['FDroneDescriptor','FDroneTelemetrySnapshot','FDroneTaskStateSnapshot']:
    m=re.search(r'struct '+name+r'\s*\{.*?\n\};',structtext,re.S)
    selected.append(m.group(0))
write('registry-structs.txt','\n\n'.join(selected))
schema=['# Stage 1 字段清单附录','',
 '本附录逐字段列出已保存原生样本的字段、观测类型、单位、语义与来源；不是自动推导的验证 Schema。Required 区分源码输出构造与请求必填，不能把样本存在当成请求必填。嵌套 snapshot 引用同名业务对象，route_snapshot 引用 Route。null 是允许观察到的值，不代表字段可省略。输入约束见主目录。', '']
def typ(v):
    return 'null' if v is None else 'boolean' if isinstance(v,bool) else 'integer' if isinstance(v,int) else 'number' if isinstance(v,float) else 'string' if isinstance(v,str) else 'object' if isinstance(v,dict) else 'array'
def unit(k):
    k=k.split('.')[-1]
    if k in ['latitude','longitude']:return 'degree'
    if k in ['altitude','distance_m','distance_travelled_m']:return 'm (altitude 基准见正文)'
    if k in ['segmentSpeed','default_speed_mps']:return 'm/s'
    if k in ['progress','overall_progress','segment_progress']:return '0..1'
    if k.endswith('_at') or k=='timestamp':return 'Unix s'
    if k.endswith('_seconds') or k in ['waitTime','wait_remaining']:return 's'
    if k in ['x','y','z']:return 'UE cm'
    return '—'
meanings={'id':'对象标识','plan_id':'所属方案','mission_id':'所属任务','route_id':'航线标识','execution_id':'执行标识','deployment_id':'部署标识','assigned_uav_id':'任务分配 UAV；不等于当前选择','uav_id':'执行 UAV','status':'配置状态','state':'执行/编辑状态','name':'名称','description':'描述','content_revision':'配置内容修订','version':'事务/对象版本','revision':'航线修订','control_version':'控制并发版本','mission_ids':'任务成员标识','review':'人工复核记录','validation':'检查结果','workflow_step':'流程页面阶段','workflow_mission_id':'流程关联任务','created_at':'创建时间','updated_at':'最后更新时间','started_at':'开始时间','completed_at':'终止时间','deployed_at':'部署时间','reviewed_at':'复核时间','reviewed_by':'复核客户端','deployed_by':'部署客户端','source_plan_id':'复制来源方案','source_deployment_id':'复制来源部署','deployment':'复制时清空的兼容字段','snapshot':'部署冻结对象','route_snapshot':'执行冻结航线','route_revision':'冻结航线修订','pathId':'旧 Path 数字标识','bClosedLoop':'旧航线闭环标记；Mock 未据此循环','waypoints':'有序航点','sequence':'顺序号','latitude':'纬度','longitude':'经度','altitude':'地理高度','segmentSpeed':'抵达该点航段速度；0 使用 Mock 默认','waitTime':'抵达等待时间','location':'UE 世界坐标缓存','position':'当前地理位置','segment_start':'当前航段起点','home_position':'独立模拟 Home','current_waypoint':'当前航点，一基','completed_waypoints':'已完成点数','total_waypoints':'总点数','progress':'按距离进度','overall_progress':'总体进度','segment_progress':'当前航段进度','default_speed_mps':'默认速度','distance_m':'初始总距离','distance_travelled_m':'累计距离','estimated_seconds':'预计时长','elapsed_seconds':'积分执行时长，不等于墙钟差','wait_remaining':'剩余等待','completion_reason':'完成原因','failure_reason':'失败原因','simulation':'模拟身份','plan_name':'创建执行时方案名','mission_name':'创建执行时任务名','resume_state':'统一暂停前状态','timestamp':'事件时间','event_type':'业务事件名','params':'本地化及关联参数','category':'事件分类','target_id':'事件目标，执行事件是 execution_id','message':'后端文字','source':'来源','execution_snapshot':'事件时执行状态投影'}
for key,label,source in [('plans','SecurityPlan','Backend/storage/security_plan_store.h'),('missions','Mission','Backend/storage/security_plan_store.h'),('paths','Route','Backend/storage/security_plan_store.h'),('deployments','Deployment','Backend/storage/security_plan_store.h'),('executions','Execution','Backend/storage/mock_execution.inl'),('events','Event','Backend/storage/security_plan_store.h + mock_execution.inl')]:
    vals=list(state[key].values()) if isinstance(state[key],dict) else state[key]
    fields=collections.defaultdict(set)
    for v in vals:
        for f,x in v.items():fields[f].add(typ(x))
    schema += [f'## {label}', '', f'Source: `{source}`；样本 `Evidence/TASK-P5.2/native/final-state.json`。', '', '| Field | Type | Required | Unit | Meaning | Persistent | Versioned |','|---|---|---|---|---|---|---|']
    conditional={'plans':{'validation','workflow_step','workflow_mission_id','deployment_id','deployment','source_plan_id','source_deployment_id'},'executions':{'resume_state'},'events':{'execution_snapshot'},'paths':{'pathId','bClosedLoop'}}
    for f,types in fields.items():
        required='条件/兼容字段' if f in conditional.get(key,set()) else '输出构造恒有（非请求必填）'
        schema.append(f'| `{f}` | {" / ".join(sorted(types))} | {required} | {unit(f)} | {meanings.get(f,"见同名源码字段")} | 是 | '+('execution_version' if key=='executions' else 'document version；Execution 事件随 execution_version' if key=='events' else 'document version')+' |')
    schema.append('')
wp=next(iter(state['paths'].values()))['waypoints'][0]
schema+=['## Waypoint','','Source: `Backend/storage/security_plan_store.h::checkPath`、`MapMissionRouteWidget.cpp::Save`。','','| Field | Type | Required | Unit | Meaning | Persistent | Versioned |','|---|---|---|---|---|---|---|']
for f,v in wp.items():schema.append(f'| `{f}` | {typ(v)} | '+('可选兼容字段' if f=='location' else '请求必需，checkPath检查')+f' | {unit(f)} | {meanings[f]} | 随 Route | Route revision |')
schema += ['', '## 嵌套对象字段', '', '| Object.Field | Type | Required | Unit / Meaning | Source | Persistent / Versioned |', '|---|---|---|---|---|---|']
nested=[('review',{'content_revision':'integer','reviewed_at':'number','reviewed_by':'string'},'SecurityPlanStore::transact(review)'),('validation',{'ready':'boolean','issues':'array','mission_count':'integer','assigned_count':'integer','routes_ready':'integer','validated_content_revision':'integer/null'},'SecurityPlanStore::validate/transact'),('validation.issues[]',{'mission_id':'string','code':'string','params':'object'},'SecurityPlanStore::validate'),('edit_sessions[]',{'edit_session_id':'string','plan_id':'string','mission_id':'string','owner_instance_id':'string','owner_client_id':'string','base_content_revision':'integer','state':'string','started_at':'number','last_seen_at':'number','lease_expires_at':'number'},'SecurityPlanStore::transact(begin_route_edit)'),('execution_requests[]',{'request':'object','execution_id':'string'},'mock_execution.inl::executionRequest'),('position / home_position / segment_start',{'latitude':'number','longitude':'number','altitude':'number'},'mock_execution.inl::position'),('Waypoint.location',{'x':'number','y':'number','z':'number'},'MapMissionRouteWidget route serialization'),('execution_snapshot',{'state':'string','position':'object','progress':'number','current_waypoint':'integer'},'mock_execution.inl::executionEvent')]
for obj,fields,src in nested:
    for f,t in fields.items():schema.append(f'| {obj}.{f} | {t} | 该条件对象存在时构造；location仅UE兼容字段 | {unit(f)} / {meanings.get(f,"同名业务属性，见源码")} | `{src}` | 随宿主文档/版本 |')
schema += ['', '## UE Registry 逐字段', '', 'Source: `Source/UE5DroneControl/DroneOps/Core/DroneOpsTypes.h`。完整声明另存 registry-structs.txt。C++成员不是网络必填；运行时值的有效性由availability/valid标志决定。', '']
for text in selected:
    name=re.search(r'struct (\w+)',text)[1]
    schema += ['### '+name,'','| Field | Type | Required | Unit / Meaning | Persistent | Versioned |','|---|---|---|---|---|---|']
    for line in text.splitlines():
        m=re.search(r'(FString|int32|float|double|bool|FVector|FRotator|FLinearColor|EDrone\w+|EUELocalDroneState)\s+(\w+)\s*(?:=|;)',line)
        if not m:continue
        t,f=m.groups()
        u=('UE cm' if f in ['WorldLocation','EnemyInitialLocation'] else 'NED m' if f=='NedLocation' else 'Lat,Lon,Alt (degree/degree/m)' if f=='GeographicLocation' else 'degree' if f in ['GpsLatitude','GpsLongitude','Attitude'] else '% (-1 unknown)' if f in ['Battery','BatteryPercent'] else 'm AMSL' if f=='GpsAltitude' else '本地运行字段；由网络/Registry赋值')
        schema.append(f'| {f} | {t} | C++声明成员；非网络必填 | {u} | 描述符同名字段可来自drones.json，其余本地内存 | 无独立版本 |')
(DOC/'Stage1-Schema-Fields.md').write_text('\n'.join(schema)+'\n',encoding='utf-8')
domain_text=read('Backend/storage/security_plan_store.h')+'\n'+read('Backend/storage/mock_execution.inl')
event_names=sorted(set(re.findall(r'"((?:PLAN|MISSION|UAV|ROUTE|DEPLOYMENT|EXECUTION|PREFLIGHT|WAYPOINT)_[A-Z_]+)"',domain_text)))
write('domain-event-and-error-symbols.txt','Source string symbols (includes error codes; not all are emitted events):\n'+'\n'.join(event_names)+'\n')
# Saved regression evidence counts only, no test execution.
xml=ET.fromstring(read('Evidence/TASK-P5.2/backend/results.xml'))
tests=xml.findall('.//testcase')
summary={'audit_head':git('rev-parse','HEAD').strip(),'tracked_files':len(files),'scanned_source_config_files':len(source_files),'http_endpoints':len(routes),'http_options':'wildcard, excluded','ws_server_types':len(server_types),'ws_client_types':len(client_types),'ws_unique_accepted_type_values':len(set(server_types+client_types)),'persistent_business_schemas':7,'coordinate_interfaces':12,'video_interfaces':5,'real_uav_interface_groups':5,'saved_backend_tests':len(tests),'saved_backend_failures':[t.attrib.get('classname','')+'.'+t.attrib['name'] for t in tests if t.find('failure') is not None],'new_runtime_tests':0}
write('counts.json',json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
write('source-sha256.json',json.dumps({p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in source_files},indent=2))
print(json.dumps(summary,ensure_ascii=False))
