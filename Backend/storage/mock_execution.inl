// Included inside SecurityPlanStore: Execution shares its mutex, atomic document
// and event log. No flight-controller/legacy execution-engine calls are made.
public:
    void configureMock(const Object& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_ = config;
        if (!mock_.contains("uavs")) mock_["uavs"] = Object{};
        const double speed = mock_.contains("default_speed_mps") ? mock_.at("default_speed_mps").to_number<double>() : 8.;
        if (!std::isfinite(speed) || speed <= 0) throw std::runtime_error("Invalid MockDefaultSpeed");
        mock_["default_speed_mps"] = speed;
        state_["mock_execution"] = mock_;
    }
    static bool executionActive(const Object& e) {
        const auto s=str(e,"state");
        return s=="CREATED" || s=="PREFLIGHT" || s=="STARTING" || s=="EXECUTING" || s=="PAUSED" || s=="RETURNING";
    }
    void tickExecutions(double dt,const std::set<std::string>& uavs,const std::function<void(const Object&)>& publish) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!std::isfinite(dt) || dt<=0) return;
        auto next=state_; bool changed=false;
        for(auto& entry:next.at("executions").as_object()) {
            auto& e=entry.value().as_object(); if(!executionActive(e))continue;
            if(str(e,"state")=="PAUSED")continue;
            changed=true;
            try {
                if(!uavs.count(str(e,"uav_id")) || !mock_.at("uavs").as_object().contains(str(e,"uav_id")))
                    throw std::runtime_error("MOCK_UAV_UNAVAILABLE");
                const auto s=str(e,"state");
                if(s=="CREATED")transition(next,e,"PREFLIGHT","PREFLIGHT_STARTED");
                else if(s=="PREFLIGHT") {
                    checkPath(e.at("route_snapshot").as_object());
                    executionEvent(next,e,"PREFLIGHT_PASSED");transition(next,e,"STARTING","MISSION_STARTING");
                } else if(s=="STARTING") {
                    e["started_at"]=OperationalContext::now();transition(next,e,"EXECUTING","MISSION_STARTED");
                } else advanceExecution(next,e,std::min(dt,1.));
            } catch(const std::exception& ex) {
                e["failure_reason"]=ex.what();e["completed_at"]=OperationalContext::now();transition(next,e,"FAILED","MISSION_FAILED");
            }
        }
        if(changed)commitExecutions(next,publish);
    }
