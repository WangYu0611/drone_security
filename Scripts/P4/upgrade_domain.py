from pathlib import Path
p=Path('Backend/storage/security_plan_store.h')
s=p.read_text(encoding='utf-8-sig')
s=s.replace('std::string code;', 'std::string code;\n    boost::json::object params;')
s=s.replace('code(std::move(c)) {}','code(std::move(c)) {}\n    PlanError(std::string c, std::string message, boost::json::object p) : std::runtime_error(message), code(std::move(c)), params(std::move(p)) {}')
s=s.replace('{"missions", Object{}}, {"paths", Object{}}, {"events", Array{}}','{"missions", Object{}}, {"paths", Object{}}, {"events", Array{}}, {"deployments", Object{}}, {"edit_sessions", Object{}}')
needle='    }\n    Object snapshot()'
s=s.replace(needle,'''        if (!state_.contains("deployments")) state_["deployments"] = Object{};
        if (!state_.contains("edit_sessions")) state_["edit_sessions"] = Object{};
        for (auto& entry : state_.at("plans").as_object()) {
            auto& p=entry.value().as_object();
            if (!p.contains("content_revision")) {
                p["content_revision"]=1; p["review"]=nullptr;
                // Legacy READY did not validate a content revision. Require a new check.
                if(str(p,"status")=="READY")p["status"]="DRAFT";
            }
        }
    }
    Object snapshot()''')
s=s.replace('auto next = state_;','auto next = state_;\n        expire(next, OperationalContext::now());',1)
s=s.replace('{"status", "DRAFT"}, {"version", 1}', '{"status", "DRAFT"}, {"content_revision",1}, {"review",nullptr}, {"version", 1}')
s=s.replace('''        } else {
            auto& plan = lookup(plans, planId, "PLAN_NOT_FOUND");''','''        } else if(action=="copy_plan") {
            const auto original=lookup(plans,planId,"PLAN_NOT_FOUND");
            if(str(original,"status")!="DEPLOYED")throw PlanError("PLAN_NOT_DEPLOYED","Copy requires deployed plan");
            const auto sourceId=planId; planId=allocate(next,"plan-");
            auto copy=original; copy["id"]=planId; copy["status"]="DRAFT";copy["version"]=1;
            copy["content_revision"]=1;copy["review"]=nullptr;copy["deployment"]=nullptr;
            copy["source_plan_id"]=sourceId;copy["source_deployment_id"]=str(original,"deployment_id");
            copy.erase("deployment_id");copy.erase("validation");copy["created_at"]=OperationalContext::now();copy["updated_at"]=OperationalContext::now();
            Array ids;
            for(const auto& id:original.at("mission_ids").as_array()) {
                auto m=missions.at(id.as_string()).as_object();auto mid=allocate(next,"mission-");
                m["id"]=mid;m["plan_id"]=planId;m["status"]="DRAFT";
                const auto oldRoute=str(m,"route_id");if(!oldRoute.empty()) {
                    auto r=paths.at(oldRoute).as_object();const auto rid=allocate(next,"route-");r["route_id"]=rid;paths[rid]=r;m["route_id"]=rid;
                }
                missions[mid]=m;ids.emplace_back(mid);
            }
            copy["mission_ids"]=ids;plans[planId]=copy;
            event(next,"PLAN_COPIED","PLAN",planId,"Plan copied as draft",source);
        } else {
            auto& plan = lookup(plans, planId, "PLAN_NOT_FOUND");''')
