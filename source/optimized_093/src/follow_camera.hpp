#pragma once
#include "presentation_camera.hpp"
#include <cstdint>
#include "nitro_camera.hpp"
namespace ofo {
// Authored follow composition with a level horizon and smooth heading only.
// Translation follows the car directly. Only active nitro adds a transient boom offset.
// Simulation time freezes the pose during pause/photo mode.
class FollowCamera {
 CameraPose pose_{};Vec last_position_{};double time_=0;float yaw_=0;NitroCameraEnvelope nitro_;uint64_t generation_=0;bool ready_=false;
 public:
 void reset(){ready_=false;}
 CameraPose update(const Mat& car,Vec offset,Vec target,float fov,float speed,float steer,double time,uint64_t generation,bool reset_now=false,bool nitro_active=false){
  (void)speed;(void)steer;
  Vec position=point(car,{}),forward=direction(car,{0,0,1});double rawdt=time-time_;
  bool valid=std::isfinite(time)&&std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z);
  bool reset=reset_now||!ready_||!valid||generation!=generation_||rawdt<0||rawdt>.25||length(position-last_position_)>20;
  float heading=std::atan2(forward.x,forward.z),dt=float(std::clamp(rawdt,0.,.25));
  if(reset){yaw_=heading;}
  else if(dt>0){float delta=std::remainder(heading-yaw_,2*3.14159265f);yaw_+=delta*(1-std::exp(-10*dt));}
  float boost=nitro_.update(nitro_active,time,generation,reset);
  if(reset||dt>0){offset.z-=.65f*boost;offset.y+=.05f*boost;fov+=8.f*boost;Mat orientation=rotate_y(yaw_);pose_={position+direction(orientation,offset),position+direction(orientation,target),fov};}
  time_=std::isfinite(time)?time:0;generation_=generation;last_position_=position;ready_=valid;return pose_;
 }
};
}
