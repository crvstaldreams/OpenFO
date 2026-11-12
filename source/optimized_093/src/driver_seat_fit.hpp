#pragma once
#include "native_driver.hpp"
#include <cmath>
// Seat placement is shared by race, cockpit, simulation/ragdoll and both GPUs.
// Stock native DriverLoc is retained unless demonstrably incompatible with a
// replacement cabin. A deliberately authored seat override takes precedence.
namespace ofo {
inline bool seat_finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool seat_near_steering(Vec seat,Vec wheel){
 const Vec delta=seat-wheel;
 // +Z is the car front. Pelvis should be behind/below the wheel and aligned
 // with its column, allowing both LHD and RHD cars.
 return seat_finite(seat)&&std::abs(delta.x)<=.85f&&delta.y>=-1.2f&&delta.y<=.5f&&delta.z>=-1.6f&&delta.z<=.45f;
}
inline Vec seat_from_steering(Vec wheel){return {wheel.x,wheel.y-.43f,wheel.z-.46f};}
inline bool find_driver_marker(const Geometry&body,Vec&position){
 for(const auto&kv:body.markers){auto label=lower(kv.first);
  if(label=="driverloc"||label=="driver_seat"||label=="driverseat"||label=="seat_driver"){
   position=point(kv.second,{});if(seat_finite(position))return true;
  }
 }
 return false;
}
inline DriverPlacement fit_driver_placement(const Geometry&body,DriverPlacement native,bool authored){
 Vec marker{};
 if(find_driver_marker(body,marker)){native.seat=marker;native.source="geometry:driver_seat_marker";return native;}
 if(!native.has_steering){
  if(authored)return native;
  // Last resort for geometry-only demo/alpha imports without DriverLoc and
  // without a named steering mesh. Keep this scoped to missing author data.
  Vec lo{1e10f,1e10f,1e10f},hi{-1e10f,-1e10f,-1e10f};size_t vertices=0;
  for(const auto&m:body.meshes)for(const auto&v:m.vertices){
   Vec w=point(m.transform,v.position);if(!seat_finite(w))continue;
   for(int k=0;k<3;k++){lo[k]=std::min(lo[k],w[k]);hi[k]=std::max(hi[k],w[k]);}
   ++vertices;
  }
  if(vertices>=3&&hi.x-lo.x>.5f&&hi.y-lo.y>.4f&&hi.z-lo.z>1.f&&hi.z-lo.z<16.f){
   const Vec size=hi-lo;
   native.seat={lo.x+size.x*.32f,lo.y+size.y*.38f,lo.z+size.z*.57f};
   native.source="geometry:bounds_seat_fallback";
  }
  return native;
 }
 Vec wheel=point(native.steering,{0,0,native.rim_z});
 if(!seat_finite(wheel))return native;
 if(!authored||!seat_near_steering(native.seat,wheel)){
  native.seat=seat_from_steering(wheel);native.source="geometry:steering_column_seat_fallback";
 }
 return native;
}
}
