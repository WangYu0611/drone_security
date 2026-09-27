// Included in SecurityPlanStore. Canonical route fields are reused; runtime
// trajectories are immutable derivatives, never written into editable paths.
    static Array assignment(const Object& o) {
        if(const auto* list=o.if_contains("assigned_uav_ids")) {
            if(!list->is_array() || list->as_array().size()>64)throw PlanError("INVALID_UAV","Expected at most 64 UAVs");
            for(const auto& v:list->as_array())if(!v.is_string() || v.as_string().empty())throw PlanError("INVALID_UAV","Invalid UAV identifier");
            return list->as_array();
        }
        const auto one=str(o,"assigned_uav_id");return one.empty()?Array{}:Array{one};
    }
    double safetySeparation() const {
        const auto* v=mock_.if_contains("safety_separation_m");return v?v->to_number<double>():1.5;
    }
    double requiredDistance(const std::string& a,const std::string& b) const {
        auto radius=[&](const std::string& id){const auto* u=mock_.at("uavs").as_object().if_contains(id);if(!u)return 0.;const auto* r=u->as_object().if_contains("radius_m");return r?r->to_number<double>():0.;};
        const auto* margin=mock_.if_contains("safety_margin_m");
        return std::max(safetySeparation(),radius(a)+radius(b)+(margin?margin->to_number<double>():0.));
    }
    static Object ellipsoidRoute(Object route) {
        for(auto& v:route.at("waypoints").as_array()) {
            auto& p=v.as_object();const auto ref=str(p,"altitude_reference");
            if(ref=="AGL")p["altitude"]=number(p,"altitude")+number(p,"terrain_ellipsoid_m");
            else if(ref=="MSL")p["altitude"]=number(p,"altitude")+number(p,"geoid_undulation_m");
            // Missing reference is legacy ellipsoid height, never silently AGL.
            if(!ref.empty())p["altitude_reference"]="Ellipsoid";
        }
        return route;
    }
    Object currentPosition(const Object& doc,const std::string& uid,bool observed=true) const {
        const auto* u=mock_.at("uavs").as_object().if_contains(uid);
        if(!u || (adapter_->simulation() && !u->as_object().contains("home_position")))throw PlanError(adapter_->simulation()?"MOCK_UAV_UNAVAILABLE":"REAL_UAV_UNAVAILABLE","No adapter position for "+uid);
        Object start;double latest=-1;
        for(const auto& entry:doc.at("executions").as_object()) {
            const auto& e=entry.value().as_object();
            if(str(e,"uav_id")==uid && str(e,"state")!="SCHEDULED" && number(e,"updated_at")>=latest){latest=number(e,"updated_at");start=e.at("position").as_object();}
        }
        if(latest<0)start=homePosition(uid);
        return adapter_->simulation()||!observed?start:adapterPosition(uid,geo(start));
    }
    Object formationRoutes(const Object& mission,const Object& input) const {
        checkPath(input);const auto base=ellipsoidRoute(input);const auto& points=base.at("waypoints").as_array();
        if(points.size()<2)throw PlanError("ROUTE_TOO_SHORT","At least two waypoints required");
        const auto members=assignment(mission);Object result;
        const double spacing=mission.contains("spacing_m")?number(mission,"spacing_m"):safetySeparation()*2.;
        const auto& p0=points[0].as_object();const auto& p1=points[1].as_object();
        constexpr double mPerDeg=6371000.*3.141592653589793/180.;
        const double cosLat=std::cos(number(p0,"latitude")*3.141592653589793/180.);
        double dx=std::remainder(number(p1,"longitude")-number(p0,"longitude"),360.)*mPerDeg*cosLat;
        double dy=(number(p1,"latitude")-number(p0,"latitude"))*mPerDeg;
        const double len=std::hypot(dx,dy);if(len<.01){dx=0;dy=1;}else{dx/=len;dy/=len;}
        if(members.size()>1 && std::abs(cosLat)<.1)throw PlanError("TRAJECTORY_DOMAIN_UNSUPPORTED","Formation requires a local nonpolar operating area");
        for(size_t i=0;i<members.size();++i) {
            const std::string uid(members[i].as_string());Object route=base;
            const double offset=(static_cast<double>(i)-(members.size()-1)*.5)*spacing;
            for(auto& v:route.at("waypoints").as_array()) {
                auto& p=v.as_object();
                if(members.size()>1 && distance(p0,p)>10000.)throw PlanError("TRAJECTORY_DOMAIN_UNSUPPORTED","Formation is limited to a 10 km local operating area");
                if(members.size()>1){
                    p["latitude"]=number(p,"latitude")-dx*offset/mPerDeg;
                    p["longitude"]=std::remainder(number(p,"longitude")+dy*offset/(mPerDeg*cosLat),360.);
                    p.erase("location"); // Geographic adapter resolves world coordinates at presentation.
                }
            }
            if(members.size()>1){route["formation"]="Line";route["slot_index"]=i;route["lateral_offset_m"]=offset;}
            result[uid]=route;
        }
        return result;
    }
    struct TrajectorySegment {double begin,end;Object from,to;};
    using Timeline=std::vector<TrajectorySegment>;
    Timeline timeline(const Object& start,const Object& route,double delay=0) const {
        Timeline out;double time=0;Object from=start;
        auto append=[&](const Object& to,double seconds){if(seconds>0){out.push_back({time,time+seconds,from,to});time+=seconds;}from=to;};
        append(start,delay);
        const auto& points=route.at("waypoints").as_array();
        for(const auto& v:points){const auto& p=v.as_object();const double speed=number(p,"segmentSpeed")>0?number(p,"segmentSpeed"):number(mock_,"default_speed_mps");append(position(p),distance(from,p)/speed);append(from,number(p,"waitTime"));}
        if(closedRoute(route)){const double speed=number(points.back().as_object(),"segmentSpeed");append(position(points.front().as_object()),distance(from,points.front().as_object())/(speed>0?speed:number(mock_,"default_speed_mps")));}
        append(from,86400.); // Occupied final position remains reserved through comparison horizon.
        return out;
    }
    struct Vec3 {double x,y,z;Vec3 operator-(Vec3 b)const{return{x-b.x,y-b.y,z-b.z};}Vec3 operator+(Vec3 b)const{return{x+b.x,y+b.y,z+b.z};}Vec3 operator*(double t)const{return{x*t,y*t,z*t};}double dot(Vec3 b)const{return x*b.x+y*b.y+z*b.z;}};
    static Vec3 localPosition(const Object& p,const Object& origin) {
        constexpr double scale=6371000.*3.141592653589793/180.;
        return {std::remainder(number(p,"longitude")-number(origin,"longitude"),360.)*scale*std::cos(number(origin,"latitude")*3.141592653589793/180.),(number(p,"latitude")-number(origin,"latitude"))*scale,number(p,"altitude")-number(origin,"altitude")};
    }
    static double minimumSeparation(const Timeline& a,const Timeline& b) {
        double minimum=std::numeric_limits<double>::infinity();size_t i=0,j=0;if(a.empty()||b.empty())return minimum;
        const auto origin=a.front().from;
        while(i<a.size() && j<b.size()) {
            const auto& x=a[i];const auto& y=b[j];const double begin=std::max(x.begin,y.begin),end=std::min(x.end,y.end);
            if(end>=begin){
                const auto vx=(localPosition(x.to,origin)-localPosition(x.from,origin))*(1./(x.end-x.begin));
                const auto vy=(localPosition(y.to,origin)-localPosition(y.from,origin))*(1./(y.end-y.begin));
                const auto rel=localPosition(x.from,origin)+vx*(begin-x.begin)-localPosition(y.from,origin)-vy*(begin-y.begin);
                const auto velocity=vx-vy;const double vv=velocity.dot(velocity);
                const double t=vv>1e-20?std::clamp(-rel.dot(velocity)/vv,0.,end-begin):0.;const auto nearest=rel+velocity*t;
                minimum=std::min(minimum,std::sqrt(nearest.dot(nearest)));
            }
            if(x.end<y.end)++i;else ++j;
        }
        return minimum;
    }
    void checkTrajectories(const Object& doc,const Object& routes) const {
        std::vector<std::pair<std::string,Timeline>> lines;
        for(const auto& entry:routes){const std::string uid(entry.key());lines.emplace_back(uid,timeline(currentPosition(doc,uid),entry.value().as_object()));}
        // Also reserve aircraft outside this assignment at their acknowledged position.
        for(const auto& entry:mock_.at("uavs").as_object())if(!routes.contains(entry.key())){
            const std::string uid(entry.key());const auto pos=currentPosition(doc,uid);Timeline reservation{{0.,172800.,pos,pos}};
            for(const auto& item:doc.at("executions").as_object()){const auto& e=item.value().as_object();
                if(str(e,"uav_id")!=uid || !executionActive(e) || str(e,"state")=="PAUSED" || str(e,"state")=="SCHEDULED")continue;
                auto route=e.at("route_snapshot").as_object();auto& points=route.at("waypoints").as_array();const auto first=points.front();
                if(str(e,"state")=="RETURNING"){auto home=e.at("home_position").as_object();home["segmentSpeed"]=0.;home["waitTime"]=0.;points=Array{home};route["bClosedLoop"]=false;route["closedRoute"]=false;}
                else {const auto count=std::min(points.size(),static_cast<size_t>(number(e,"completed_waypoints")));points.erase(points.begin(),points.begin()+count);
                    if(closedRoute(route) && !(e.contains("closure_completed") && e.at("closure_completed").as_bool())){auto closure=first.as_object();closure["waitTime"]=0.;points.push_back(closure);}
                    route["bClosedLoop"]=false;route["closedRoute"]=false;}
                reservation=timeline(pos,route,number(e,"wait_remaining"));break;
            }
            lines.push_back({uid,reservation});
        }
        for(size_t i=0;i<lines.size();++i)for(size_t j=i+1;j<lines.size();++j){
            if(!routes.contains(lines[i].first) && !routes.contains(lines[j].first))continue;
            const double required=requiredDistance(lines[i].first,lines[j].first),d=minimumSeparation(lines[i].second,lines[j].second);
            if(d+1e-6<required)throw PlanError("TRAJECTORY_CONFLICT","Minimum predicted separation is below the required distance",{{"uav_a",lines[i].first},{"uav_b",lines[j].first},{"minimum_separation_m",d},{"required_separation_m",required}});
        }
    }

    static double parseScheduledTime(const std::string& text) {
        if(text.size()!=20 || text.back()!='Z')throw PlanError("INVALID_SCHEDULE","Use UTC ISO8601 YYYY-MM-DDTHH:MM:SSZ");
        std::tm tm{};std::istringstream in(text);in>>std::get_time(&tm,"%Y-%m-%dT%H:%M:%SZ");
        if(in.fail())throw PlanError("INVALID_SCHEDULE","Invalid UTC date/time");
        const auto original=tm;
#ifdef _WIN32
        const auto epoch=_mkgmtime(&tm);
#else
        const auto epoch=timegm(&tm);
#endif
        if(epoch<0 || tm.tm_year!=original.tm_year || tm.tm_mon!=original.tm_mon || tm.tm_mday!=original.tm_mday || tm.tm_hour!=original.tm_hour || tm.tm_min!=original.tm_min || tm.tm_sec!=original.tm_sec)
            throw PlanError("INVALID_SCHEDULE","Invalid calendar date/time");
        return static_cast<double>(epoch);
    }
