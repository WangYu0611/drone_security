#pragma once
#include "storage/operational_context.h"
#include <set>
#include <vector>
#include <cmath>

struct PlanError : std::runtime_error {
    std::string code;
    boost::json::object params;
    PlanError(std::string c, std::string message) : std::runtime_error(message), code(std::move(c)) {}
    PlanError(std::string c, std::string message, boost::json::object p) : std::runtime_error(message), code(std::move(c)), params(std::move(p)) {}
};

// Backend authority. Paths use the existing DronePathSaveData JSON contract with
// additive WGS84 metadata; there is no execution-engine call in this store.
class SecurityPlanStore {
    using Object = boost::json::object;
    using Array = boost::json::array;
public:
    explicit SecurityPlanStore(std::string path) : path_(std::move(path)) {
        state_ = {{"version", 0}, {"next_id", 1}, {"plans", Object{}},
            {"missions", Object{}}, {"paths", Object{}}, {"events", Array{}}, {"deployments", Object{}}, {"edit_sessions", Object{}}};
        if (!path_.empty() && std::filesystem::exists(path_)) {
            std::ifstream in(path_); std::string text((std::istreambuf_iterator<char>(in)), {});
            boost::json::parse_options options; options.numbers=boost::json::number_precision::precise;
            state_ = boost::json::parse(text, {}, options).as_object();
            for (auto key : {"plans", "missions", "paths"}) state_.at(key).as_object();
            state_.at("events").as_array();
        }
        if (!state_.contains("executions")) state_["executions"] = Object{};
        if (!state_.contains("execution_version")) state_["execution_version"] = 0;
        if (!state_.contains("execution_requests")) state_["execution_requests"] = Object{};
        if (!state_.contains("deployments")) state_["deployments"] = Object{};
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
    Object snapshot() const { std::lock_guard<std::mutex> lock(mutex_); return state_; }
    static std::string str(const Object& o, const char* key) {
        auto* v = o.if_contains(key); return v && v->is_string() ? std::string(v->as_string()) : "";
    }
    Object transact(const Object& request, const std::set<std::string>& uavs,
                    const std::string& source, const std::function<void(const Object&)>& publish) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto next = state_;
        expire(next, OperationalContext::now());
        const auto action = str(request, "action");
        if (action.rfind("execution_",0)==0) return executionRequest(request,uavs,source,publish);
        if (auto v = request.if_contains("expected_version"))
            if (!v->is_number() || v->to_number<int64_t>() != state_.at("version").to_number<int64_t>())
                throw PlanError("VERSION_CONFLICT", "Plan data changed; reload and retry");
        auto& plans = next.at("plans").as_object();
        auto& missions = next.at("missions").as_object();
        auto& paths = next.at("paths").as_object();
        std::string planId = str(request, "plan_id"), missionId = str(request, "mission_id");
        if (action == "create_plan") {
            const auto name = requiredName(request);
            planId = allocate(next, "plan-");
            plans[planId] = Object{{"id", planId}, {"name", name}, {"description", str(request,"description")},
                {"status", "DRAFT"}, {"content_revision",1}, {"review",nullptr}, {"version", 1}, {"created_at", OperationalContext::now()},
                {"updated_at", OperationalContext::now()}, {"mission_ids", Array{}}};
            event(next,"PLAN_CREATED","PLAN",planId,"Security plan created: " + name,source);
            // Optional additive workflow contract: legacy callers still create an empty plan.
            if (!str(request,"default_mission_name").empty()) {
                missionId=allocate(next,"mission-");
                missions[missionId]=Object{{"id",missionId},{"plan_id",planId},{"name",str(request,"default_mission_name")},
                    {"assigned_uav_id",nullptr},{"route_id",nullptr},{"status","DRAFT"}};
                plans.at(planId).as_object()["mission_ids"]=Array{missionId};
                plans.at(planId).as_object()["workflow_step"]="TaskConfig";
                event(next,"MISSION_CREATED","MISSION",missionId,"Default task created",source);
            }
        } else if(action=="open_plan") {
            lookup(plans,planId,"PLAN_NOT_FOUND");
            event(next,"PLAN_OPENED","PLAN",planId,"Security plan opened",source);
        } else if(action=="copy_plan") {
            const auto original=lookup(plans,planId,"PLAN_NOT_FOUND");
            const auto* allowDraft=request.if_contains("allow_draft");
            if(str(original,"status")!="DEPLOYED" && !(allowDraft && allowDraft->is_bool() && allowDraft->as_bool()))throw PlanError("PLAN_NOT_DEPLOYED","Copy requires deployed plan");
            const auto sourceId=planId; planId=allocate(next,"plan-");missionId.clear();
            auto copy=original; copy["id"]=planId; copy["status"]="DRAFT";copy["version"]=1;
            if(!str(request,"name").empty())copy["name"]=requiredName(request);
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
            copy["mission_ids"]=ids;copy["workflow_step"]="TaskConfig";copy.erase("workflow_mission_id");plans[planId]=copy;
            if(!ids.empty())missionId=std::string(ids[0].as_string());
            event(next,"PLAN_COPIED","PLAN",planId,"Plan copied as draft",source);
            event(next,"PLAN_VERSION_CREATED","PLAN",planId,"New plan version created",source);
        } else if(action=="delete_plan") {
            const auto plan=lookup(plans,planId,"PLAN_NOT_FOUND");
            if(str(plan,"status")!="DRAFT")throw PlanError("PLAN_DEPLOYED","Only drafts may be deleted");
            for(const auto& entry:next.at("edit_sessions").as_object())if(str(entry.value().as_object(),"plan_id")==planId)throw PlanError("PLAN_EDIT_IN_PROGRESS","Close route editing first");
            for(const auto& id:plan.at("mission_ids").as_array()){
                paths.erase(str(missions.at(id.as_string()).as_object(),"route_id"));missions.erase(id.as_string());
            }
            event(next,"PLAN_DELETED","PLAN",planId,"Draft deleted",source);plans.erase(planId);planId.clear();missionId.clear();
        } else if (action=="begin_plan_move" || action=="move_plan" || action=="cancel_plan_move") {
            movePlan(next,request,planId,source);
        } else {
            auto& plan = lookup(plans, planId, "PLAN_NOT_FOUND");
            if (str(plan,"status") == "DEPLOYED") throw PlanError("PLAN_DEPLOYED", "Deployed plan is read-only");
            auto& ids = plan.at("mission_ids").as_array();
            const auto revision=plan.at("content_revision").to_number<int64_t>();
            const bool sessionAction=action=="begin_route_edit" || action=="mark_route_dirty" || action=="discard_route_edit" || action=="finish_route_edit" || action=="set_workflow_step";
            if(action=="review" || action=="deploy") {
                for(const auto& entry:next.at("edit_sessions").as_object()) {
                    const auto& session=entry.value().as_object();
                    if(str(session,"plan_id")==planId)throw PlanError("PLAN_EDIT_IN_PROGRESS","Route editing in progress",
                        {{"mission_id",session.at("mission_id")},{"owner_client",session.at("owner_client_id")},{"state",session.at("state")}});
                }
            }
            if(action=="set_workflow_step") {
                const auto step=str(request,"workflow_step");
                if(step!="BasicInfo" && step!="TaskConfig")throw PlanError("INVALID_SELECTION","Invalid workflow step");
                for(const auto& e:next.at("edit_sessions").as_object())if(str(e.value().as_object(),"plan_id")==planId)throw PlanError("PLAN_EDIT_IN_PROGRESS","Finish editing first");
                plan["workflow_step"]=step;
            } else if(sessionAction) {
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
                            if(str(existing,"mode")!="MOVE" && str(existing,"owner_instance_id")==owner && str(existing,"mission_id")==missionId)
                                return Object{{"state",state_},{"edit_session_id",entry.key()},{"plan_id",planId},{"mission_id",missionId}};
                            throw PlanError("EDIT_SESSION_CONFLICT","Plan already has an editor");
                        }
                    }
                    const auto sid=allocate(next,"edit-");const auto now=OperationalContext::now();
                    sessions[sid]=Object{{"edit_session_id",sid},{"plan_id",planId},{"mission_id",missionId},{"owner_instance_id",owner},
                        {"owner_client_id",source},{"base_content_revision",revision},{"state","EDITING"},{"started_at",now},{"last_seen_at",now},{"lease_expires_at",now+20}};
                    const auto* workflow=request.if_contains("workflow");
                    if(workflow && workflow->is_bool() && workflow->as_bool()) {
                        plan["workflow_step"]="RouteEditing";
                        plan["workflow_mission_id"]=missionId;
                    }
                    event(next,"ROUTE_EDIT_STARTED","MISSION",missionId,"Route editing started",source);
                } else {
                    auto& session=ownedSession(next,request,planId,missionId);
                    if(str(session,"mode")=="MOVE")throw PlanError("EDIT_SESSION_CONFLICT","Finish plan movement first");
                    if(action=="mark_route_dirty")session["state"]="DIRTY";
                    else {
                        if(action=="finish_route_edit") {
                            if(str(session,"state")=="DIRTY")throw PlanError("PLAN_EDIT_IN_PROGRESS","Save changes before finishing");
                            const auto routeId=str(mission,"route_id");
                            auto* route=paths.if_contains(routeId);
                            if(!route || route->as_object().at("waypoints").as_array().size()<2)throw PlanError("ROUTE_TOO_SHORT","At least two waypoints required");
                            plan["workflow_step"]="PreDeployReview";
                            event(next,"ROUTE_EDIT_COMPLETED","MISSION",missionId,"Route editing completed",source);
                        } else plan["workflow_step"]="TaskConfig";
                        sessions.erase(str(session,"edit_session_id"));
                    }
                }
            } else if (action == "add_mission") {
                missionId = allocate(next,"mission-");
                missions[missionId] = Object{{"id",missionId},{"plan_id",planId},{"name",requiredName(request)},
                    {"assigned_uav_id",nullptr},{"route_id",nullptr},{"status","DRAFT"}};
                ids.emplace_back(missionId);
                event(next,"MISSION_CREATED","MISSION",missionId,"Mission created: " + requiredName(request),source);
            } else if (action == "update_plan") {
                plan["name"] = requiredName(request); plan["description"] = str(request,"description");
                event(next,"PLAN_UPDATED","PLAN",planId,"Security plan updated: " + str(plan,"name"),source);
            } else if (action == "validate" || action == "review" || action == "deploy") {
                auto result = validate(next, planId, uavs);
                if(action=="validate") {
                    result["validated_content_revision"]=revision;plan["validation"]=result;
                    plan["status"]=result.at("ready").as_bool()?"READY":"DRAFT";
                    if(!result.at("ready").as_bool())plan["review"]=nullptr;
                    event(next,"PLAN_VALIDATED","PLAN",planId,"Configuration checked",source);
                    if(result.at("ready").as_bool())event(next,"PLAN_REVIEW_READY","PLAN",planId,"Pre-deployment check ready",source);
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
                        const auto reviewRecord=review->as_object();const auto deployedIds=ids;
                        const auto deploymentId=allocate(next,"deployment-");
                        plan["status"]="DEPLOYED";plan["deployment_id"]=deploymentId;
                        Object snapM,snapR;
                        for(const auto& id:deployedIds) {
                            auto& m=missions.at(id.as_string()).as_object();m["status"]="DEPLOYED";snapM[id.as_string()]=m;
                            const auto rid=str(m,"route_id");if(!rid.empty())snapR[rid]=paths.at(rid);
                        }
                        next.at("deployments").as_object()[deploymentId]=Object{{"id",deploymentId},{"plan_id",planId},
                            {"content_revision",revision},{"deployed_at",OperationalContext::now()},{"deployed_by",source},
                            {"reviewed_by",reviewRecord.at("reviewed_by")},{"reviewed_at",reviewRecord.at("reviewed_at")},
                            {"snapshot",Object{{"plan",plan},{"missions",snapM},{"paths",snapR}}}};
                        event(next,"DEPLOYMENT_CREATED","PLAN",planId,"Deployment snapshot created",source);
                        event(next,"PLAN_DEPLOYED","PLAN",planId,"Deployed. Execution has not started.",source);
                    }
                }
            } else {
                auto& mission = lookup(missions, missionId,"MISSION_NOT_FOUND");
                if (str(mission,"plan_id") != planId) throw PlanError("MISSION_PLAN_MISMATCH","Mission does not belong to plan");
                if (action == "delete_mission") {
                    auto route = str(mission,"route_id"); if (!route.empty()) paths.erase(route);
                    missions.erase(missionId);
                    ids.erase(std::remove_if(ids.begin(), ids.end(), [&](const auto& v){return std::string(v.as_string()) == missionId;}),ids.end());
                    event(next,"MISSION_DELETED","MISSION",missionId,"Mission deleted",source);
                } else if (action == "rename_mission") {
                    mission["name"] = requiredName(request);
                    event(next,"MISSION_UPDATED","MISSION",missionId,"Mission renamed: " + str(mission,"name"),source);
                } else if (action == "assign") {
                    const auto uav = str(request,"assigned_uav_id");
                    if (!uav.empty() && !uavs.count(uav)) throw PlanError("INVALID_UAV","Unknown UAV: " + uav);
                    for (const auto& id : ids) {
                        const auto& other = missions.at(id.as_string()).as_object();
                        if (!uav.empty() && std::string(id.as_string()) != missionId && str(other,"assigned_uav_id") == uav)
                            throw PlanError("UAV_ALREADY_ASSIGNED",uav + " already assigned to " + str(other,"name"));
                    }
                    mission["assigned_uav_id"] = uav.empty() ? boost::json::value(nullptr) : boost::json::value(uav);
                    event(next,uav.empty()?"UAV_UNASSIGNED":"UAV_ASSIGNED","MISSION",missionId,
                        uav.empty()?"UAV unassigned":uav + " assigned to " + str(mission,"name"),source);
                    if(!uav.empty())event(next,"MISSION_CONFIGURED","MISSION",missionId,"Task configured",source);
                } else if (action == "save_route") {
                    auto& session=ownedSession(next,request,planId,missionId);
                    if(str(session,"mode")=="MOVE")throw PlanError("EDIT_SESSION_CONFLICT","Finish plan movement first");
                    if(session.at("base_content_revision")!=plan.at("content_revision"))throw PlanError("VERSION_CONFLICT","Draft base revision changed");
                    session["state"]="SAVING";
                    const auto* value = request.if_contains("path");
                    if (!value || !value->is_object()) throw PlanError("INVALID_ROUTE","path must be an object");
                    Object route = value->as_object(); checkPath(route);
                    route["bClosedLoop"]=closedRoute(route);route["closedRoute"]=closedRoute(route);
                    auto routeId = str(mission,"route_id"); const bool created = routeId.empty();
                    if (created) routeId = allocate(next,"route-");
                    route["route_id"] = routeId;
                    route["revision"]=created?1:(paths.at(routeId).as_object().contains("revision")?paths.at(routeId).as_object().at("revision").to_number<int64_t>()+1:1);
                    paths[routeId] = route; mission["route_id"] = routeId;
                    const bool empty = route.at("waypoints").as_array().empty();
                    event(next,empty?"ROUTE_CLEARED":created?"ROUTE_CREATED":"ROUTE_UPDATED","MISSION",missionId,
                        empty?"Route cleared":"Route saved: " + str(mission,"name"),source);
                    const auto* keep=request.if_contains("keep_editing");
                    if(keep && keep->is_bool() && keep->as_bool()) {
                        session["state"]="EDITING";
                        session["base_content_revision"]=revision+1;
                        event(next,"ROUTE_SAVED","MISSION",missionId,"Route draft saved",source);
                    } else next.at("edit_sessions").as_object().erase(str(request,"edit_session_id"));
                } else throw PlanError("UNKNOWN_ACTION","Unknown security plan action");
            }
            if (action != "validate" && action != "review" && action != "deploy" && !sessionAction) {
                plan["status"]="DRAFT";plan["content_revision"]=revision+1;plan["review"]=nullptr;plan.erase("validation");
            }
            if(!sessionAction)plan["version"] = plan.at("version").to_number<int64_t>() + 1;
            if(!sessionAction)plan["updated_at"] = OperationalContext::now();
        }
        // Summaries and mission readiness are always computed by Backend.
        for (auto& item : plans) {
            auto& plan = item.value().as_object();
            if(str(plan,"status")=="DEPLOYED")continue;
            auto summary=validate(next,std::string(item.key()),uavs);
            if(!plan.contains("validation")) {summary["validated_content_revision"]=nullptr;summary["ready"]=false;plan["validation"]=summary;}
        }
        next["version"] = state_.at("version").to_number<int64_t>() + 1;
        // Construct the event before touching durable state. A synchronous publisher
        // rejection restores the authoritative file and leaves the in-memory document unchanged.
        const Object notification{{"type","SecurityPlansChanged"},{"payload",next}};
        persist(next);
        try { publish(notification); }
        catch (...) { persist(state_); throw; }
        state_.swap(next);
        Object reply{{"state",state_},{"plan_id",planId},{"mission_id",missionId}};
        if(action=="begin_route_edit" || action=="begin_plan_move")for(const auto& e:state_.at("edit_sessions").as_object())if(str(e.value().as_object(),"plan_id")==planId)reply["edit_session_id"]=e.key();
        return reply;
    }
    void checkSelection(const std::string& planId, const std::string& missionId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (planId.empty()) { if (!missionId.empty()) throw PlanError("INVALID_SELECTION","Mission requires a plan"); return; }
        const auto& plans = state_.at("plans").as_object();
        if (!plans.contains(planId)) throw PlanError("PLAN_NOT_FOUND","Unknown plan");
        const auto& missions = state_.at("missions").as_object();
        if (!missionId.empty() && (!missions.contains(missionId) || str(missions.at(missionId).as_object(),"plan_id") != planId))
            throw PlanError("MISSION_PLAN_MISMATCH","Mission does not belong to plan");
    }
    void heartbeat(const std::string& owner, const std::function<void(const Object&)>& publish) {
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
#include "storage/plan_geometry.inl"
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
    static Object& lookup(Object& objects, const std::string& id, const char* error) {
        auto* v = objects.if_contains(id); if (!v) throw PlanError(error,"Unknown ID: " + id); return v->as_object();
    }
    static std::string requiredName(const Object& o) {
        auto s = str(o,"name");
        if (s.empty() || s.size()>160 || s.find_first_not_of(" \t\r\n")==std::string::npos)
            throw PlanError("INVALID_NAME","Name is required (maximum 160 bytes)"); return s;
    }
    static std::string allocate(Object& s, const char* prefix) {
        auto n = s.at("next_id").to_number<int64_t>(); s["next_id"] = n+1; return prefix+std::to_string(n);
    }
    static bool closedRoute(const Object& path) {
        const auto* flag=path.if_contains("closedRoute");
        if(!flag)flag=path.if_contains("bClosedLoop");
        return flag && flag->is_bool() && flag->as_bool();
    }
    static void checkPath(const Object& path) {
        auto* points = path.if_contains("waypoints");
        if (!points || !points->is_array() || points->as_array().size()>1000) throw PlanError("INVALID_ROUTE","Expected up to 1000 waypoints");
        if(auto* canonical=path.if_contains("closedRoute")) {
            if(!canonical->is_bool())throw PlanError("INVALID_ROUTE","closedRoute must be boolean");
            if(auto* legacy=path.if_contains("bClosedLoop"))if(*legacy!=*canonical)throw PlanError("INVALID_ROUTE","Route topology fields disagree");
        }
        if(auto* flag=path.if_contains("bClosedLoop")) {
            if(!flag->is_bool())throw PlanError("INVALID_ROUTE","bClosedLoop must be boolean");
            if(flag->as_bool() && points->as_array().size()<3)throw PlanError("CLOSED_ROUTE_TOO_SHORT","Closed route requires three waypoints");
        }
        if(closedRoute(path) && points->as_array().size()<3)throw PlanError("CLOSED_ROUTE_TOO_SHORT","Closed route requires three waypoints");
        int index=0;
        for (const auto& v : points->as_array()) {
            if (!v.is_object()) throw PlanError("INVALID_WAYPOINT","Waypoint must be an object");
            const auto& p=v.as_object();
            for (auto key : {"latitude","longitude","altitude","segmentSpeed","waitTime"}) {
                auto* n=p.if_contains(key);
                if (!n || !n->is_number() || !std::isfinite(n->to_number<double>())) throw PlanError("INVALID_WAYPOINT",std::string("Invalid ")+key);
            }
            if (std::abs(p.at("latitude").to_number<double>())>90 || std::abs(p.at("longitude").to_number<double>())>180 ||
                p.at("segmentSpeed").to_number<double>()<0 || p.at("waitTime").to_number<double>()<0)
                throw PlanError("INVALID_WAYPOINT","Waypoint values out of range");
            auto* seq=p.if_contains("sequence");
            if (!seq || !seq->is_number() || seq->to_number<double>() != ++index) throw PlanError("INVALID_WAYPOINT","Waypoint sequence must be contiguous from 1");
        }
    }
    static Object validate(Object& s, const std::string& id, const std::set<std::string>& uavs) {
        auto& plan=s.at("plans").as_object().at(id).as_object();
        Array issues; int assigned=0, routes=0; std::set<std::string> seen;
        const auto& ids=plan.at("mission_ids").as_array();
        auto issue=[&](const std::string& mid,const char* code){issues.emplace_back(Object{{"mission_id",mid},{"code",code},{"params",Object{}}});};
        if (ids.empty()) issue("","NO_MISSIONS");
        for (const auto& mid : ids) {
            auto& m=s.at("missions").as_object().at(mid.as_string()).as_object();
            const auto before=issues.size();
            const auto u=str(m,"assigned_uav_id"), r=str(m,"route_id");
            const std::string key(mid.as_string());
            if (u.empty()) issue(key,"NO_UAV_ASSIGNED");
            else if (!uavs.count(u)) issue(key,"INVALID_UAV");
            else { ++assigned; if (!seen.insert(u).second) issue(key,"UAV_ALREADY_ASSIGNED"); }
            auto* path=s.at("paths").as_object().if_contains(r);
            if (!path) issue(key,"ROUTE_MISSING");
            else {
                try { checkPath(path->as_object());
                    if(path->as_object().at("waypoints").as_array().size()<2) issue(key,"ROUTE_TOO_SHORT"); else ++routes;
                } catch (const PlanError&) { issue(key,"INVALID_WAYPOINT"); }
            }
            if (str(m,"status")!="DEPLOYED") m["status"] = issues.size()==before ? "READY" : "DRAFT";
        }
        return {{"ready",issues.empty()},{"issues",issues},{"mission_count",ids.size()},{"assigned_count",assigned},{"routes_ready",routes}};
    }
    static void event(Object& s,const char* type,const char* category,const std::string& target,const std::string& message,const std::string& source) {
        Object params{{"target_id",target}};
        auto* p=s.at("plans").as_object().if_contains(target);if(p)params["plan_name"]=p->as_object().at("name");
        auto* m=s.at("missions").as_object().if_contains(target);if(m){params["mission_name"]=m->as_object().at("name");params["uav_id"]=m->as_object().at("assigned_uav_id");}
        auto& list=s.at("events").as_array();
        list.emplace_back(Object{{"sequence",list.size()+1},{"timestamp",OperationalContext::now()},
            {"event_type",type},{"params",params},{"category",category},{"target_id",target},{"message",message},{"source",source}});
    }
    void persist(const Object& value) {
        if (path_.empty()) return;
        auto target=std::filesystem::path(path_); if(target.has_parent_path())std::filesystem::create_directories(target.parent_path());
        auto temp=path_+".tmp";
        {std::ofstream out(temp,std::ios::trunc);out<<boost::json::serialize(value);out.flush();if(!out)throw std::runtime_error("plan persistence failed");}
#ifdef _WIN32
        if(!MoveFileExA(temp.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("plan persistence replace failed");
#else
        std::filesystem::rename(temp,path_);
#endif
    }
    #include "storage/mock_execution.inl"
    std::string path_; mutable std::mutex mutex_; Object state_;
};
