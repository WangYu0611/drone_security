from pathlib import Path
p=Path('Backend/http/http_server.h');s=p.read_text(encoding='utf-8-sig')
s=s.replace('#include "storage/security_plan_store.h"','#include "storage/security_plan_store.h"\n#include "storage/shared_view_store.h"')
s=s.replace('SecurityPlanStore security_plans_;','SecurityPlanStore security_plans_;\n    UIPreferenceStore ui_preferences_;\n    VideoViewStore video_view_;\n    std::thread plan_lease_thread_;\n    boost::json::value ViewRequest(const boost::json::object& body,bool language);')
p.write_text(s,encoding='utf-8')
p=Path('Backend/http/http_server.cpp');s=p.read_text(encoding='utf-8-sig')
s=s.replace('    , security_plans_((std::filesystem::path(config.storage_path).parent_path() / "security_plans.json").string())','''    , security_plans_((std::filesystem::path(config.storage_path).parent_path() / "security_plans.json").string())
    , ui_preferences_((std::filesystem::path(config.storage_path).parent_path() / "ui_preferences.json").string())
    , video_view_((std::filesystem::path(config.storage_path).parent_path() / "video_view.json").string())''')
s=s.replace('    running_ = true;','''    running_ = true;
    plan_lease_thread_=std::thread([this]{while(running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        try {security_plans_.heartbeat("",[this](const boost::json::object& e){ws_manager_.broadcast(json_stringify(e));});}
        catch(const std::exception& e){spdlog::error("P4 lease maintenance failed: {}",e.what());}
    }});''')
s=s.replace('    ws_thread_.join();','    ws_thread_.join();\n    if(plan_lease_thread_.joinable())plan_lease_thread_.join();')
s=s.replace('''        if (method == "GET" && path == "/api/security-plans")''','''        if(path=="/api/ui-preferences" || path=="/api/video-view") {
            const bool language=path=="/api/ui-preferences";
            if(method=="GET")return MakeResponse(req,200,json_stringify(language?ui_preferences_.snapshot():video_view_.snapshot()));
            if(method=="PATCH") {
                try {return MakeResponse(req,200,json_stringify(ViewRequest(require_object(body,"body"),language)));}
                catch(const PlanError& e){return MakeResponse(req,400,json_stringify(boost::json::object{{"code",e.code},{"params",e.params},{"message",e.what()}}));}
            }
        }
        if (method == "GET" && path == "/api/security-plans")''')
s=s.replace('{"code",e.code},{"message",e.what()}', '{"code",e.code},{"params",e.params},{"message",e.what()}')
s=s.replace('''        ws_manager_.send(session, "{\\"type\\":\\"context_pong\\"}"); return true;''','''        std::string owner;
        {std::lock_guard<std::mutex> lock(context_clients_mutex_);for(const auto& e:context_sessions_)if(e.second.lock()==session){owner=e.first;break;}}
        if(!owner.empty())security_plans_.heartbeat(owner,[this](const boost::json::object& e){ws_manager_.broadcast(json_stringify(e));});
        ws_manager_.send(session, "{\\"type\\":\\"context_pong\\"}"); return true;''')
s=s.replace('''    if(action=="deploy" && role!="Command") throw ApiError(403,"Only Command may deploy");
    if(action!="deploy" && action!="select" && action!="validate" && role!="Map") throw ApiError(403,"Use Main Map Plan Editor");''','''    const std::set<std::string> commandActions{"create_plan","update_plan","add_mission","rename_mission","delete_mission","assign","validate","review","deploy","copy_plan","select","request_map_route_edit"};
    const std::set<std::string> mapActions{"select","validate","begin_route_edit","mark_route_dirty","save_route","discard_route_edit"};
    if(!(role=="Command"?commandActions:mapActions).count(action))throw ApiError(403,"Role cannot perform this plan action");
    if(action=="request_map_route_edit") {
        const auto plan=get_string(body,"plan_id"),mission=get_string(body,"mission_id");security_plans_.checkSelection(plan,mission);
        if(mission.empty())throw PlanError("MISSION_NOT_FOUND","Mission required");
        const auto snapshot=security_plans_.snapshot();
        if(SecurityPlanStore::str(snapshot.at("plans").as_object().at(plan).as_object(),"status")=="DEPLOYED")throw PlanError("PLAN_DEPLOYED","Deployed plan is read-only");
        bool online=false;{std::lock_guard<std::mutex> lock(context_clients_mutex_);for(const auto& e:context_clients_)if(e.second.at("client_role")=="Map" && e.second.at("state")=="ONLINE")online=true;}
        if(!online)throw PlanError("MAP_CLIENT_UNAVAILABLE","Map client unavailable");
        static std::atomic<uint64_t> serial{0};
        boost::json::object payload{{"request_id","map-edit-"+std::to_string(++serial)},{"plan_id",plan},{"mission_id",mission},{"requested_by",source},{"timestamp",OperationalContext::now()}};
        ws_manager_.broadcast(json_stringify(boost::json::object{{"type","MapRouteEditRequested"},{"payload",payload}}));
        return boost::json::object{{"request",payload},{"state",snapshot}};
    }''')
s=s.replace('if(action=="create_plan" || action=="add_mission")','if(action=="create_plan" || action=="add_mission" || action=="copy_plan")')
s+='''
boost::json::value HttpServer::ViewRequest(const boost::json::object& body,bool language) {
    std::string source;
    {std::lock_guard<std::mutex> lock(context_clients_mutex_);auto it=context_clients_.find(get_string(body,"instance_id"));
        if(it==context_clients_.end() || it->second.at("state")!="ONLINE")throw ApiError(409,"client not subscribed");source=get_string(it->second,"client_id");}
    std::lock_guard<std::mutex> records(records_mutex_);std::set<std::string> uavs;
    for(const auto& d:drone_records_)if(d.slot>0)uavs.insert("UAV-"+std::string(d.slot<10?"0":"")+std::to_string(d.slot));
    auto publish=[this](const boost::json::object& e){ws_manager_.broadcast(json_stringify(e));};
    return language?ui_preferences_.update(body,uavs,source,publish):video_view_.update(body,uavs,source,publish);
}
'''
p.write_text(s,encoding='utf-8')
p=Path('Backend/storage/security_plan_store.h');s=p.read_text();s=s.replace('#include <set>','#include <set>\n#include <vector>')
s=s.replace('const auto deploymentId=allocate(next,"deployment-");','const auto reviewRecord=review->as_object();\n                        const auto deploymentId=allocate(next,"deployment-");')
s=s.replace('{"reviewed_by",review->as_object().at("reviewed_by")},{"reviewed_at",review->as_object().at("reviewed_at")}', '{"reviewed_by",reviewRecord.at("reviewed_by")},{"reviewed_at",reviewRecord.at("reviewed_at")}')
p.write_text(s)
