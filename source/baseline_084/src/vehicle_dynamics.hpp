#pragma once
#include "assets.hpp"
#include "alpha_car.hpp"
namespace ofo {
// Native menu uses Normal/Pro; polynomial-to-assist mapping below is a declared
// adapter because the original arcade steering solver is not decoded.
inline void apply_handling_difficulty(VehicleConfig&c,const Resources&r,int mode){
 bool pro=mode==2;auto n=std::to_string(c.id);auto body=alpha_car_id(c.id)?bed_file(r,alpha_car_directory(c.id)+"/car.ini"):Json(bed_file(r,"equipmnt/car"+n+"/body_car"+n+".bed").at("Car.Body.Car"+n).at("Settings"));auto&h=c.provenance["handling"];h["difficulty"]=pro?"Pro":"Normal";h["difficulty_id"]=pro?2:1;
 for(auto key:{"SteerBalanceFactor","SteerBalanceRate"}){auto source=std::string(pro?"Pro":"")+key;if(!body.contains(source))source=key;auto j=body.at(source);h[key]=Json::array({j.at("1"),j.at("2"),j.at("3")});}
 h["difficulty_source"]=alpha_car_id(c.id)?"prototype CAR.INI SteerBalanceFactor/Rate; the prototype has no separate Pro calibration":"native body SteerBalanceFactor/Rate vs ProSteerBalanceFactor/Rate; scripts/careermodemenu.bed SetEasyDifficulty";
 h["difficulty_adapter"]="grounded yaw/slip alignment with native onset and rate; original arcade solver parity unverified";
}
inline float handling_assist(const VehicleConfig&c,float speed){if(!c.provenance.is_object())return 0;auto h=c.provenance.value("handling",Json::object());if(!h.contains("SteerBalanceFactor"))return 0;auto f=h.at("SteerBalanceFactor").get<std::array<float,3>>();auto r=h.at("SteerBalanceRate").get<std::array<float,3>>();float v=std::abs(speed),above=std::max(0.f,v-f[0]);float rate=v/std::max(.001f,r[0]+r[1]*v+r[2]*v*v);return std::clamp((1-std::exp(-f[1]*above*above-f[2]*above*above*above))*std::clamp(rate,0.f,1.f),0.f,.65f);}
struct HandlingTuning {
 std::array<float,2> steering_rates{{1.8f,2.8f}},rollbar{{.12f,.1f}};
 explicit HandlingTuning(const VehicleConfig&c){try{auto j=c.provenance.value("handling",Json::object());if(j.contains("steering_rates"))steering_rates=j["steering_rates"].get<std::array<float,2>>();if(j.contains("rollbar"))rollbar=j["rollbar"].get<std::array<float,2>>();}catch(const std::exception&){}for(auto&v:steering_rates)v=std::isfinite(v)?std::clamp(v,.4f,6.f):1.8f;for(auto&v:rollbar)v=std::isfinite(v)?std::clamp(v,0.f,.8f):.1f;}
};
// Native angle, speed reduction and rate ratio remain authored. The response
// gain and combined return/attack countersteer are explicit control adapters.
inline float steering_step(float&input,float requested,float speed,const VehicleConfig&c,const HandlingTuning&t,float dt){
 requested=std::clamp(requested,-1.f,1.f);
 bool countersteering=requested*input<0;
 bool returning=std::abs(requested)<std::abs(input);
 float remaining=dt;
 if(countersteering){
  float rate=(t.steering_rates[0]+t.steering_rates[1])*1.25f;
  float crossing=std::abs(input)/rate;
  if(remaining<crossing){input+=std::copysign(rate*remaining,requested);remaining=0;}
  else{input=0;remaining-=crossing;}
  returning=false;
 }
 float rate=t.steering_rates[returning?1:0]*1.25f;
 input+=std::clamp(requested-input,-rate*remaining,rate*remaining);
 float velocity=std::abs(speed);
 float native=1-c.steering_reduction*std::clamp(velocity/std::max(1.f,c.steering_limit),0.f,1.f);
 return input*c.steering_angle*(3.14159265f/180)*native;
}
// Smooth brush approximation: slope chosen so authored optimal slip reaches the
// friction peak. This is a dissipative adapter, not a recovered native curve.
inline float tire_brush_response(float normalized_slip){float x=std::min(1.f,std::abs(normalized_slip));return std::copysign(1-(1-x)*(1-x)*(1-x),normalized_slip);}
// A firmer lateral brush keeps the authored zero/peak and friction envelope.
// Its small-slip slope is five instead of three: this response is an explicit
// adapter, since the native aligning-moment/brush solver is unavailable.
inline float tire_lateral_response(float normalized_slip){float x=std::min(1.f,std::abs(normalized_slip));float a=1-x;return std::copysign(1-a*a*a*a*a,normalized_slip);}
inline Vec aerodynamic_drag(const VehicleConfig&c,Vec velocity){return {-c.drag.x*velocity.x*std::abs(velocity.x),-c.drag.y*velocity.y*std::abs(velocity.y),-c.drag.z*velocity.z*std::abs(velocity.z)};}
struct TireImpulse {float longitudinal=0,lateral=0;};
inline TireImpulse combined_tire_impulse(float longitudinal,float lateral,float max_long,float max_lat){TireImpulse out;if(max_long<=0||max_lat<=0)return out;out.longitudinal=std::clamp(longitudinal,-max_long,max_long);out.lateral=std::clamp(lateral,-max_lat,max_lat);float x=out.longitudinal/max_long,y=out.lateral/max_lat;float demand=std::sqrt(x*x+y*y);if(demand>1){out.longitudinal/=demand;out.lateral/=demand;}return out;}
// Tire lateral force acts at an adapted roll center while retaining native COM,
// inertia, wheel positions and full suspension/obstacle collision.
inline Vec lateral_roll_point(Vec contact,Vec up){return contact-up*(dot(contact,up)*.45f);}
// Explicit 5cm spring preload adapter raises the settled chassis without
// moving authored suspension attachments or enlarging tire/sweep geometry.
// Extra compression headroom comes from sitting 5cm further into authored
// droop travel. Native min/max, spring and damping coefficients remain intact.
inline constexpr float suspension_lift_m=.05f;
// Keep the authored marker for mesh attachments. The wheel and its collider
// share the fitted center under the arch.
inline Vec suspension_mount(const WheelConfig& w){return w.mount+w.arch_offset;}
inline float suspension_force(const WheelConfig&w,float length,float relative_speed,float mass,float antiroll){float damping=relative_speed<0?w.bump:w.rebound;float bump=std::max(0.f,w.min+.025f-length)*w.k*2;return std::clamp(w.k*(w.rest+suspension_lift_m-length)-damping*relative_speed+bump+antiroll,0.f,mass*9.81f*1.25f);}
}
