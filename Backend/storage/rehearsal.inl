// Recorded rehearsals share saved missions and the route controller, with a
// dedicated fleet and a software-only motion adapter.
public:
    static bool isSimulationSlot(int slot) { return slot>=simulationFirstSlot_ && slot<=simulationLastSlot_; }

    Object simulationFleet() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Object fleet=state_.at("simulation_uavs").as_object();
        for(auto& entry:fleet) {
            auto& row=entry.value().as_object();
            row["position"]=rehearsalPosition(state_,std::string(entry.key()));
            row["telemetry_source"]="mock_rehearsal";
        }
        return fleet;
    }

private:
    static constexpr int simulationFirstSlot_=1001, simulationLastSlot_=1099;
    static constexpr double rehearsalSpeed_=8.;
    security_mission::MockAdapter rehearsalAdapter_;

    static bool isRehearsal(const Object& e) {
        const auto* value=e.if_contains("rehearsal");
        return value && *value==true;
    }

    static bool isSimulatedAircraft(const Object& doc,const std::string& uid) {
        return doc.at("simulation_uavs").as_object().contains(uid);
    }

    static bool rehearsalAssignment(const Object& doc,const Array& members) {
        size_t simulated=0;
        for(const auto& member:members)
            if(isSimulatedAircraft(doc,std::string(member.as_string())))++simulated;
        if(simulated && simulated!=members.size())
            throw PlanError("MIXED_MISSION_ASSIGNMENT","A single mission requires aircraft of the same type");
        return simulated>0;
    }

    static Object rehearsalPosition(const Object& doc,const std::string& uid) {
        const auto& aircraft=lookup(doc.at("simulation_uavs").as_object(),uid,"SIMULATION_UAV_UNAVAILABLE");
        const Object* latest=nullptr;
        for(const auto& entry:doc.at("executions").as_object()) {
            const auto& e=entry.value().as_object();
            if(isRehearsal(e) && str(e,"uav_id")==uid && (!latest || number(e,"created_at")>=number(*latest,"created_at")))
                latest=&e;
        }
        return (latest?latest->at("position"):aircraft.at("home_position")).as_object();
    }

    static void setSimulationBinding(Object& aircraft, const Object& request) {
        const auto* target = request.if_contains("resolved_real_binding");
        if (!target) return;
        if (!target->is_null() && (!target->is_object() || !target->as_object().contains("drone_id")))
            throw PlanError("REAL_UAV_REGISTRATION_CHANGED", "Select the currently registered aircraft");
        const auto* previous = aircraft.if_contains("binding_version");
        const auto version = previous ? previous->to_number<int64_t>() : 0;
        aircraft["binding_version"] = version + 1;
        aircraft["real_binding"] = *target;
    }

    static void captureSimulationCandidate(const Object& doc, Object& mission, const Array& assigned) {
        mission.erase("planned_real_aircraft");
        if (assigned.size() != 1) return;
        const auto* simulation = doc.at("simulation_uavs").as_object().if_contains(assigned.front().as_string());
        if (!simulation) return;
        const auto& aircraft = simulation->as_object();
        const auto* binding = aircraft.if_contains("real_binding");
        if (!binding || !binding->is_object()) return;
        auto candidate = binding->as_object();
        candidate["simulated_uav_id"] = assigned.front();
        candidate["binding_version"] = aircraft.at("binding_version");
        mission["planned_real_aircraft"] = std::move(candidate);
    }

    void createSimulationAircraft(Object& doc,const Object& r,const std::set<std::string>& physicalUavs,int maximum=12) const {
        const auto* count=r.if_contains("count");
        if(!count || !count->is_number() || !std::isfinite(count->to_number<double>()) ||
           count->to_number<double>()<1 || count->to_number<double>()>maximum ||
           std::floor(count->to_number<double>())!=count->to_number<double>())
            throw PlanError("SIMULATION_INVALID_COUNT","Add between one and " + std::to_string(maximum) + " simulated aircraft");
        for(const auto* key:{"latitude","longitude","altitude"}) {
            const auto* value=r.if_contains(key);
            if(!value || !value->is_number() || !std::isfinite(value->to_number<double>()))
                throw PlanError("SIMULATION_INVALID_ORIGIN","Enter finite coordinates and ellipsoid height");
        }
        const auto origin=geo(r);
        if(std::abs(origin.latitude)>80 || std::abs(origin.longitude)>180 || origin.altitude< -1000 || origin.altitude>100000)
            throw PlanError("SIMULATION_INVALID_ORIGIN","Simulation origin is outside the supported map limits");

        auto& fleet=doc.at("simulation_uavs").as_object();
        std::vector<int> slots;
        const int requested=count->to_number<int>();
        if (requested != 1 && r.contains("resolved_real_binding"))
            throw PlanError("SIMULATION_BINDING_SINGLE_AIRCRAFT", "Set the association on an individual simulated aircraft");
        for(int slot=simulationFirstSlot_;slot<=simulationLastSlot_ && static_cast<int>(slots.size())<requested;++slot) {
            const auto uid="UAV-"+std::to_string(slot);
            if(!fleet.contains(uid) && !physicalUavs.count(uid))slots.push_back(slot);
        }
        if(static_cast<int>(slots.size())<requested)
            throw PlanError("SIMULATION_FLEET_FULL","There are too few available simulated aircraft slots");

        // Place batches on an east-west line. Occupied starting points or
        // current positions push later aircraft to the next available point.
        constexpr double radians=3.14159265358979323846/180.;
        const double spacing=std::max(20.,safetySeparation()*2.);
        const double degrees=spacing/(6371000.*radians*std::cos(origin.latitude*radians));
        std::vector<Object> occupied;
        for(const auto& entry:fleet) {
            occupied.push_back(entry.value().as_object().at("home_position").as_object());
            occupied.push_back(rehearsalPosition(doc,std::string(entry.key())));
        }
        std::map<int,Object> starts;
        for(int i=0;starts.size()<slots.size() && i<2*(simulationLastSlot_-simulationFirstSlot_+1);++i) {
            const int offset=(i%2==0?1:-1)*((i+1)/2);
            auto point=origin;
            point.longitude=std::remainder(origin.longitude+offset*degrees,360.);
            auto candidate=jsonPosition(point);
            if(std::any_of(occupied.begin(),occupied.end(),[&](const Object& p){return distance(p,candidate)<spacing*.9;}))
                continue;
            occupied.push_back(candidate);
            starts.emplace(offset,std::move(candidate));
        }
        if(starts.size()!=slots.size())
            throw PlanError("SIMULATION_ORIGIN_OCCUPIED","Choose a different simulated starting area");
        auto start=starts.begin();
        for(const int slot:slots) {
            const auto uid="UAV-"+std::to_string(slot);
            const int ordinal=slot-simulationFirstSlot_+1;
            fleet[uid]=Object{{"slot",slot},{"name","SIM-"+std::string(ordinal<10?"0":"")+std::to_string(ordinal)},
                {"home_position",std::move(start->second)}};
            setSimulationBinding(fleet.at(uid).as_object(), r);
            ++start;
        }
    }

    Object rehearsalInputs(const Object& mission) const {
        const auto* spacing=mission.if_contains("spacing_m");
        return {{"assigned_uav_ids",assignment(mission)},{"formation",str(mission,"formation")},
                {"spacing_m",spacing?*spacing:boost::json::value(safetySeparation()*2.)}};
    }

    static void requireSavedRehearsalRoute(const Object& doc,const std::string& plan) {
        for(const auto& entry:doc.at("edit_sessions").as_object())
            if(str(entry.value().as_object(),"plan_id")==plan)
                throw PlanError("PLAN_EDIT_IN_PROGRESS","Finish or discard the route edit first");
    }

    static void sampleRehearsal(Object& execution,bool finalSample=false) {
        auto& samples=execution.at("replay_samples").as_array();
        double interval=number(execution,"replay_interval_seconds");
        const double elapsed=number(execution,"elapsed_seconds");
        if(!samples.empty() && !finalSample && elapsed-number(samples.back().as_object(),"elapsed_seconds")<interval)return;
        Object sample{{"elapsed_seconds",elapsed},{"position",execution.at("position")},
            {"progress",execution.at("progress")},{"current_waypoint",execution.at("current_waypoint")},
            {"completed_waypoints",execution.at("completed_waypoints")}};
        if(!samples.empty() && number(samples.back().as_object(),"elapsed_seconds")==elapsed) {
            samples.back()=std::move(sample);
            return;
        }
        if(samples.size()>=256) {
            Array reduced;
            for(size_t i=0;i<samples.size();i+=2)reduced.push_back(std::move(samples[i]));
            samples=std::move(reduced);
            execution["replay_interval_seconds"]=interval*2.;
        }
        samples.emplace_back(std::move(sample));
    }

    std::string startRehearsal(Object& doc,const Object& r) {
        const auto pid=str(r,"plan_id"),mid=str(r,"mission_id");
        const auto& plan=lookup(doc.at("plans").as_object(),pid,"PLAN_NOT_FOUND");
        const auto& mission=lookup(doc.at("missions").as_object(),mid,"MISSION_NOT_FOUND");
        if(str(mission,"plan_id")!=pid)throw PlanError("MISSION_PLAN_MISMATCH","Mission does not belong to the selected plan");
        requireSavedRehearsalRoute(doc,pid);
        if(!rehearsalAssignment(doc,assignment(mission)))
            throw PlanError("SIMULATION_UAV_REQUIRED","Assign one or more simulated aircraft first");
        const auto rid=str(mission,"route_id");
        const auto& central=lookup(doc.at("paths").as_object(),rid,"ROUTE_MISSING");
        const auto routes=formationRoutes(mission,central);
        auto& executions=doc.at("executions").as_object();
        for(const auto& route:routes)for(const auto& item:executions) {
            const auto& existing=item.value().as_object();
            if(str(existing,"uav_id")==route.key() && executionActive(existing))
                throw PlanError("EXECUTION_CONFLICT","A selected simulated aircraft is already assigned to an active run");
        }
        checkTrajectories(doc,routes,true);
        const auto group=allocate(doc,"group-");
        std::string first;
        for(const auto& entry:routes) {
            const auto eid=createRuntimeExecution(doc,entry.value().as_object(),plan,mission,
                std::string(entry.key()),pid,mid,rid,"",true);
            auto& e=executions.at(eid).as_object();
            e["group_id"]=group;
            e["rehearsal"]=true;
            e["rehearsal_inputs"]=rehearsalInputs(mission);
            e["plan_route_snapshot"]=central;
            e["formation"]=str(mission,"formation");
            e["safety_separation_m"]=safetySeparation();
            e["replay_samples"]=Array{};
            e["replay_interval_seconds"]=1.;
            sampleRehearsal(e,true);
            executionEvent(doc,e,"EXECUTION_CREATED");
            if(first.empty())first=eid;
        }
        return first;
    }

    void checkCurrentRehearsal(const Object& doc,const Object& execution) const {
        const auto mid=str(execution,"mission_id"),pid=str(execution,"plan_id");
        const auto* mission=doc.at("missions").as_object().if_contains(mid);
        if(!mission || str(mission->as_object(),"plan_id")!=pid)
            throw PlanError("REHEARSAL_STALE","The rehearsed task no longer exists");
        requireSavedRehearsalRoute(doc,pid);
        const auto* route=doc.at("paths").as_object().if_contains(str(mission->as_object(),"route_id"));
        if(!route || *route!=execution.at("plan_route_snapshot") || rehearsalInputs(mission->as_object())!=execution.at("rehearsal_inputs"))
            throw PlanError("REHEARSAL_STALE","The task changed. Run a new rehearsal");
    }

    void reviewRehearsal(Object& doc,const Object& execution,const std::string& source) const {
        checkCurrentRehearsal(doc,execution);
        const auto group=str(execution,"group_id");
        const Object review{{"reviewed_at",OperationalContext::now()},{"reviewed_by",source}};
        for(auto& entry:doc.at("executions").as_object()) {
            auto& e=entry.value().as_object();
            if(!isRehearsal(e) || str(e,"group_id")!=group)continue;
            if(str(e,"state")!="COMPLETED" || str(e,"completion_reason")!="ROUTE_FINISHED")
                throw PlanError("REHEARSAL_INCOMPLETE","Every assigned simulated aircraft must complete the route");
            if(!e.contains("rehearsal_review"))e["rehearsal_review"]=review;
        }
    }

    Object assignRehearsalToReal(Object& doc,const Object& execution,const Object& request,
                                const std::set<std::string>& physicalUavs,const std::string& source) {
        checkCurrentRehearsal(doc,execution);
        if(!execution.contains("rehearsal_review"))
            throw PlanError("REHEARSAL_REVIEW_REQUIRED","Review the completed rehearsal first");
        if(execution.at("rehearsal_inputs").as_object().at("assigned_uav_ids").as_array().size()!=1)
            throw PlanError("REHEARSAL_MULTI_AIRCRAFT","Rehearse a single-aircraft version before assigning one real aircraft");
        const auto uid=str(request,"assigned_uav_id");
        const auto* registration=request.if_contains("assigned_drone_id");
        if(!physicalUavs.count(uid) || isSimulatedAircraft(doc,uid) || !registration ||
           !registration->is_int64() || registration->as_int64()<=0)
            throw PlanError("REAL_UAV_REQUIRED","Select a registered real aircraft");
        if (const auto* expected = request.if_contains("expected_assigned_drone_id"); expected && *expected != *registration)
            throw PlanError("REAL_UAV_REGISTRATION_CHANGED", "Select the currently registered aircraft");

        const auto sourcePlan=str(execution,"plan_id"),sourceMission=str(execution,"mission_id");
        auto& sourceTask = doc.at("missions").as_object().at(sourceMission).as_object();
        for (const auto& entry : doc.at("plans").as_object()) {
            const auto& plan = entry.value().as_object();
            const auto* linked = plan.if_contains("real_assignment");
            if (!linked || !linked->is_object() || !str(plan, "source_plan_id").empty()) continue;
            const auto& record = linked->as_object();
            const auto* identity = record.if_contains("assigned_drone_id");
            if (str(record, "source_execution_id") == str(execution, "execution_id") &&
                str(record, "assigned_uav_id") == uid && identity && *identity == *registration) {
                if (const auto& tasks = plan.at("mission_ids").as_array(); !tasks.empty()) {
                    sourceTask["preferred_real_plan_id"] = entry.key();
                    doc["configuration_session"] = nullptr;
                    return {{"plan_id", entry.key()}, {"mission_id", tasks.front()}, {"existing_assignment", true}};
                }
            }
        }
        const auto pid=allocate(doc,"plan-"),mid=allocate(doc,"mission-"),rid=allocate(doc,"route-");
        auto& plans=doc.at("plans").as_object();
        auto& missions=doc.at("missions").as_object();
        const auto& original=plans.at(sourcePlan).as_object();
        sourceTask["preferred_real_plan_id"] = pid;
        auto mission=sourceTask;
        mission.erase("planned_real_aircraft");
        mission.erase("preferred_real_plan_id");
        mission["id"]=mid;mission["plan_id"]=pid;mission["route_id"]=rid;mission["status"]="DRAFT";
        mission["assigned_uav_id"]=uid;mission["assigned_uav_ids"]=Array{uid};
        missions[mid]=std::move(mission);
        auto route=execution.at("plan_route_snapshot").as_object();
        route["route_id"]=rid;
        doc.at("paths").as_object()[rid]=std::move(route);
        const double now=OperationalContext::now();
        plans[pid]=Object{{"id",pid},{"name",original.at("name")},{"description",original.at("description")},
            {"status","DRAFT"},{"content_revision",1},{"review",nullptr},{"version",1},{"created_at",now},{"updated_at",now},
            {"mission_ids",Array{mid}},{"workflow_step","PreDeployReview"},{"workflow_mission_id",mid},
            {"real_assignment",Object{{"source_execution_id",str(execution,"execution_id")},{"source_plan_id",sourcePlan},
                {"source_mission_id",sourceMission},{"assigned_uav_id",uid},{"assigned_drone_id",*registration}}}};
        auto validation=validate(doc,pid,physicalUavs);
        validation["validated_content_revision"]=nullptr;
        validation["ready"]=false;
        plans.at(pid).as_object()["validation"]=std::move(validation);
        doc["configuration_session"]=nullptr;
        event(doc,"REAL_ASSIGNMENT_CREATED","MISSION",mid,"Rehearsal assigned to a real aircraft pending checks",source);
        return {{"plan_id",pid},{"mission_id",mid}};
    }

    void checkRealAssignment(const Object& doc,const Object& plan,const Object& mission,
                             const std::set<std::string>& uavs) const {
        const auto& assignmentRecord=plan.at("real_assignment").as_object();
        const auto uid=str(assignmentRecord,"assigned_uav_id");
        if(!uavs.count(uid) || isSimulatedAircraft(doc,uid))
            throw PlanError("REAL_UAV_REQUIRED","The assigned real aircraft is unavailable");
        const auto& proof=lookup(doc.at("executions").as_object(),str(assignmentRecord,"source_execution_id"),"REHEARSAL_STALE");
        const auto* storedRoute=doc.at("paths").as_object().if_contains(str(mission,"route_id"));
        if(!isRehearsal(proof) || !proof.contains("rehearsal_review") || !storedRoute)
            throw PlanError("REHEARSAL_STALE","Rehearse the current task before assigning it");
        auto route=storedRoute->as_object();
        route["route_id"]=proof.at("plan_route_snapshot").as_object().at("route_id");
        auto inputs=rehearsalInputs(mission);
        auto approved=proof.at("rehearsal_inputs").as_object();
        approved["assigned_uav_ids"]=Array{uid};
        if(route!=proof.at("plan_route_snapshot") || inputs!=approved)
            throw PlanError("REHEARSAL_STALE","The assigned route or mission parameters changed");
        if(adapter_->simulation())
            throw PlanError("REAL_EXECUTION_DISABLED","Real mission execution is disabled on this Backend");
        const auto* configured=mock_.at("uavs").as_object().if_contains(uid);
        if(!configured)
            throw PlanError("REAL_UAV_UNAVAILABLE","The real aircraft has no calibrated execution configuration");
        const auto* registeredId=assignmentRecord.if_contains("assigned_drone_id");
        const auto* calibratedId=configured->as_object().if_contains("drone_id");
        if(!registeredId || !calibratedId || *registeredId!=*calibratedId)
            throw PlanError("REAL_UAV_REGISTRATION_CHANGED","Select the currently registered aircraft and review its assignment");
        homePosition(uid);
        adapterPosition(uid,{});
    }

    Object rehearsalRequest(Object& next,const Object& r,const std::set<std::string>& physicalUavs,
                            const std::string& source,const std::function<void(const Object&)>& publish) {
        const auto action=str(r,"action"), key=str(r,"request_id");
        if(key.empty() || key.size()>128)throw PlanError("REQUEST_ID_REQUIRED","A rehearsal request ID is required");
        auto identity=r;
        identity.erase("expected_version");identity.erase("instance_id");
        identity.erase("resolved_real_binding");
        if (r.contains("expected_assigned_drone_id")) identity.erase("assigned_drone_id");
        auto& requests=next.at("simulation_requests").as_object();
        if(const auto* recorded=requests.if_contains(key)) {
            const auto& prior=recorded->as_object();
            if(prior.at("request")!=identity)throw PlanError("REQUEST_ID_REUSED","Request id has different content");
            auto reply=prior.at("reply").as_object();
            reply["state"]=state_;
            reply["replayed"]=true;
            return reply;
        }
        if(const auto* version=r.if_contains("expected_version"))
            if(!version->is_number() || version->to_number<double>()!=number(state_,"version"))
                throw PlanError("VERSION_CONFLICT","Plan data changed; reload and retry");
        Object reply;
        if(action=="simulation_create") {
            createSimulationAircraft(next,r,physicalUavs);
        } else if (action == "simulation_set_real_binding") {
            auto& aircraft = lookup(next.at("simulation_uavs").as_object(), str(r, "simulated_uav_id"), "SIMULATION_UAV_UNAVAILABLE");
            const auto* version = aircraft.if_contains("binding_version");
            const auto expected = r.if_contains("expected_binding_version");
            if (!expected || *expected != boost::json::value(version ? version->to_number<int64_t>() : 0))
                throw PlanError("VERSION_CONFLICT", "Refresh the simulated aircraft association");
            if (!r.contains("resolved_real_binding"))
                throw PlanError("REAL_UAV_REQUIRED", "Choose a registered aircraft or clear the association");
            setSimulationBinding(aircraft, r);
        } else if(action=="simulation_start") {
            checkConfigurationRequest(next,r);
            reply["execution_id"]=startRehearsal(next,r);
        } else if(action=="simulation_review" || action=="simulation_assign_real") {
            const auto eid=str(r,"execution_id");
            const auto& execution=lookup(next.at("executions").as_object(),eid,"EXECUTION_NOT_FOUND");
            if(!isRehearsal(execution))throw PlanError("REHEARSAL_REQUIRED","Select a simulated rehearsal");
            auto scoped=r;
            scoped["plan_id"]=execution.at("plan_id");
            checkConfigurationRequest(next,scoped);
            if(action=="simulation_review")reviewRehearsal(next,execution,source);
            else reply=assignRehearsalToReal(next,execution,r,physicalUavs,source);
            reply["execution_id"]=eid;
        } else throw PlanError("UNKNOWN_ACTION","Unknown rehearsal action");
        requests[key]=Object{{"request",std::move(identity)},{"reply",reply}};
        next["version"]=state_.at("version").to_number<int64_t>()+1;
        persist(next);
        try {publish(Object{{"type","SecurityPlansChanged"},{"payload",next}});}
        catch (...) {persist(state_);throw;}
        state_.swap(next);
        reply["state"]=state_;
        return reply;
    }

    bool tickRehearsals(Object& doc,double dt) {
        auto& executions=doc.at("executions").as_object();
        std::map<std::string,Object> previous;
        const auto eventCount=doc.at("events").as_array().size();
        std::set<std::string> failedGroups;
        for(auto& item:executions) {
            auto& e=item.value().as_object();
            if(!isRehearsal(e) || !executionActive(e) || str(e,"state")=="PAUSED")continue;
            previous.emplace(std::string(item.key()),e);
            try {
                if(e.at("simulation")!=true || !isSimulatedAircraft(doc,str(e,"uav_id")))
                    throw std::runtime_error("SIMULATION_UAV_UNAVAILABLE");
                advanceMissionTick(doc,e,dt,rehearsalAdapter_);
            } catch(const std::exception& ex) {
                failedGroups.insert(str(e,"group_id"));
                e["failure_reason"]=ex.what();
                e["completed_at"]=OperationalContext::now();
                transition(doc,e,"FAILED","MISSION_FAILED");
            }
        }
        if(previous.empty())return false;

        std::map<std::string,Timeline> sweeps;
        const auto& fleet=doc.at("simulation_uavs").as_object();
        for(const auto& entry:fleet) {
            const std::string uid(entry.key());
            const auto end=rehearsalPosition(doc,uid);
            sweeps.emplace(uid,Timeline{{0.,1.,end,end}});
        }
        std::set<std::string> moving;
        for(const auto& entry:previous) {
            const auto& before=entry.second;
            const auto& after=executions.at(entry.first).as_object();
            const auto uid=str(before,"uav_id");
            if(!sweeps.count(uid))continue;
            auto& segment=sweeps.at(uid).front();
            segment.from=before.at("position").as_object();
            segment.to=after.at("position").as_object();
            if(distance(segment.from,segment.to)>1e-10)moving.insert(uid);
        }
        bool conflict=false;
        for(auto a=sweeps.begin();a!=sweeps.end() && !conflict;++a) {
            for(auto b=std::next(a);b!=sweeps.end();++b) {
                if(!moving.count(a->first) && !moving.count(b->first))continue;
                if(minimumSeparation(a->second,b->second)+1e-6<requiredDistance(a->first,b->first)) {conflict=true;break;}
            }
        }
        if(conflict) {
            doc.at("events").as_array().resize(eventCount);
            for(auto& record:previous) {
                executions[record.first]=std::move(record.second);
                auto& e=executions.at(record.first).as_object();
                e["failure_reason"]="TRAJECTORY_CONFLICT";
                e["completed_at"]=OperationalContext::now();
                transition(doc,e,"FAILED","SAFETY_CONFLICT");
            }
        }
        for(auto& item:executions) {
            auto& e=item.value().as_object();
            if(!isRehearsal(e))continue;
            if(executionActive(e) && failedGroups.count(str(e,"group_id"))) {
                e["failure_reason"]="GROUP_MEMBER_FAILED";
                e["completed_at"]=OperationalContext::now();
                transition(doc,e,"FAILED","MISSION_FAILED");
                sampleRehearsal(e,true);
            } else if(previous.count(std::string(item.key())))sampleRehearsal(e,!executionActive(e));
        }
        return true;
    }
