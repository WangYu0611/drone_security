#pragma once
#include "execution/security_mission_adapter.h"
#include "conversion/mission_geodesy.h"
#include "drone/drone_manager.h"
#include <boost/json.hpp>

namespace security_mission {
// Transport-only bridge. All task lifecycle/waypoint/hover logic stays in the
// SecurityPlanStore mission controller. Construction never sends a command.
class DroneManagerBridge {
    DroneManager& manager_;boost::json::object fleet_;
    std::function<bool()> legacy_busy_;
    int id(const std::string& uid) const {return fleet_.at(uid).as_object().at("drone_id").to_number<int>();}
    NedFrame frame(const std::string& uid) const {
        const auto& cfg=fleet_.at(uid).as_object();const auto& a=cfg.at("frame_anchor").as_object();
        const auto ref=std::string(a.at("altitude_reference").as_string());
        const double offset=ref=="MSL"?a.at("geoid_undulation_m").to_number<double>():0.;
        if(ref!="Ellipsoid" && ref!="MSL")throw std::runtime_error("REAL_ANCHOR_DATUM_UNRESOLVED");
        NedFrame f{{a.at("latitude").to_number<double>(),a.at("longitude").to_number<double>(),a.at("altitude").to_number<double>()+offset},
            {a.at("ned_n").to_number<double>(),a.at("ned_e").to_number<double>(),a.at("ned_d").to_number<double>()}};
        validate(f.anchor);for(double v:f.anchor_ned)if(!std::isfinite(v))throw std::runtime_error("REAL_ANCHOR_INVALID");
        const auto live=manager_.GetAnchor(id(uid));
        if(!live.valid || distance(f.anchor,{live.latitude,live.longitude,live.altitude+offset})>.05)
            throw std::runtime_error("REAL_ANCHOR_CHANGED_RECALIBRATE");
        return f;
    }
    TelemetryData telemetry(const std::string& uid) const {
        const int drone=id(uid);TelemetryData sample;
        const auto status=manager_.GetStatus(drone);
        const auto expected="UAV-"+std::string(status.slot<10?"0":"")+std::to_string(status.slot);
        if(uid!=expected || manager_.GetConnectionState(drone)!=DroneConnectionState::Online || !manager_.TryGetFreshMissionTelemetry(drone,sample))
            throw std::runtime_error("REAL_TELEMETRY_UNAVAILABLE");
        if(legacy_busy_())throw std::runtime_error("LEGACY_CONTROLLER_BUSY");
        return sample;
    }
public:
    DroneManagerBridge(DroneManager& manager,boost::json::object fleet,std::function<bool()> busy)
        :manager_(manager),fleet_(std::move(fleet)),legacy_busy_(std::move(busy)){}
    Position observe(const std::string& uid) const {const auto t=telemetry(uid);return frame(uid).fromNed({t.position_ned[0],t.position_ned[1],t.position_ned[2]});}
    Position home(const std::string& uid) const {return frame(uid).anchor;}
    bool send(const Command& c) {
        const auto sample=telemetry(c.uav_id);
        if(!sample.IsArmed() || !sample.IsOffboard())return false;
        const auto target=frame(c.uav_id).toNed(c.target);
        return manager_.ProcessMoveCommandNed(id(c.uav_id),target[0],target[1],target[2],static_cast<float>(c.speed_mps));
    }
    bool hold(const std::string& uid){return manager_.AbortAndHoldAtCurrentPosition(id(uid));}
};
inline std::shared_ptr<Adapter> makeDroneManagerAdapter(DroneManager& manager,const boost::json::object& config,std::function<bool()> busy) {
    auto bridge=std::make_shared<DroneManagerBridge>(manager,config.at("uavs").as_object(),std::move(busy));
    const double arrival=config.contains("arrival_threshold_m")?config.at("arrival_threshold_m").to_number<double>():.25;
    return std::make_shared<RealAdapter>([bridge](const std::string& id){return bridge->observe(id);},[bridge](const std::string& id){return bridge->home(id);},
        [bridge](const Command& c){return bridge->send(c);},[bridge](const std::string& id){return bridge->hold(id);},arrival);
}
}
