#pragma once
#include "execution/security_mission_adapter.h"
#include <array>
namespace security_mission {
using Vector=std::array<double,3>;
inline Vector ecef(Position p){validate(p);constexpr double r=3.141592653589793/180.,a=6378137.,e2=6.6943799901413165e-3;
    const double lat=p.latitude*r,lon=p.longitude*r,n=a/std::sqrt(1-e2*std::sin(lat)*std::sin(lat));
    return {(n+p.altitude)*std::cos(lat)*std::cos(lon),(n+p.altitude)*std::cos(lat)*std::sin(lon),(n*(1-e2)+p.altitude)*std::sin(lat)};}
inline Position geographic(Vector p){constexpr double r=180./3.141592653589793,a=6378137.,e2=6.6943799901413165e-3;
    const double radial=std::hypot(p[0],p[1]);if(radial<1.)throw std::runtime_error("POLAR_FRAME_UNSUPPORTED");
    double lat=std::atan2(p[2],radial*(1-e2)),h=0;
    for(int i=0;i<12;++i){const double n=a/std::sqrt(1-e2*std::sin(lat)*std::sin(lat));h=radial/std::cos(lat)-n;lat=std::atan2(p[2],radial*(1-e2*n/(n+h)));}
    const double n=a/std::sqrt(1-e2*std::sin(lat)*std::sin(lat));h=radial/std::cos(lat)-n;
    Position result{lat*r,std::atan2(p[1],p[0])*r,h};validate(result);return result;}
// Calibrated point and its measured NED coordinates are explicit. Never assume
// the first received GPS sample coincides with PX4's local (0,0,0).
struct NedFrame {
    Position anchor;Vector anchor_ned;
    Vector toNed(Position target) const {
        if(std::abs(anchor.latitude)>80||distance(anchor,target)>10000.)throw std::runtime_error("REAL_FRAME_DOMAIN_UNSUPPORTED");
        const auto a=ecef(anchor),b=ecef(target);const double x=b[0]-a[0],y=b[1]-a[1],z=b[2]-a[2];
        constexpr double r=3.141592653589793/180.;const double s=std::sin(anchor.latitude*r),c=std::cos(anchor.latitude*r),sl=std::sin(anchor.longitude*r),cl=std::cos(anchor.longitude*r);
        return {anchor_ned[0]-s*cl*x-s*sl*y+c*z,anchor_ned[1]-sl*x+cl*y,anchor_ned[2]-c*cl*x-c*sl*y-s*z};
    }
    Position fromNed(Vector point) const {
        for(double v:point)if(!std::isfinite(v))throw std::runtime_error("REAL_NED_INVALID");
        const double n=point[0]-anchor_ned[0],e=point[1]-anchor_ned[1],d=point[2]-anchor_ned[2];
        if(std::hypot(std::hypot(n,e),d)>10000.)throw std::runtime_error("REAL_FRAME_DOMAIN_UNSUPPORTED");
        constexpr double r=3.141592653589793/180.;const double s=std::sin(anchor.latitude*r),c=std::cos(anchor.latitude*r),sl=std::sin(anchor.longitude*r),cl=std::cos(anchor.longitude*r);auto a=ecef(anchor);
        return geographic({a[0]-s*cl*n-sl*e-c*cl*d,a[1]-s*sl*n+cl*e-c*sl*d,a[2]+c*n-s*d});
    }
};
}
