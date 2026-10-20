// Explicit Launcher transaction. Ordinary Backend hydration preserves history.
// Caller holds its received-telemetry guard until this transaction finishes.
    Object resetMockPositions(const Object& request,const std::set<std::string>& registered,
        const std::set<std::string>& received,const std::function<void(const Object&)>& publish) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!adapter_->simulation())throw PlanError("ADAPTER_MODE_MISMATCH","Mock reset cannot control real aircraft");
        if(!request.contains("simulation") || request.at("simulation")!=true)
            throw PlanError("SIMULATION_CONFIRMATION_REQUIRED","Explicit simulation confirmation required");
        const auto key=str(request,"request_id");
        if(key.empty() || key.size()>128)throw PlanError("REQUEST_ID_REQUIRED","Reset request id required");
        if(const auto* requests=state_.if_contains("mock_reset_requests"))if(const auto* previous=requests->as_object().if_contains(key)) {
            if(previous->as_object().at("request")!=request)throw PlanError("REQUEST_ID_REUSED","Reset request id has different content");
            return {{"state",state_},{"reset_epoch",previous->as_object().at("epoch")},{"replayed",true}};
        }
        const auto* input=request.if_contains("startup_formation");
        if(!input)input=mock_.if_contains("startup_formation");
        if(!input || !input->is_object())throw PlanError("INVALID_STARTUP_FORMATION","Startup formation is required");
        const auto& formation=input->as_object();
        if(str(formation,"direction")!="east" || !formation.contains("origin") ||
           !formation.at("origin").is_object() || !formation.contains("spacing_m") || !formation.at("spacing_m").is_number())
            throw PlanError("INVALID_STARTUP_FORMATION","Expected east formation, origin and spacing");
        const auto origin=formation.at("origin").as_object();
        for(const auto* field:{"latitude","longitude","altitude"})
            if(!origin.contains(field) || !origin.at(field).is_number())
                throw PlanError("INVALID_STARTUP_FORMATION","Complete numeric origin required");
        security_mission::Position center;
        try {center=geo(origin);security_mission::validate(center);}
        catch(const std::exception&){throw PlanError("INVALID_STARTUP_FORMATION","Invalid formation origin");}
        const double spacing=number(formation,"spacing_m");
        constexpr double mPerDeg=6371000.*3.141592653589793/180.;
        const double cosLat=std::cos(center.latitude*3.141592653589793/180.);
        if(!std::isfinite(spacing) || spacing<=0 || spacing>1000 || std::abs(cosLat)<.1)
            throw PlanError("INVALID_STARTUP_FORMATION","Invalid formation spacing or polar origin");

        std::set<std::string> protectedIds=received;
        std::map<std::string,const Object*> latest;
        for(const auto& item:state_.at("executions").as_object()) {
            const auto& e=item.value().as_object();const auto uid=str(e,"uav_id");
            if(executionActive(e) && (!registered.count(uid) || !mock_.at("uavs").as_object().contains(uid)))protectedIds.insert(uid);
            if(str(e,"state")!="SCHEDULED" && (!latest.count(uid) || number(e,"updated_at")>=number(*latest.at(uid),"updated_at")))
                latest[uid]=&e;
            if(executionActive(e) && (!e.contains("simulation") || e.at("simulation")!=true))protectedIds.insert(uid);
        }
        for(const auto& item:latest)if(!item.second->contains("simulation") || item.second->at("simulation")!=true)protectedIds.insert(item.first);
        // Never terminate or reset only part of an active mixed/protected group.
        bool expanded=true;
        while(expanded) {
            expanded=false;std::set<std::string> groups;
            for(const auto& item:state_.at("executions").as_object()) {
                const auto& e=item.value().as_object();
                if(executionActive(e) && protectedIds.count(str(e,"uav_id")) && !str(e,"group_id").empty())groups.insert(str(e,"group_id"));
            }
            for(const auto& item:state_.at("executions").as_object()) {
                const auto& e=item.value().as_object();
                if(executionActive(e) && groups.count(str(e,"group_id")))expanded=protectedIds.insert(str(e,"uav_id")).second || expanded;
            }
        }
        std::vector<std::pair<unsigned long,std::string>> ordered;
        for(const auto& item:mock_.at("uavs").as_object()) {
            const std::string uid(item.key());if(!registered.count(uid))continue;
            if(uid.rfind("UAV-",0)!=0 || uid.size()<=4 ||
               uid.find_first_not_of("0123456789",4)!=std::string::npos)
                throw PlanError("INVALID_UAV","Startup formation requires numbered UAV identifiers");
            ordered.emplace_back(std::stoul(uid.substr(4)),uid);
        }
        std::sort(ordered.begin(),ordered.end());
        if(ordered.size()>64)throw PlanError("INVALID_UAV","Startup formation supports at most 64 aircraft");
        Object positions;Array skipped;
        for(size_t index=0;index<ordered.size();++index) {
            const auto& uid=ordered[index].second;
            if(protectedIds.count(uid)){skipped.emplace_back(uid);continue;}
            auto point=center;point.longitude+=spacing*index/(mPerDeg*cosLat);
            try{security_mission::validate(point);}catch(const std::exception&){throw PlanError("INVALID_STARTUP_FORMATION","Formation exceeds coordinate domain");}
            positions[uid]=jsonPosition(point);
        }
        for(const auto& a:positions)for(const auto& b:positions)if(a.key()<b.key() &&
            distance(a.value().as_object(),b.value().as_object())<requiredDistance(std::string(a.key()),std::string(b.key())))
            throw PlanError("INVALID_STARTUP_FORMATION","Formation violates aircraft separation");
        const int64_t epoch=state_.contains("mock_position_baseline")?
            state_.at("mock_position_baseline").as_object().at("epoch").to_number<int64_t>()+1:1;
        auto next=state_;const double now=OperationalContext::now();
        for(auto& item:next.at("executions").as_object()) {
            auto& e=item.value().as_object();
            if(!positions.contains(str(e,"uav_id")) || !executionActive(e) ||
               !e.contains("simulation") || e.at("simulation")!=true)continue;
            const bool scheduled=str(e,"state")=="SCHEDULED";
            e["failure_reason"]="MOCK_STARTUP_RESET";e["completed_at"]=now;
            transition(next,e,scheduled?"CANCELLED":"ABORTED",scheduled?"MISSION_CANCELLED":"MISSION_ABORTED");
        }
        next["mock_position_baseline"]=Object{{"epoch",epoch},{"reset_at",now},{"request_id",key},
            {"positions",positions},{"startup_formation",formation},{"skipped_uav_ids",skipped}};
        if(!next.contains("mock_reset_requests"))next["mock_reset_requests"]=Object{};
        next.at("mock_reset_requests").as_object()[key]=Object{{"request",request},{"epoch",epoch}};
        event(next,"MOCK_STARTUP_RESET","SYSTEM","","Mock aircraft returned to startup formation","Launcher");
        // Preserve the complete pre-reset document before the authoritative commit.
        // An orphan backup after a failed publish is safe and never overwritten.
        if(!path_.empty()) {
            const auto folder=std::filesystem::path(path_).parent_path()/"mock-reset-history";
            std::filesystem::create_directories(folder);
            const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            const auto backup=folder/("before-"+std::to_string(epoch)+"-"+std::to_string(stamp)+".json");
            const auto temp=backup.string()+".tmp";
            {std::ofstream out(temp,std::ios::binary|std::ios::trunc);
             if(!out)throw std::runtime_error("MOCK_RESET_BACKUP_FAILED");
             out<<boost::json::serialize(state_);out.flush();
             if(!out)throw std::runtime_error("MOCK_RESET_BACKUP_FAILED");}
            std::filesystem::rename(temp,backup);
        }
        commitExecutions(next,publish);
        return {{"state",state_},{"reset_epoch",epoch},{"positions",positions},{"skipped_uav_ids",skipped},{"replayed",false}};
    }