s=s.replace('''            if (action == "add_mission")''','''            const auto revision=plan.at("content_revision").to_number<int64_t>();
            const bool sessionAction=action=="begin_route_edit" || action=="mark_route_dirty" || action=="discard_route_edit";
            if(action=="review" || action=="deploy") {
                for(const auto& entry:next.at("edit_sessions").as_object()) {
                    const auto& session=entry.value().as_object();
                    if(str(session,"plan_id")==planId)throw PlanError("PLAN_EDIT_IN_PROGRESS","Route editing in progress",
                        {{"mission_id",session.at("mission_id")},{"owner_client",session.at("owner_client_id")},{"state",session.at("state")}});
                }
            }
            if(sessionAction) {
                auto& mission=lookup(missions,missionId,"MISSION_NOT_FOUND");
                if(str(mission,"plan_id")!=planId)throw PlanError("MISSION_PLAN_MISMATCH","Mission does not belong to plan");
                auto& sessions=next.at("edit_sessions").as_object();
                const auto owner=str(request,"instance_id");
                if(owner.empty())throw PlanError("INVALID_IDENTITY","Session owner required");
                if(action=="begin_route_edit") {
                    auto expected=request.if_contains("content_revision");
                    if(!expected || !expected->is_number() || expected->to_number<int64_t>()!=revision)throw PlanError("VERSION_CONFLICT","Content revision changed");
                    for(const auto& entry:sessions) {
                        const auto& existing=entry.value().as_object();
                        if(str(existing,"plan_id")==planId) {
                            if(str(existing,"owner_instance_id")==owner && str(existing,"mission_id")==missionId)
                                return Object{{"state",state_},{"edit_session_id",entry.key()},{"plan_id",planId},{"mission_id",missionId}};
                            throw PlanError("EDIT_SESSION_CONFLICT","Plan already has an editor");
                        }
                    }
                    const auto sid=allocate(next,"edit-");const auto now=OperationalContext::now();
                    sessions[sid]=Object{{"edit_session_id",sid},{"plan_id",planId},{"mission_id",missionId},{"owner_instance_id",owner},
                        {"owner_client_id",source},{"base_content_revision",revision},{"state","EDITING"},{"started_at",now},{"last_seen_at",now},{"lease_expires_at",now+20}};
                } else {
                    auto& session=ownedSession(next,request,planId,missionId);
                    if(action=="mark_route_dirty")session["state"]="DIRTY";
                    else sessions.erase(str(session,"edit_session_id"));
                }
            } else if (action == "add_mission")''')
a=s.index('            } else if (action == "validate" || action == "deploy")')
b=s.index('            } else {\n                auto& mission',a)
s=s[:a]+'''            } else if (action == "validate" || action == "review" || action == "deploy") {
                auto result = validate(next, planId, uavs);
                if(action=="validate") {
                    result["validated_content_revision"]=revision;plan["validation"]=result;
                    plan["status"]=result.at("ready").as_bool()?"READY":"DRAFT";
                    if(!result.at("ready").as_bool())plan["review"]=nullptr;
                    event(next,"PLAN_VALIDATED","PLAN",planId,"Configuration checked",source);
                } else {
                    auto* validation=plan.if_contains("validation");
                    if(str(plan,"status")!="READY")throw PlanError("PLAN_NOT_READY","Configuration check required");
                    if(!validation || !validation->is_object() || validation->as_object().at("validated_content_revision")!=plan.at("content_revision"))
                        throw PlanError("VALIDATION_STALE","Validation is stale");
                    if(!result.at("ready").as_bool())throw PlanError("PLAN_NOT_READY","Configuration recheck failed");
                    if(action=="review") {
                        plan["review"]=Object{{"content_revision",revision},{"reviewed_at",OperationalContext::now()},{"reviewed_by",source}};
                        event(next,"PLAN_REVIEWED","PLAN",planId,"Current configuration reviewed",source);
                    } else {
                        auto* review=plan.if_contains("review");
                        if(!review || !review->is_object())throw PlanError("PLAN_REVIEW_REQUIRED","Review required");
                        if(review->as_object().at("content_revision")!=plan.at("content_revision"))throw PlanError("REVIEW_STALE","Review is stale");
                        if(!request.contains("expected_version"))throw PlanError("VERSION_CONFLICT","Expected version required");
                        const auto deploymentId=allocate(next,"deployment-");
                        plan["status"]="DEPLOYED";plan["deployment_id"]=deploymentId;
                        Object snapM,snapR;
                        for(const auto& id:ids) {
                            auto& m=missions.at(id.as_string()).as_object();m["status"]="DEPLOYED";snapM[id.as_string()]=m;
                            const auto rid=str(m,"route_id");if(!rid.empty())snapR[rid]=paths.at(rid);
                        }
                        next.at("deployments").as_object()[deploymentId]=Object{{"id",deploymentId},{"plan_id",planId},
                            {"content_revision",revision},{"deployed_at",OperationalContext::now()},{"deployed_by",source},
                            {"reviewed_by",review->as_object().at("reviewed_by")},{"reviewed_at",review->as_object().at("reviewed_at")},
                            {"snapshot",Object{{"plan",plan},{"missions",snapM},{"paths",snapR}}}};
                        event(next,"PLAN_DEPLOYED","PLAN",planId,"Deployed. Execution has not started.",source);
                    }
                }
''' +s[b:]
s=s.replace('''                    const auto* value = request.if_contains("path");''','''                    auto& session=ownedSession(next,request,planId,missionId);
                    if(session.at("base_content_revision")!=plan.at("content_revision"))throw PlanError("VERSION_CONFLICT","Draft base revision changed");
                    session["state"]="SAVING";
                    const auto* value = request.if_contains("path");''')
