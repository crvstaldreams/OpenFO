#pragma once
#include "simulation.hpp"
namespace ofo {
inline float controller_steering(float value,float deadzone,float sensitivity){value=std::clamp(value,-1.f,1.f);deadzone=std::clamp(deadzone,.05f,.4f);sensitivity=std::clamp(sensitivity,.5f,2.f);if(std::abs(value)<=deadzone)return 0;return std::clamp((std::abs(value)-deadzone)/(1-deadzone)*sensitivity,-1.f,1.f)*(value<0?-1.f:1.f);}

// S/LT backs up after stopping. W/RT brakes a reversing car before selecting forward.
inline void direction_pedals(Input&in,const State&s){if(in.throttle>.1f&&s.gear<0&&std::abs(s.speed)>=.4f){in.brake=std::max(in.brake,in.throttle);in.throttle=0;return;}if(in.brake>.1f&&in.throttle<=0&&(s.gear<0||std::abs(s.speed)<.4f)){in.throttle=-in.brake;in.brake=0;}}
}
