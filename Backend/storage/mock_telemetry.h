#pragma once
#include <boost/json.hpp>

namespace mock_telemetry {
// REST hydration and WS refresh share this projection. Connection status, task
// state and battery stay authoritative: a synthetic GPS point is not a link.
inline bool project(boost::json::object& message, const boost::json::value* entry,
                    bool received_telemetry) {
    if(received_telemetry || !entry)return false;
    const auto& item=entry->as_object();const auto& p=item.at("position").as_object();
    message["simulation"]=true;message["telemetry_source"]=item.at("telemetry_source");
    message["mock_position"]=p;
    if(const auto* epoch=item.if_contains("mock_epoch"))message["mock_epoch"]=*epoch;
    message["gps_lat"]=p.at("latitude");message["gps_lon"]=p.at("longitude");
    message["gps_alt"]=p.at("altitude");message["gps_fix"]=true;
    message["local_position_valid"]=false;message["anchor_valid"]=false;
    return true;
}
}
