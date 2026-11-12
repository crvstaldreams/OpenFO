#pragma once
#include "scene_lighting.hpp"
#include "vehicle_damage.hpp"
namespace ofo {
class VehicleHeadlightDamage {
 std::array<float,2> health_{{1,1}};
public:
 const std::array<float,2>& health()const{return health_;}
 // Solved external contacts only. A scrape/resting support cannot break a lamp.
 // Thresholds and localized front zones are adapters; no native lamp thresholds recovered.
 void hit(const VehicleConfig&cfg,const VehicleDamageContact&hit){
  if(!std::isfinite(hit.impulse)||!std::isfinite(hit.closing_speed)||!std::isfinite(hit.radius))return;
  if(hit.closing_speed<2.5f||hit.impulse/std::max(1.f,cfg.mass)<.7f)return;
  if(!std::isfinite(hit.position.x)||!std::isfinite(hit.position.y)||!std::isfinite(hit.position.z)||!std::isfinite(hit.normal.z))return;
  if(hit.normal.z>-.25f||hit.position.z<cfg.hi.z-.65f)return;
  const auto mount=headlight_mount(cfg);float radius=std::clamp(hit.radius,.25f,1.2f)+.18f;
  for(int side=0;side<2;side++){
   Vec lamp{(side==0?-1.f:1.f)*mount[0],mount[1],cfg.hi.z};
   if(length(hit.position-lamp)<=radius)health_[side]=0;
  }
 }
};
}
