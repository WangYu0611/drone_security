    // Called inside the same store transaction as the mutation. Historical
    // deployments and executions are immutable; only the editable plan changes.
    static void checkPlanLifecycle(const Object& state,const Object& request,const Object& plan) {
        if(!request.if_contains("expected_version"))throw PlanError("VERSION_CONFLICT","Current plan version is required");
        const auto* revision=request.if_contains("expected_content_revision");
        if(!revision || !revision->is_number() || revision->to_number<int64_t>()!=plan.at("content_revision").to_number<int64_t>())
            throw PlanError("CONTENT_REVISION_CONFLICT","Plan contents changed; reopen the action and confirm again");
        const auto id=str(plan,"id");
        for(const auto& entry:state.at("edit_sessions").as_object())
            if(str(entry.value().as_object(),"plan_id")==id)throw PlanError("PLAN_EDIT_IN_PROGRESS","Finish the active route edit or plan movement first");
        checkPlanNotExecuting(state,id);
    }
    static void checkPlanNotExecuting(const Object& state,const std::string& id) {
        for(const auto& entry:state.at("executions").as_object()) {
            const auto& execution=entry.value().as_object();if(str(execution,"plan_id")!=id)continue;
            const auto status=str(execution,"state");
            // Unknown/nonterminal states fail closed, including a future schedule.
            if(status!="COMPLETED" && status!="ABORTED" && status!="CANCELLED" && status!="FAILED")
                throw PlanError("PLAN_EXECUTING","Finish or cancel this plan's active or scheduled executions first",
                    {{"execution_id",entry.key()},{"state",status}});
        }
    }
    static void reopenPlan(Object& state,const Object& request,const std::string& id,const std::string& source) {
        auto& plan=lookup(state.at("plans").as_object(),id,"PLAN_NOT_FOUND");
        const auto* lease=activeConfiguration(state);
        if(!lease)throw PlanError("CONFIGURATION_REQUIRED","Acquire this plan's configuration lease first");
        if(str(*lease,"owner_instance_id")!=str(request,"instance_id") || str(*lease,"plan_id")!=id)configurationConflict(*lease);
        checkPlanLifecycle(state,request,plan);
        if(str(plan,"status")!="DEPLOYED")throw PlanError("PLAN_NOT_DEPLOYED","Only a deployed plan needs to be reopened");
        // Retain the plan, task and route IDs. The previous deployment remains in
        // deployments with its own full snapshot and can still back execution history.
        plan["previous_deployment_id"]=str(plan,"deployment_id");
        plan.erase("deployment_id");plan["deployment"]=nullptr;plan["review"]=nullptr;plan.erase("validation");
        plan["status"]="DRAFT";plan["workflow_step"]="TaskConfig";
        plan["content_revision"]=plan.at("content_revision").to_number<int64_t>()+1;
        plan["version"]=plan.at("version").to_number<int64_t>()+1;plan["updated_at"]=OperationalContext::now();
        for(const auto& mid:plan.at("mission_ids").as_array())state.at("missions").as_object().at(mid.as_string()).as_object()["status"]="DRAFT";
        event(state,"PLAN_REOPENED","PLAN",id,"Existing plan reopened for editing",source);
    }
    static void deletePlan(Object& state,const Object& request,const std::string& id,const std::string& source) {
        auto& plans=state.at("plans").as_object();const auto plan=lookup(plans,id,"PLAN_NOT_FOUND");
        if(str(request,"confirmation_name")!=str(plan,"name"))throw PlanError("DELETE_CONFIRMATION_REQUIRED","Enter the exact plan name to confirm deletion");
        checkPlanLifecycle(state,request,plan);
        auto& missions=state.at("missions").as_object();auto& paths=state.at("paths").as_object();
        Object savedMissions,savedPaths;std::set<std::string> routeIds;
        for(const auto& mid:plan.at("mission_ids").as_array()) {
            const auto* value=missions.if_contains(mid.as_string());if(!value)continue;
            const auto& mission=value->as_object();
            if(str(mission,"plan_id")!=id)throw PlanError("MISSION_PLAN_MISMATCH","Plan task membership changed");
            savedMissions[mid.as_string()]=mission;const auto route=str(mission,"route_id");
            if(const auto* path=paths.if_contains(route)){savedPaths[route]=*path;routeIds.insert(route);}
        }
        state.at("deleted_plans").as_object()[id]=Object{{"deleted_at",OperationalContext::now()},{"deleted_by",source},
            {"snapshot",Object{{"plan",plan},{"missions",savedMissions},{"paths",savedPaths}}}};
        for(const auto& mid:plan.at("mission_ids").as_array())missions.erase(mid.as_string());
        for(const auto& route:routeIds) {
            bool referenced=false;for(const auto& entry:missions)referenced|=str(entry.value().as_object(),"route_id")==route;
            if(!referenced)paths.erase(route);
        }
        event(state,"PLAN_DELETED","PLAN",id,"Plan removed from workspace; historical snapshots retained",source);plans.erase(id);
    }
