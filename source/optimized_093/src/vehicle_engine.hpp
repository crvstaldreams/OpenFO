#pragma once
#include "assets.hpp"
namespace ofo {
// Native parameters with an explicit rotor / slipping clutch adapter. The
// original executable's integration and automatic-shift algorithms are unknown.
inline float engine_curve(const VehicleConfig&c,float rpm){
 if(c.torque_curve.empty())return 1;
 if(rpm<=c.torque_curve.front().first)return c.torque_curve.front().second;
 for(size_t i=1;i<c.torque_curve.size();++i){auto a=c.torque_curve[i-1],b=c.torque_curve[i];if(rpm<=b.first)return a.second+(b.second-a.second)*(rpm-a.first)/(b.first-a.first);}
 return c.torque_curve.back().second;
}
struct VehicleEngine {
 float rpm=0,shift_remaining=0,clutch=0,transmitted=0;int gear=1,last_shift=0;bool limiter=false;float direction_brake=0;
 void reset(const VehicleConfig&c){*this=VehicleEngine{};rpm=c.idle_rpm;}
 float step(const VehicleConfig&c,const std::array<float,4>&wheel,float speed,float pedal,float brake,float handbrake,bool automatic,int shift,float clutch_pedal,float dt){
  constexpr float rpm_per_rad=9.5492966f;
  if(rpm<=0)rpm=c.idle_rpm;
  gear=std::clamp(gear,-1,int(c.ratios.size()));direction_brake=0;
  // A direction request first brakes. Crossing zero cannot apply reverse
  // wheel torque until road speed is low enough for selecting the new gear.
  bool opposite=(pedal<-.1f&&gear>=0)||(pedal>.1f&&gear<0);
  if(opposite&&std::abs(speed)<.7f){gear=pedal<0?-1:(c.ratios.empty()?0:1);shift_remaining=c.clutch_release+c.gear_shift+c.clutch_engage;}
  else if(opposite)direction_brake=std::abs(pedal);
  float throttle=opposite&&direction_brake>0?0:std::abs(pedal);
  float driven=(wheel[0]+wheel[1])*.5f*c.front_drive+(wheel[2]+wheel[3])*.5f*(1-c.front_drive);
  float power_peak=c.idle_rpm;float peak=0;for(auto p:c.torque_curve)if(p.first*p.second>peak){peak=p.first*p.second;power_peak=p.first;}
  float upshift=std::min(c.max_rpm*.91f,std::max(power_peak*1.12f,c.idle_rpm*2));
  if(shift_remaining<=0){int next=gear;
   // Automatic drive re-engages a real forward gear after manual neutral.
   // Synthetic configurations without forward ratios remain safely neutral.
   if(automatic&&gear==0&&pedal>.1f&&!c.ratios.empty())next=1;
   if(!automatic&&shift&&shift!=last_shift)next=std::clamp(gear+(shift>0?1:-1),0,int(c.ratios.size()));
   if(automatic&&gear>0&&throttle>.1f&&!opposite){
    if(rpm>upshift&&gear<int(c.ratios.size()))next=gear+1;
    else if(gear>1){float projected=std::abs(driven)*c.ratios[size_t(gear-2)]*c.final_ratio*rpm_per_rad;if(rpm<power_peak*.58f&&projected<upshift*.9f)next=gear-1;}
   }
   if(next!=gear){gear=next;shift_remaining=c.clutch_release+c.gear_shift+c.clutch_engage;}
  }last_shift=shift;
  float ratio=gear==0?0:(gear<0?c.reverse_ratio:c.ratios[size_t(gear-1)])*c.final_ratio;
  float target=std::max(0.f,driven*ratio);
  float inertia=std::max(.01f,c.engine_inertia+c.shaft_inertia);
  float rotor=rpm/rpm_per_rad;
  limiter=rpm>=c.max_rpm*.995f;
  float combustion=limiter?0:c.engine_torque*engine_curve(c,rpm)*throttle;
  float friction=c.engine_friction*rotor;
  // Idle governor only supplies the loss torque, never propels a parked car.
  float governor=std::clamp((c.idle_rpm/rpm_per_rad-rotor)*inertia/dt+friction,0.f,c.engine_torque*.5f);
  float free_torque=combustion-friction+governor;
  clutch=1-std::clamp(clutch_pedal,0.f,1.f);
  if(!gear||handbrake>.1f||direction_brake>0)clutch=0;
  if(target*rpm_per_rad<c.idle_rpm*1.2f){if(throttle<.01f)clutch=0;else clutch*=std::clamp((rpm-c.idle_rpm*.65f)/(c.idle_rpm*.7f),0.f,1.f);}
  if(shift_remaining>0){float engage=std::max(.001f,c.clutch_engage);clutch*=std::clamp(1-shift_remaining/engage,0.f,1.f);shift_remaining=std::max(0.f,shift_remaining-dt);}
  float capacity=c.clutch_torque*clutch;
  // Include reflected wheel inertia in the synchronization impulse. This is
  // stable when tires are airborne as well as when the road loads the axle.
  float inverse_wheel=0;for(size_t i=0;i<4;++i){float share=i<2?c.front_drive*.5f:(1-c.front_drive)*.5f;auto&w=c.wheels[i];float wi=std::max(.001f,.5f*w.mass*w.inertia_factor*w.radius*w.radius);inverse_wheel+=share*share/wi;}
  float required=(rotor-target+free_torque/inertia*dt)/(dt*(1/inertia+ratio*ratio*inverse_wheel));
  transmitted=std::clamp(required,-capacity,capacity);
  // Automatic launch clutch prevents the anti-stall floor creating energy.
  if(target*rpm_per_rad<c.idle_rpm&&transmitted>0)transmitted=std::min(transmitted,std::max(0.f,free_torque));
  rotor+=(free_torque-transmitted)*dt/inertia;
  rpm=std::clamp(rotor*rpm_per_rad,c.idle_rpm,c.max_rpm*1.08f);
  return transmitted*ratio*.9f;
 }
};
}
