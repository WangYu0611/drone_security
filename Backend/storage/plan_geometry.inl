// Included in SecurityPlanStore; uses the same transaction, lease and persistence.
    static void movePlan(Object& doc,const Object& r,const std::string& pid,const std::string& source) {
        auto& plan=lookup(doc.at("plans").as_object(),pid,"PLAN_NOT_FOUND");
        const auto action=str(r,"action"),owner=str(r,"instance_id");
        auto& sessions=doc.at("edit_sessions").as_object();
        if(owner.empty())throw PlanError("INVALID_IDENTITY","Move owner required");
        if(action!="cancel_plan_move")for(const auto& item:doc.at("executions").as_object()) {
            const auto& e=item.value().as_object();
            if(str(e,"plan_id")==pid && executionActive(e))throw PlanError("PLAN_EXECUTING","Cannot move the plan while the mission is running.");
        }
        const auto revision=plan.at("content_revision").to_number<int64_t>();
        if(action=="begin_plan_move") {
            for(const auto& item:sessions)if(str(item.value().as_object(),"plan_id")==pid)throw PlanError("PLAN_EDIT_IN_PROGRESS","Finish current editing first");
            bool hasPoints=false;
            for(const auto& mid:plan.at("mission_ids").as_array()) {
                const auto rid=str(doc.at("missions").as_object().at(mid.as_string()).as_object(),"route_id");
                auto* route=doc.at("paths").as_object().if_contains(rid);
                if(route && !route->as_object().at("waypoints").as_array().empty())hasPoints=true;
            }
            if(!hasPoints)throw PlanError("ROUTE_TOO_SHORT","No waypoints to move");
            const auto sid=allocate(doc,"move-");const auto now=OperationalContext::now();
            sessions[sid]=Object{{"edit_session_id",sid},{"plan_id",pid},{"mission_id",str(r,"mission_id")},
                {"owner_instance_id",owner},{"owner_client_id",source},{"mode","MOVE"},{"state","EDITING"},
                {"base_content_revision",revision},{"started_at",now},{"last_seen_at",now},{"lease_expires_at",now+20}};
            return;
        }
        auto& session=ownedSession(doc,r,pid,str(r,"mission_id"));
        if(str(session,"mode")!="MOVE")throw PlanError("EDIT_SESSION_CONFLICT","Not a move session");
        if(action=="cancel_plan_move"){sessions.erase(str(r,"edit_session_id"));return;}
        if(session.at("base_content_revision")!=plan.at("content_revision"))throw PlanError("VERSION_CONFLICT","Plan changed during preview");
        const auto* value=r.if_contains("paths");
        if(!value || !value->is_object())throw PlanError("INVALID_ROUTE","All plan routes required");
        const auto& incoming=value->as_object();auto& paths=doc.at("paths").as_object();
        size_t count=0;bool hasDelta=false;double delta[3]{};
        for(const auto& mid:plan.at("mission_ids").as_array()) {
            const auto rid=str(doc.at("missions").as_object().at(mid.as_string()).as_object(),"route_id");
            if(rid.empty())continue;++count;
            auto* replacement=incoming.if_contains(rid);
            if(!replacement || !replacement->is_object())throw PlanError("INVALID_ROUTE","Missing plan route");
            const auto& old=paths.at(rid).as_object();auto next=replacement->as_object();checkPath(next);
            const auto& before=old.at("waypoints").as_array();const auto& after=next.at("waypoints").as_array();
            if(before.size()!=after.size() || closedRoute(old)!=closedRoute(next))throw PlanError("INVALID_TRANSLATION","Translation cannot change topology");
            for(size_t i=0;i<before.size();++i) {
                const auto& a=before[i].as_object();const auto& b=after[i].as_object();
                if(a.at("segmentSpeed")!=b.at("segmentSpeed") || a.at("waitTime")!=b.at("waitTime"))throw PlanError("INVALID_TRANSLATION","Translation cannot change route parameters");
                // The Map produces both representations through ICoordinateService.
                // Validate a common UE translation across every route, never degree offsets.
                for(int axis=0;axis<3;++axis) {
                    const auto key=axis==0?"x":axis==1?"y":"z";
                    const auto* al=a.if_contains("location");const auto* bl=b.if_contains("location");
                    if(!al || !bl || !al->is_object() || !bl->is_object())throw PlanError("COORDINATES_NOT_READY","World coordinates required");
                    const auto* av=al->as_object().if_contains(key);const auto* bv=bl->as_object().if_contains(key);
                    if(!av || !bv || !av->is_number() || !bv->is_number())throw PlanError("INVALID_TRANSLATION","Invalid world coordinates");
                    const double d=bv->to_number<double>()-av->to_number<double>();
                    if(!std::isfinite(d) || (hasDelta && std::abs(d-delta[axis])>.1))throw PlanError("INVALID_TRANSLATION","All points must have one translation");
                    if(!hasDelta)delta[axis]=d;
                }
                hasDelta=true;
            }
            // Preserve route metadata; only coordinates change.
            auto updated=old;updated["waypoints"]=after;
            updated["revision"]=old.contains("revision")?old.at("revision").to_number<int64_t>()+1:1;
            paths[rid]=updated;
        }
        if(incoming.size()!=count)throw PlanError("INVALID_ROUTE","Unexpected route");
        plan["content_revision"]=revision+1;plan["version"]=plan.at("version").to_number<int64_t>()+1;
        plan["updated_at"]=OperationalContext::now();plan["review"]=nullptr;plan.erase("validation");
        if(str(plan,"status")=="DEPLOYED") {
            // Keep historical deployment and execution snapshots immutable.
            const auto previous=str(plan,"deployment_id");const auto id=allocate(doc,"deployment-");
            auto deployment=doc.at("deployments").as_object().at(previous).as_object();
            plan["deployment_id"]=id;deployment["id"]=id;deployment["previous_deployment_id"]=previous;
            deployment["content_revision"]=revision+1;deployment["deployed_at"]=OperationalContext::now();deployment["deployed_by"]=source;
            Object routes,missions;
            for(const auto& mid:plan.at("mission_ids").as_array()) {
                const auto& m=doc.at("missions").as_object().at(mid.as_string());missions[mid.as_string()]=m;
                const auto rid=str(m.as_object(),"route_id");if(!rid.empty())routes[rid]=paths.at(rid);
            }
            deployment["snapshot"]=Object{{"plan",plan},{"missions",missions},{"paths",routes}};
            doc.at("deployments").as_object()[id]=deployment;
        } else plan["status"]="DRAFT";
        sessions.erase(str(r,"edit_session_id"));event(doc,"PLAN_TRANSLATED","PLAN",pid,"Plan position confirmed",source);
    }
