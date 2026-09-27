#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace security_mission {
// Shared command/observation domain: WGS84 degrees, ellipsoid metres, m/s.
// Adapters never own a waypoint index, hover timer, schedule or mission state.
struct Position { double latitude=0,longitude=0,altitude=0; };
inline void validate(Position p) {
    if(!std::isfinite(p.latitude)||!std::isfinite(p.longitude)||!std::isfinite(p.altitude)||std::abs(p.latitude)>90||std::abs(p.longitude)>180)
        throw std::runtime_error("ADAPTER_POSITION_INVALID");
}
inline double distance(Position a,Position b) {
    constexpr double rad=3.141592653589793/180.,earth=6371000.;
    const double y=(b.latitude-a.latitude)*rad*earth;
    const double x=std::remainder(b.longitude-a.longitude,360.)*rad*earth*std::cos((a.latitude+b.latitude)*.5*rad);
    return std::hypot(std::hypot(x,y),b.altitude-a.altitude);
}
struct Command { std::string uav_id,execution_id; Position target; double speed_mps=0; };
struct Motion { Position position; double consumed_seconds=0; bool arrived=false; };
class Adapter {
public:
    virtual ~Adapter()=default;
    virtual bool simulation() const=0;
    virtual Position observe(const std::string&,Position persisted) const {return persisted;}
    virtual Position home(const std::string&,Position configured) const {return configured;}
    virtual Motion advance(const Command&,Position from,double dt)=0;
    virtual bool hold(const std::string&)=0;
};
class MockAdapter final:public Adapter {
public:
    bool simulation() const override{return true;}
    Motion advance(const Command& c,Position from,double dt) override {
        validate(from);validate(c.target);
        if(!std::isfinite(c.speed_mps)||c.speed_mps<=0||!std::isfinite(dt)||dt<0)throw std::runtime_error("ADAPTER_COMMAND_INVALID");
        const double length=distance(from,c.target),moved=std::min(length,c.speed_mps*dt),fraction=length>1e-8?moved/length:1.;
        Position p{from.latitude+(c.target.latitude-from.latitude)*fraction,
            std::remainder(from.longitude+std::remainder(c.target.longitude-from.longitude,360.)*fraction,360.),
            from.altitude+(c.target.altitude-from.altitude)*fraction};
        return {p,moved/c.speed_mps,fraction>=1.};
    }
    bool hold(const std::string&) override{return true;}
};
class RealAdapter final:public Adapter {
public:
    using Observe=std::function<Position(const std::string&)>;
    using Send=std::function<bool(const Command&)>;
    using Hold=std::function<bool(const std::string&)>;
    RealAdapter(Observe observe,Observe home,Send send,Hold hold,double arrival_m=.25)
        :observe_(std::move(observe)),home_(std::move(home)),send_(std::move(send)),hold_(std::move(hold)),arrival_(arrival_m) {
        if(!observe_||!home_||!send_||!hold_||!std::isfinite(arrival_)||arrival_<=0||arrival_>.5)throw std::runtime_error("REAL_ADAPTER_CONFIG_INVALID");
    }
    bool simulation() const override{return false;}
    Position observe(const std::string& id,Position) const override {auto p=observe_(id);validate(p);return p;}
    Position home(const std::string& id,Position) const override {auto p=home_(id);validate(p);return p;}
    Motion advance(const Command& c,Position,double dt) override {
        validate(c.target);auto p=observe(c.uav_id,{});
        if(!std::isfinite(c.speed_mps)||c.speed_mps<=0)throw std::runtime_error("ADAPTER_COMMAND_INVALID");
        const auto old=sent_.find(c.uav_id);
        if(old==sent_.end()||old->second.execution_id!=c.execution_id||distance(old->second.target,c.target)>.000001||old->second.speed_mps!=c.speed_mps){
            if(!send_(c))throw std::runtime_error("REAL_COMMAND_REJECTED");sent_[c.uav_id]=c;
        }
        // Command acceptance does not imply arrival. Only measured position advances
        // the common controller, at most one waypoint per observation/tick.
        return {p,dt,distance(p,c.target)<=arrival_};
    }
    bool hold(const std::string& id) override {sent_.erase(id);return hold_(id);}
private:
    Observe observe_,home_;Send send_;Hold hold_;double arrival_;
    std::unordered_map<std::string,Command> sent_;
};
}