s=s.replace('''                        empty?"Route cleared":"Route saved: " + str(mission,"name"),source);''','''                        empty?"Route cleared":"Route saved: " + str(mission,"name"),source);
                    next.at("edit_sessions").as_object().erase(str(request,"edit_session_id"));''')
s=s.replace('''            if (action != "validate" && action != "deploy") plan["status"] = "DRAFT";''','''            if (action != "validate" && action != "review" && action != "deploy" && !sessionAction) {
                plan["status"]="DRAFT";plan["content_revision"]=revision+1;plan["review"]=nullptr;plan.erase("validation");
            }''')
s=s.replace('''            plan["validation"] = validate(next, std::string(item.key()), uavs);''','''            if(str(plan,"status")=="DEPLOYED")continue;
            auto summary=validate(next,std::string(item.key()),uavs);
            if(!plan.contains("validation")) {summary["validated_content_revision"]=nullptr;summary["ready"]=false;plan["validation"]=summary;}''')
s=s.replace('''        return Object{{"state",state_},{"plan_id",planId},{"mission_id",missionId}};''','''        Object reply{{"state",state_},{"plan_id",planId},{"mission_id",missionId}};
        if(action=="begin_route_edit")for(const auto& e:state_.at("edit_sessions").as_object())if(str(e.value().as_object(),"plan_id")==planId)reply["edit_session_id"]=e.key();
        return reply;''')
s=s.replace('''private:
    static Object& lookup''','''    void heartbeat(const std::string& owner, const std::function<void(const Object&)>& publish) {
        std::lock_guard<std::mutex> lock(mutex_);auto next=state_;const auto now=OperationalContext::now();
        bool changed=expire(next,now);
        for(auto& e:next.at("edit_sessions").as_object()) {
            auto& session=e.value().as_object();if(str(session,"owner_instance_id")==owner) {
                session["last_seen_at"]=now;session["lease_expires_at"]=now+20;changed=true;
            }
        }
        if(changed){next["version"]=state_.at("version").to_number<int64_t>()+1;persist(next);state_=next;publish({{"type","SecurityPlansChanged"},{"payload",state_}});}
    }
private:
    static bool expire(Object& state,double now) {
        auto& sessions=state.at("edit_sessions").as_object();std::vector<std::string> expired;
        for(const auto& e:sessions)if(e.value().as_object().at("lease_expires_at").to_number<double>()<=now)expired.emplace_back(e.key());
        for(const auto& id:expired)sessions.erase(id);return !expired.empty();
    }
    static Object& ownedSession(Object& state,const Object& request,const std::string& plan,const std::string& mission) {
        auto& s=lookup(state.at("edit_sessions").as_object(),str(request,"edit_session_id"),"EDIT_SESSION_EXPIRED");
        if(str(s,"owner_instance_id")!=str(request,"instance_id") || str(s,"plan_id")!=plan || str(s,"mission_id")!=mission)
            throw PlanError("EDIT_SESSION_CONFLICT","Session owner or mission differs");return s;
    }
    static Object& lookup''')
s=s.replace('{"mission_id",mid},{"code",code}', '{"mission_id",mid},{"code",code},{"params",Object{}}')
s=s.replace('''        auto& list=s.at("events").as_array();''','''        Object params{{"target_id",target}};
        auto* p=s.at("plans").as_object().if_contains(target);if(p)params["plan_name"]=p->as_object().at("name");
        auto* m=s.at("missions").as_object().if_contains(target);if(m){params["mission_name"]=m->as_object().at("name");params["uav_id"]=m->as_object().at("assigned_uav_id");}
        auto& list=s.at("events").as_array();''')
s=s.replace('{"event_type",type},{"category",category}', '{"event_type",type},{"params",params},{"category",category}')
p.write_text(s,encoding='utf-8')