private:
    Object mock_{{"default_speed_mps",8.},{"uavs",Object{}}};
    static double number(const Object& o,const char* k){return o.at(k).to_number<double>();}
    static Object position(const Object& o){return {{"latitude",o.at("latitude")},{"longitude",o.at("longitude")},{"altitude",o.at("altitude")}};}
    static double distance(const Object& a,const Object& b) {
        constexpr double rad=3.141592653589793/180.,earth=6371000.;
        const double y=(number(b,"latitude")-number(a,"latitude"))*rad*earth;
        const double x=std::remainder(number(b,"longitude")-number(a,"longitude"),360.)*rad*earth*std::cos((number(a,"latitude")+number(b,"latitude"))*.5*rad);
        const double z=number(b,"altitude")-number(a,"altitude");return std::sqrt(x*x+y*y+z*z);
    }
    static void executionEvent(Object& doc,const Object& e,const char* type) {
        event(doc,type,"MISSION",str(e,"execution_id"),type,"Mock Execution");
        auto& log=doc.at("events").as_array().back().as_object();
        log["params"]=Object{{"target_id",e.at("execution_id")},{"uav_id",e.at("uav_id")},{"reason",e.at("failure_reason")}};
        log["execution_snapshot"]=Object{{"state",e.at("state")},{"position",e.at("position")},{"progress",e.at("progress")},{"current_waypoint",e.at("current_waypoint")}};
    }
    static void transition(Object& doc,Object& e,const char* state,const char* type) {
        e["state"]=state;e["updated_at"]=OperationalContext::now();e["control_version"]=number(e,"control_version")+1;
        executionEvent(doc,e,type);
    }
    void commitExecutions(Object& next,const std::function<void(const Object&)>& publish) {
        next["execution_version"]=state_.at("execution_version").to_number<int64_t>()+1;
        persist(next);
        try {publish(Object{{"type","SecurityPlansChanged"},{"payload",next}});}catch(...){persist(state_);throw;}
        state_.swap(next);
    }
    Object executionRequest(const Object& r,const std::set<std::string>& uavs,const std::string&,const std::function<void(const Object&)>& publish) {
        const auto action=str(r,"action"),key=str(r,"request_id");
        if(key.empty())throw PlanError("REQUEST_ID_REQUIRED","Execution request id required");
        Object identity=r;identity.erase("expected_version");identity.erase("instance_id");
        auto& requests=state_.at("execution_requests").as_object();
        if(auto* old=requests.if_contains(key)) {
            if(old->as_object().at("request")!=identity)throw PlanError("REQUEST_ID_REUSED","Request id has different content");
            return {{"state",state_},{"execution_id",old->as_object().at("execution_id")}};
        }
        auto next=state_;auto& all=next.at("executions").as_object();auto id=str(r,"execution_id");
        if(action=="execution_start") {
            const auto pid=str(r,"plan_id"),mid=str(r,"mission_id");
            const auto& plan=lookup(next.at("plans").as_object(),pid,"PLAN_NOT_FOUND");
            if(str(plan,"status")!="DEPLOYED")throw PlanError("PLAN_NOT_DEPLOYED","Deploy before starting");
            const auto did=str(plan,"deployment_id");
            const auto& deployment=lookup(next.at("deployments").as_object(),did,"DEPLOYMENT_MISSING");
            const auto& snap=deployment.at("snapshot").as_object();
            const auto* mv=snap.at("missions").as_object().if_contains(mid);
            if(!mv)throw PlanError("MISSION_PLAN_MISMATCH","Task does not belong to deployment");
            const auto& mission=mv->as_object();const auto uid=str(mission,"assigned_uav_id"),rid=str(mission,"route_id");
            if(!uavs.count(uid) || !mock_.at("uavs").as_object().contains(uid))throw PlanError("MOCK_UAV_UNAVAILABLE","Mock UAV unavailable");
            for(const auto& entry:all)if(str(entry.value().as_object(),"uav_id")==uid && executionActive(entry.value().as_object()))
                throw PlanError("EXECUTION_CONFLICT",uid+" already has an active mission");
            const auto& route=snap.at("paths").as_object().at(rid).as_object();checkPath(route);
            const auto& points=route.at("waypoints").as_array();if(points.size()<2)throw PlanError("ROUTE_TOO_SHORT","At least two waypoints required");
            const auto home=position(mock_.at("uavs").as_object().at(uid).as_object().at("home_position").as_object());
            for(const auto* k:{"latitude","longitude","altitude"})if(!std::isfinite(number(home,k)))throw PlanError("MOCK_UAV_UNAVAILABLE","Invalid mock home");
            if(std::abs(number(home,"latitude"))>90 || std::abs(number(home,"longitude"))>180)throw PlanError("MOCK_UAV_UNAVAILABLE","Invalid mock home");
            Object start=home;double latest=-1;
            for(const auto& entry:all){const auto& prev=entry.value().as_object();if(str(prev,"uav_id")==uid && number(prev,"updated_at")>=latest){latest=number(prev,"updated_at");start=prev.at("position").as_object();}}
            double total=0,seconds=0;auto from=start;
            for(const auto& v:points){const auto& point=v.as_object();const auto d=distance(from,point);const auto speed=number(point,"segmentSpeed")>0?number(point,"segmentSpeed"):number(mock_,"default_speed_mps");total+=d;seconds+=d/speed+number(point,"waitTime");from=point;}
            id=allocate(next,"execution-");const auto now=OperationalContext::now();
            all[id]=Object{{"execution_id",id},{"deployment_id",did},{"plan_id",pid},{"mission_id",mid},{"route_id",rid},
                {"route_revision",route.contains("revision")?route.at("revision"):deployment.at("content_revision")},{"route_snapshot",route},{"plan_name",plan.at("name")},{"mission_name",mission.at("name")},
                {"uav_id",uid},{"state","CREATED"},{"control_version",0},{"current_waypoint",1},{"completed_waypoints",0},{"total_waypoints",points.size()},
                {"segment_progress",0.},{"overall_progress",0.},{"progress",0.},{"position",start},{"segment_start",start},{"home_position",home},
                {"default_speed_mps",mock_.at("default_speed_mps")},{"distance_m",total},{"estimated_seconds",seconds},{"distance_travelled_m",0.},
                {"wait_remaining",0.},{"elapsed_seconds",0.},{"created_at",now},{"started_at",nullptr},{"updated_at",now},{"completed_at",nullptr},
                {"completion_reason",""},{"failure_reason",""},{"simulation",true}};
            executionEvent(next,all.at(id).as_object(),"EXECUTION_CREATED");
        } else if(action=="execution_shutdown") {
            for(auto& entry:all){auto& e=entry.value().as_object();if(executionActive(e) && str(e,"state")!="PAUSED") {
                e["resume_state"]=str(e,"state");transition(next,e,"PAUSED","MISSION_PAUSED");}}
        } else {
            auto& e=lookup(all,id,"EXECUTION_NOT_FOUND");const auto s=str(e,"state");
            const auto* version=r.if_contains("control_version");
            if(!version || !version->is_number() || version->to_number<double>()!=number(e,"control_version"))throw PlanError("EXECUTION_STALE","Execution state changed; retry");
            if(action=="execution_pause" && s=="EXECUTING")transition(next,e,"PAUSED","MISSION_PAUSED");
            else if(action=="execution_pause" && s=="PAUSED"){}
            else if(action=="execution_resume" && s=="PAUSED") {
                const auto previous=str(e,"resume_state");e.erase("resume_state");
                transition(next,e,previous.empty()?"EXECUTING":previous.c_str(),"MISSION_RESUMED");
            } else if(action=="execution_return" && s=="EXECUTING") {
                e["segment_start"]=e.at("position");transition(next,e,"RETURNING","MISSION_RETURNING");
            } else if(action=="execution_abort" && (s=="EXECUTING" || s=="PAUSED")) {
                e["completed_at"]=OperationalContext::now();transition(next,e,"ABORTED","MISSION_ABORTED");
            } else throw PlanError("EXECUTION_STATE_INVALID","Action is unavailable in current execution state");
        }
        next.at("execution_requests").as_object()[key]=Object{{"request",identity},{"execution_id",id}};
        commitExecutions(next,publish);return {{"state",state_},{"execution_id",id}};
    }
    static void advanceExecution(Object& doc,Object& e,double dt) {
        const bool returning=str(e,"state")=="RETURNING";auto& points=e.at("route_snapshot").as_object().at("waypoints").as_array();
        if(points.size()<2)throw std::runtime_error("IMMUTABLE_ROUTE_MISSING");
        e["elapsed_seconds"]=number(e,"elapsed_seconds")+dt;e["updated_at"]=OperationalContext::now();
        double remaining=dt;
        for(size_t guard=0;remaining>0 && guard<=points.size();++guard) {
            const double wait=number(e,"wait_remaining");
            if(!returning && wait>0){const auto used=std::min(wait,remaining);e["wait_remaining"]=wait-used;remaining-=used;if(remaining<=0)break;}
            const int completed=static_cast<int>(number(e,"completed_waypoints"));
            if(!returning && completed>=static_cast<int>(points.size())) {finishExecution(doc,e,"ROUTE_FINISHED");break;}
            const Object target=returning?e.at("home_position").as_object():position(points[completed].as_object());
            auto pos=e.at("position").as_object();const double length=distance(pos,target);
            const double configured=returning?0:number(points[completed].as_object(),"segmentSpeed");
            const double speed=configured>0?configured:number(e,"default_speed_mps");
            const double moved=std::min(length,speed*remaining),fraction=length>1e-8?moved/length:1.;
            for(const char* k:{"latitude","longitude","altitude"})pos[k]=number(pos,k)+(number(target,k)-number(pos,k))*fraction;
            pos["longitude"]=std::remainder(number(e.at("position").as_object(),"longitude")+std::remainder(number(target,"longitude")-number(e.at("position").as_object(),"longitude"),360.)*fraction,360.);
            e["position"]=pos;remaining-=moved/speed;
            if(!returning){e["distance_travelled_m"]=number(e,"distance_travelled_m")+moved;
                const double total=number(e,"distance_m"),progress=total>1e-8?std::min(.999999,number(e,"distance_travelled_m")/total):0;
                e["progress"]=progress;e["overall_progress"]=progress;
                const double segment=distance(e.at("segment_start").as_object(),target);e["segment_progress"]=segment>1e-8?1-distance(pos,target)/segment:1.;}
            if(fraction<1.)break;
            e["position"]=target;
            if(returning){finishExecution(doc,e,"RETURNED_HOME");break;}
            e["completed_waypoints"]=completed+1;executionEvent(doc,e,"WAYPOINT_REACHED");
            e["wait_remaining"]=number(points[completed].as_object(),"waitTime");e["segment_start"]=target;
            e["current_waypoint"]=std::min(completed+2,static_cast<int>(points.size()));e["segment_progress"]=0.;
            if(completed+1==static_cast<int>(points.size()) && number(e,"wait_remaining")==0){finishExecution(doc,e,"ROUTE_FINISHED");break;}
        }
    }
    static void finishExecution(Object& doc,Object& e,const char* reason) {
        e["completion_reason"]=reason;e["completed_at"]=OperationalContext::now();
        if(std::string(reason)=="ROUTE_FINISHED"){e["progress"]=1.;e["overall_progress"]=1.;e["segment_progress"]=1.;}
        transition(doc,e,"COMPLETED","MISSION_COMPLETED");
    }
