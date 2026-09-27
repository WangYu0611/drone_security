// Included inside SecurityPlanStore: Execution shares its mutex, atomic document
// and event log. The historical filename is retained for include compatibility.
// Lifecycle, hover, formation, scheduling and progress are shared by both adapters.
public:
    void configureMock(const Object& config) {configureAdapter(config,std::make_shared<security_mission::MockAdapter>());}
    bool usesRealAdapter() const {std::lock_guard<std::mutex> lock(mutex_);return !adapter_->simulation();}
    void configureAdapter(const Object& config,std::shared_ptr<security_mission::Adapter> adapter) {
        if(!adapter)throw std::runtime_error("ADAPTER_REQUIRED");
        std::lock_guard<std::mutex> lock(mutex_);
        adapter_=std::move(adapter);
        mock_ = config;
        if (!mock_.contains("uavs")) mock_["uavs"] = Object{};
        const double speed = mock_.contains("default_speed_mps") ? mock_.at("default_speed_mps").to_number<double>() : 8.;
        if (!std::isfinite(speed) || speed <= 0) throw std::runtime_error("Invalid MockDefaultSpeed");
        mock_["default_speed_mps"] = speed;
        for(const auto* key:{"safety_separation_m","safety_margin_m"})if(const auto* v=mock_.if_contains(key))
            if(!v->is_number() || !std::isfinite(v->to_number<double>()) || v->to_number<double>()<0 || (std::string(key)=="safety_separation_m" && v->to_number<double>()==0))throw std::runtime_error("Invalid safety configuration");
        for(const auto& item:mock_.at("uavs").as_object())if(const auto* r=item.value().as_object().if_contains("radius_m"))
            if(!r->is_number() || !std::isfinite(r->to_number<double>()) || r->to_number<double>()<0)throw std::runtime_error("Invalid UAV radius");
        mock_["safety_separation_m"]=safetySeparation();
        state_["mock_execution"] = mock_; // Legacy configuration key remains readable.
        state_["execution_adapter"]=adapter_->simulation()?"Mock":"Real";
        if(!adapter_->simulation())for(auto& item:state_.at("executions").as_object()){
            auto& e=item.value().as_object();
            if(executionActive(e) && str(e,"state")!="SCHEDULED" && str(e,"state")!="PAUSED"){
                e["resume_state"]=e.at("state");e["failure_reason"]="REAL_RESTART_REQUIRES_RESUME";
                transition(state_,e,"PAUSED","MISSION_PAUSED");
            }
        }
        if(!adapter_->simulation())persist(state_);
    }
    static bool executionActive(const Object& e) {
        const auto s=str(e,"state");
        return s=="SCHEDULED" || s=="CREATED" || s=="PREFLIGHT" || s=="STARTING" || s=="EXECUTING" || s=="PAUSED" || s=="RETURNING";
    }
    void tickExecutions(double dt,const std::set<std::string>& uavs,const std::function<void(const Object&)>& publish) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!std::isfinite(dt) || dt<=0) return;
        auto next=state_; bool changed=false;
        // Scheduled executions live in the same durable document and use the same
        // adapter and state machine as immediate starts. Due groups start atomically.
        std::set<std::string> due;
        for(const auto& item:next.at("executions").as_object()){const auto& e=item.value().as_object();
            if(str(e,"state")=="SCHEDULED" && number(e,"scheduled_start_epoch")<=OperationalContext::now())due.insert(str(e,"group_id"));}
        for(const auto& group:due) {
            Object routes;
            for(const auto& item:next.at("executions").as_object()){const auto& e=item.value().as_object();if(str(e,"group_id")==group && str(e,"state")=="SCHEDULED")routes[str(e,"uav_id")]=e.at("route_snapshot");}
            std::string failure;Object startPositions;
            try {checkTrajectories(next,routes);for(const auto& r:routes){const std::string uid(r.key());if(!uavs.count(uid))throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","Scheduled aircraft unavailable");startPositions[uid]=currentPosition(next,uid);}}
            catch(const std::exception& ex){failure=ex.what();}
            for(auto& item:next.at("executions").as_object()){auto& e=item.value().as_object();if(str(e,"group_id")!=group || str(e,"state")!="SCHEDULED")continue;
                if(failure.empty()){e["position"]=startPositions.at(str(e,"uav_id"));e["segment_start"]=e.at("position");transition(next,e,"CREATED","SCHEDULE_STARTED");}
                else{e["failure_reason"]=failure;e["completed_at"]=OperationalContext::now();transition(next,e,"FAILED","MISSION_FAILED");}}
            changed=true;
        }
        for(auto& entry:next.at("executions").as_object()) {
            auto& e=entry.value().as_object(); if(!executionActive(e))continue;
            if(e.at("simulation").as_bool()!=adapter_->simulation()){
                e["failure_reason"]="ADAPTER_MODE_MISMATCH";transition(next,e,"FAILED","MISSION_FAILED");changed=true;continue;
            }
            if(str(e,"state")=="SCHEDULED")continue;
            if(adapter_->simulation() && str(e,"state")=="PAUSED")continue;
            changed=true;
            try {
                if(!adapter_->simulation()){
                    e["position"]=jsonPosition(adapter_->observe(str(e,"uav_id"),geo(e.at("position").as_object())));
                    e["updated_at"]=OperationalContext::now();
                    if(str(e,"state")=="PAUSED")continue;
                }
                if(!uavs.count(str(e,"uav_id")) || !mock_.at("uavs").as_object().contains(str(e,"uav_id")))
                    throw std::runtime_error(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE");
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
        if(changed) {
            if(!adapter_->simulation()){
                std::set<std::string> failedGroups;
                for(const auto& item:next.at("executions").as_object())if(str(item.value().as_object(),"state")=="FAILED")failedGroups.insert(str(item.value().as_object(),"group_id"));
                for(auto& item:next.at("executions").as_object()){auto& e=item.value().as_object();
                    if(failedGroups.count(str(e,"group_id")) && executionActive(e) && str(e,"state")!="SCHEDULED"){
                        e["completed_at"]=OperationalContext::now();e["failure_reason"]="GROUP_MEMBER_FAILED";transition(next,e,"FAILED","MISSION_FAILED");
                    }
                }
            }
            // Continuous swept-segment check prevents a collision between ticks, including
            // a moving member approaching a paused member. Freeze the entire moving set.
            std::vector<std::string> ids;for(const auto& u:mock_.at("uavs").as_object())ids.emplace_back(u.key());
            bool conflict=false;
            try {for(size_t i=0;i<ids.size();++i)for(size_t j=i+1;j<ids.size();++j){
                const auto oldA=currentPosition(state_,ids[i],false),oldB=currentPosition(state_,ids[j],false);
                const auto newA=currentPosition(next,ids[i],false),newB=currentPosition(next,ids[j],false);
                if(distance(oldA,newA)<1e-10 && distance(oldB,newB)<1e-10)continue;
                const Timeline ta{{0.,1.,oldA,newA}},tb{{0.,1.,oldB,newB}};
                if(minimumSeparation(ta,tb)+1e-6<requiredDistance(ids[i],ids[j]))conflict=true;
            }
            }catch(...){if(adapter_->simulation())throw;conflict=true;}
            if(conflict){if(adapter_->simulation())next=state_;for(auto& item:next.at("executions").as_object()){auto& e=item.value().as_object();if(executionActive(e) && str(e,"state")!="SCHEDULED") {
                e["failure_reason"]="TRAJECTORY_CONFLICT";e["resume_state"]=str(e,"state");transition(next,e,"PAUSED","SAFETY_CONFLICT");}}}
            commitExecutions(next,publish);
        }
    }
private:
    std::shared_ptr<security_mission::Adapter> adapter_=std::make_shared<security_mission::MockAdapter>();
    Object mock_{{"default_speed_mps",8.},{"uavs",Object{}}};
    static double number(const Object& o,const char* k){return o.at(k).to_number<double>();}
    static Object position(const Object& o){return {{"latitude",o.at("latitude")},{"longitude",o.at("longitude")},{"altitude",o.at("altitude")}};}
    static security_mission::Position geo(const Object& p){return {number(p,"latitude"),number(p,"longitude"),number(p,"altitude")};}
    static Object jsonPosition(security_mission::Position p){return {{"latitude",p.latitude},{"longitude",p.longitude},{"altitude",p.altitude}};}
    Object adapterPosition(const std::string& uid,security_mission::Position fallback,bool home=false) const {
        try{return jsonPosition(home?adapter_->home(uid,fallback):adapter_->observe(uid,fallback));}
        catch(const std::exception& error){
            if(adapter_->simulation())throw;
            const std::string reason=error.what();
            throw PlanError(reason.rfind("REAL_",0)==0 || reason.rfind("ADAPTER_",0)==0 || reason=="LEGACY_CONTROLLER_BUSY"?reason:"REAL_CONFIGURATION_INVALID",reason);
        }
    }
    Object homePosition(const std::string& uid) const {
        const auto& config=mock_.at("uavs").as_object().at(uid).as_object();
        const auto* p=config.if_contains("home_position");
        if(!p && adapter_->simulation())throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","No configured home");
        return adapterPosition(uid,p?geo(p->as_object()):security_mission::Position{},true);
    }
    static double distance(const Object& a,const Object& b) {return security_mission::distance(geo(a),geo(b));}
    static void executionEvent(Object& doc,const Object& e,const char* type) {
        event(doc,type,"MISSION",str(e,"execution_id"),type,"Mission Controller");
        auto& log=doc.at("events").as_array().back().as_object();
        log["params"]=Object{{"target_id",e.at("execution_id")},{"uav_id",e.at("uav_id")},{"reason",e.at("failure_reason")}};
        log["execution_snapshot"]=Object{{"state",e.at("state")},{"position",e.at("position")},{"progress",e.at("progress")},{"current_waypoint",e.at("current_waypoint")}};
    }
    static void transition(Object& doc,Object& e,const char* state,const char* type) {
        e["state"]=state;e["updated_at"]=OperationalContext::now();e["control_version"]=number(e,"control_version")+1;
        executionEvent(doc,e,type);
    }
    void commitExecutions(Object& next,const std::function<void(const Object&)>& publish) {
        if(!adapter_->simulation())for(auto& item:next.at("executions").as_object()){
            auto& e=item.value().as_object();const auto status=str(e,"state");
            const auto* previous=state_.at("executions").as_object().if_contains(item.key());
            if(!e.at("simulation").as_bool() && previous &&
               (str(previous->as_object(),"state")=="EXECUTING" || str(previous->as_object(),"state")=="RETURNING" || str(previous->as_object(),"state")=="PAUSED") &&
               (str(previous->as_object(),"state")!=status || number(previous->as_object(),"control_version")!=number(e,"control_version")) &&
               (status=="PAUSED"||status=="ABORTED"||status=="FAILED"||status=="COMPLETED")){
                bool held=false;try{held=adapter_->hold(str(e,"uav_id"));}catch(...){}
                if(!held){e["failure_reason"]="REAL_HOLD_REJECTED";e["state"]="FAILED";}
            }
        }
        next["execution_version"]=state_.at("execution_version").to_number<int64_t>()+1;
        persist(next);
        try {publish(Object{{"type","SecurityPlansChanged"},{"payload",next}});}catch(...){if(adapter_->simulation()){persist(state_);throw;}}
        state_.swap(next);
    }
    std::string createRuntimeExecution(Object& next,const Object& route,const Object& plan,const Object& mission,
        const std::string& uid,const std::string& pid,const std::string& mid,const std::string& rid,const std::string& did) {
        auto& all=next.at("executions").as_object();const auto& points=route.at("waypoints").as_array();
        const auto home=homePosition(uid);
        const auto start=currentPosition(next,uid);
            double total=0,seconds=0;auto from=start;
            for(const auto& v:points){const auto& point=v.as_object();const auto d=distance(from,point);const auto speed=number(point,"segmentSpeed")>0?number(point,"segmentSpeed"):number(mock_,"default_speed_mps");total+=d;seconds+=d/speed+number(point,"waitTime");from=point;}
            if(closedRoute(route)){const auto d=distance(points.back().as_object(),points.front().as_object());const double configured=number(points.back().as_object(),"segmentSpeed");total+=d;seconds+=d/(configured>0?configured:number(mock_,"default_speed_mps"));}
            const auto id=allocate(next,"execution-");const auto now=OperationalContext::now();
            all[id]=Object{{"execution_id",id},{"deployment_id",did},{"plan_id",pid},{"mission_id",mid},{"route_id",rid},
                {"route_revision",route.contains("revision")?route.at("revision"):boost::json::value(1)},{"route_snapshot",route},{"plan_name",plan.at("name")},{"mission_name",mission.at("name")},
                {"uav_id",uid},{"state","CREATED"},{"control_version",0},{"current_waypoint",1},{"completed_waypoints",0},{"total_waypoints",points.size()},
                {"segment_progress",0.},{"overall_progress",0.},{"progress",0.},{"position",start},{"segment_start",start},{"home_position",home},
                {"default_speed_mps",mock_.at("default_speed_mps")},{"distance_m",total},{"estimated_seconds",seconds},{"distance_travelled_m",0.},
                {"wait_remaining",0.},{"elapsed_seconds",0.},{"created_at",now},{"started_at",nullptr},{"updated_at",now},{"completed_at",nullptr},
                {"completion_reason",""},{"failure_reason",""},{"simulation",adapter_->simulation()},{"adapter",adapter_->simulation()?"Mock":"Real"},{"loopMode","Once"},{"closure_completed",false}};
        return id;
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
        if(action=="execution_group_move") {
            const auto members=assignment(r);if(members.size()<2)throw PlanError("INVALID_UAV","Select at least two UAVs");
            std::set<std::string> unique;
            for(const auto& v:members){const std::string uid(v.as_string());if(!unique.insert(uid).second || !uavs.count(uid) || !mock_.at("uavs").as_object().contains(uid))throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","Selected UAV is unavailable");
                for(const auto& old:all)if(str(old.value().as_object(),"uav_id")==uid && executionActive(old.value().as_object()))throw PlanError("EXECUTION_CONFLICT","Selected UAV already has a mission");}
            const auto* t=r.if_contains("target");if(!t || !t->is_object())throw PlanError("INVALID_WAYPOINT","Group anchor required");
            for(const auto* k:{"latitude","longitude","altitude"}){const auto* v=t->as_object().if_contains(k);if(!v || !v->is_number() || !std::isfinite(v->to_number<double>()))throw PlanError("INVALID_WAYPOINT","Group target requires finite latitude, longitude and altitude");}
            const auto target=position(t->as_object());
            if(std::abs(number(target,"latitude"))>80 || std::abs(number(target,"longitude"))>180)throw PlanError("INVALID_WAYPOINT","Group target outside supported local area");
            const auto anchor=currentPosition(next,std::string(members.front().as_string()));
            if(distance(anchor,target)>10000.)throw PlanError("TRAJECTORY_DOMAIN_UNSUPPORTED","Group move is limited to 10 km");
            const auto* area=r.if_contains("target_area_radius_m");
            const double radius=area && area->is_number()?area->to_number<double>():10000.;
            if(!std::isfinite(radius) || radius<=0)throw PlanError("INVALID_SEPARATION","Target area radius must be positive");
            Object routes;bool needsPacking=false;
            for(const auto& v:members){const std::string uid(v.as_string());const auto start=currentPosition(next,uid);auto end=target;
                end["latitude"]=number(target,"latitude")+number(start,"latitude")-number(anchor,"latitude");
                end["longitude"]=std::remainder(number(target,"longitude")+std::remainder(number(start,"longitude")-number(anchor,"longitude"),360.),360.);
                end["altitude"]=number(target,"altitude")+number(start,"altitude")-number(anchor,"altitude");
                if(distance(end,target)>radius)needsPacking=true;
                auto first=start;first["sequence"]=1;first["segmentSpeed"]=0.;first["waitTime"]=0.;end["sequence"]=2;end["segmentSpeed"]=number(mock_,"default_speed_mps");end["waitTime"]=0.;
                routes[uid]=Object{{"waypoints",Array{first,end}},{"bClosedLoop",false}};
            }
            if(needsPacking){
                std::vector<std::string> order(unique.begin(),unique.end());
                std::sort(order.begin(),order.end(),[&](const auto& a,const auto& b){return number(currentPosition(next,a),"longitude")<number(currentPosition(next,b),"longitude");});
                double spacing=safetySeparation();for(const auto& a:order)for(const auto& b:order)if(a!=b)spacing=std::max(spacing,requiredDistance(a,b));
                spacing*=1.01; // Numerical/geodetic margin above the configured minimum.
                if((order.size()-1)*spacing*.5>radius)throw PlanError("TRAJECTORY_CONFLICT","Target area cannot contain a safe Line formation",{{"required_separation_m",spacing},{"available_radius_m",radius}});
                const double scale=6371000.*3.141592653589793/180.*std::cos(number(target,"latitude")*3.141592653589793/180.);
                for(size_t i=0;i<order.size();++i){auto& end=routes.at(order[i]).as_object().at("waypoints").as_array()[1].as_object();
                    end["latitude"]=target.at("latitude");end["altitude"]=target.at("altitude");end["longitude"]=number(target,"longitude")+(i-(order.size()-1)*.5)*spacing/scale;}
            }
            checkTrajectories(next,routes);const auto group=allocate(next,"group-");
            for(const auto& member:routes){const std::string uid(member.key());const auto eid=createRuntimeExecution(next,member.value().as_object(),Object{{"name","Group Move"}},Object{{"name","Group Move"}},uid,"","","","");
                auto& e=all.at(eid).as_object();e["group_id"]=group;e["control_kind"]="GROUP_MOVE";e["formation"]=needsPacking?"Line":"RelativeOffset";e["safety_separation_m"]=safetySeparation();executionEvent(next,e,"EXECUTION_CREATED");if(id.empty())id=eid;}
        } else if(action=="execution_start") {
            const auto pid=str(r,"plan_id"),mid=str(r,"mission_id");
            const auto& plan=lookup(next.at("plans").as_object(),pid,"PLAN_NOT_FOUND");
            if(str(plan,"status")!="DEPLOYED")throw PlanError("PLAN_NOT_DEPLOYED","Deploy before starting");
            for(const auto& item:next.at("edit_sessions").as_object())if(str(item.value().as_object(),"plan_id")==pid)throw PlanError("PLAN_EDIT_IN_PROGRESS","Finish plan editing before execution");
            const auto did=str(plan,"deployment_id");
            const auto& deployment=lookup(next.at("deployments").as_object(),did,"DEPLOYMENT_MISSING");
            const auto& snap=deployment.at("snapshot").as_object();
            const auto* mv=snap.at("missions").as_object().if_contains(mid);
            if(!mv)throw PlanError("MISSION_PLAN_MISMATCH","Task does not belong to deployment");
            const auto& mission=mv->as_object();const auto rid=str(mission,"route_id");
            const auto& central=snap.at("paths").as_object().at(rid).as_object();
            const auto trajectories=formationRoutes(mission,central);
            if(trajectories.empty())throw PlanError("NO_UAV_ASSIGNED","Assign at least one UAV");
            for(const auto& member:trajectories) {
                const std::string uid(member.key());
                if(!uavs.count(uid) || !mock_.at("uavs").as_object().contains(uid))throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","Mock UAV unavailable");
                for(const auto& entry:all)if(str(entry.value().as_object(),"uav_id")==uid && executionActive(entry.value().as_object()))
                    throw PlanError("EXECUTION_CONFLICT",uid+" already has an active or scheduled mission");
            }
            // Legacy single-aircraft routes retain their original contract. Multi-aircraft
            // assignments always pass a joint, continuous-time preflight before creation.
            checkTrajectories(next,trajectories);
            const auto group=allocate(next,"group-");Array created;
            for(const auto& member:trajectories) {
                const std::string uid(member.key());const auto& route=member.value().as_object();
                const auto& points=route.at("waypoints").as_array();
                const auto home=homePosition(uid);
                for(const auto* k:{"latitude","longitude","altitude"})if(!std::isfinite(number(home,k)))throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","Invalid mock home");
                if(std::abs(number(home,"latitude"))>90 || std::abs(number(home,"longitude"))>180)throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","Invalid mock home");
                const auto start=currentPosition(next,uid);
                id=createRuntimeExecution(next,route,plan,mission,uid,pid,mid,rid,did);
                auto& execution=all.at(id).as_object();execution["group_id"]=group;
                execution["plan_route_snapshot"]=central;execution["formation"]=str(mission,"formation");
                execution["safety_separation_m"]=safetySeparation();
                if(r.contains("scheduled_start_at")){
                    const double when=parseScheduledTime(str(r,"scheduled_start_at"));
                    if(when<=OperationalContext::now())throw PlanError("INVALID_SCHEDULE","Scheduled time must be in the future");
                    execution["state"]="SCHEDULED";execution["scheduled_start_at"]=r.at("scheduled_start_at");execution["scheduled_start_epoch"]=when;
                }
                executionEvent(next,execution,"EXECUTION_CREATED");created.emplace_back(id);
            }
            id=std::string(created.front().as_string());
        } else if(action=="execution_shutdown") {
            for(auto& entry:all){auto& e=entry.value().as_object();if(executionActive(e) && str(e,"state")!="PAUSED" && str(e,"state")!="SCHEDULED") {
                e["resume_state"]=str(e,"state");transition(next,e,"PAUSED","MISSION_PAUSED");}}
        } else {
            auto& e=lookup(all,id,"EXECUTION_NOT_FOUND");const auto s=str(e,"state");
            const auto* version=r.if_contains("control_version");
            if(!version || !version->is_number() || version->to_number<double>()!=number(e,"control_version"))throw PlanError("EXECUTION_STALE","Execution state changed; retry");
            const auto group=str(e,"group_id");std::vector<std::string> targets;
            for(const auto& member:all)if(std::string(member.key())==id || (!group.empty() && str(member.value().as_object(),"group_id")==group))targets.emplace_back(member.key());
            for(const auto& target:targets){auto& e=all.at(target).as_object();const auto s=str(e,"state");
            if(action=="execution_reschedule" && s=="SCHEDULED") {
                const double when=parseScheduledTime(str(r,"scheduled_start_at"));if(when<=OperationalContext::now())throw PlanError("INVALID_SCHEDULE","Time must be in the future");
                e["scheduled_start_at"]=r.at("scheduled_start_at");e["scheduled_start_epoch"]=when;transition(next,e,"SCHEDULED","SCHEDULE_CHANGED");
            } else if(action=="execution_start_now" && s=="SCHEDULED") {
                e["scheduled_start_epoch"]=OperationalContext::now();transition(next,e,"SCHEDULED","SCHEDULE_DUE");
            } else if(action=="execution_cancel" && s=="SCHEDULED") {
                e["completed_at"]=OperationalContext::now();transition(next,e,"CANCELLED","MISSION_CANCELLED");
            } else if(action=="execution_pause" && s=="EXECUTING")transition(next,e,"PAUSED","MISSION_PAUSED");
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
        }
        next.at("execution_requests").as_object()[key]=Object{{"request",identity},{"execution_id",id}};
        commitExecutions(next,publish);return {{"state",state_},{"execution_id",id}};
    }
    void advanceExecution(Object& doc,Object& e,double dt) {
        const bool returning=str(e,"state")=="RETURNING";auto& points=e.at("route_snapshot").as_object().at("waypoints").as_array();
        if(points.size()<2)throw std::runtime_error("IMMUTABLE_ROUTE_MISSING");
        e["elapsed_seconds"]=number(e,"elapsed_seconds")+dt;e["updated_at"]=OperationalContext::now();
        double remaining=dt;
        for(size_t guard=0;remaining>0 && guard<=points.size();++guard) {
            const double wait=number(e,"wait_remaining");
            if(!returning && wait>0){
                if(!adapter_->simulation() && distance(e.at("position").as_object(),points[static_cast<size_t>(number(e,"completed_waypoints"))-1].as_object())>.5)throw std::runtime_error("REAL_HOVER_DRIFT");
                e["phase"]="HOVER";const auto used=std::min(wait,remaining);e["wait_remaining"]=wait-used;remaining-=used;if(remaining<=0)break;}
            e["phase"]=returning?"RETURNING":"TRAVEL";
            const int completed=static_cast<int>(number(e,"completed_waypoints"));
            const bool closing=!returning && completed>=static_cast<int>(points.size()) && closedRoute(e.at("route_snapshot").as_object()) && !(e.contains("closure_completed") && e.at("closure_completed").as_bool());
            if(!returning && completed>=static_cast<int>(points.size()) && !closing) {finishExecution(doc,e,"ROUTE_FINISHED");break;}
            if(closing)e["current_waypoint"]=1;
            const auto index=closing?0:completed;
            const Object target=returning?e.at("home_position").as_object():position(points[index].as_object());
            auto pos=e.at("position").as_object();const double length=distance(pos,target);
            const double configured=returning?0:number(points[closing?points.size()-1:completed].as_object(),"segmentSpeed");
            const double speed=configured>0?configured:number(e,"default_speed_mps");
            const auto motion=adapter_->advance({str(e,"uav_id"),str(e,"execution_id"),geo(target),speed},geo(pos),remaining);
            const double moved=distance(pos,jsonPosition(motion.position));
            pos=jsonPosition(motion.position);e["position"]=pos;remaining=std::max(0.,remaining-motion.consumed_seconds);
            if(!returning){e["distance_travelled_m"]=number(e,"distance_travelled_m")+moved;
                const double total=number(e,"distance_m"),progress=total>1e-8?std::min(.999999,number(e,"distance_travelled_m")/total):0;
                e["progress"]=progress;e["overall_progress"]=progress;
                const double segment=distance(e.at("segment_start").as_object(),target);e["segment_progress"]=segment>1e-8?1-distance(pos,target)/segment:1.;}
            if(!motion.arrived)break;
            if(adapter_->simulation())e["position"]=target;
            if(returning){finishExecution(doc,e,"RETURNED_HOME");break;}
            if(closing){e["closure_completed"]=true;e["current_waypoint"]=1;executionEvent(doc,e,"ROUTE_CLOSURE_REACHED");finishExecution(doc,e,"ROUTE_FINISHED");break;}
            e["completed_waypoints"]=completed+1;executionEvent(doc,e,"WAYPOINT_REACHED");
            e["wait_remaining"]=number(points[completed].as_object(),"waitTime");e["phase"]=number(e,"wait_remaining")>0?"HOVER":"TRAVEL";e["segment_start"]=e.at("position");
            e["current_waypoint"]=std::min(completed+2,static_cast<int>(points.size()));e["segment_progress"]=0.;
            if(completed+1==static_cast<int>(points.size()) && number(e,"wait_remaining")==0 && !closedRoute(e.at("route_snapshot").as_object())){finishExecution(doc,e,"ROUTE_FINISHED");break;}
        }
    }
    static void finishExecution(Object& doc,Object& e,const char* reason) {
        e["completion_reason"]=reason;e["completed_at"]=OperationalContext::now();
        if(std::string(reason)=="ROUTE_FINISHED"){e["progress"]=1.;e["overall_progress"]=1.;e["segment_progress"]=1.;}
        transition(doc,e,"COMPLETED","MISSION_COMPLETED");
    }
