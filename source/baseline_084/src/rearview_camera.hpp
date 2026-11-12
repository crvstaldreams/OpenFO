#pragma once
#include "math.hpp"
namespace ofo {
struct RearviewCamera {Vec eye,target;Mat view,projection;};
// The camera is beyond the rear bumper, looking away from the player's body.
// Heading follows vehicle yaw while the horizon remains level during rolls.
inline RearviewCamera rearview_camera(const Mat&car,Vec lo,Vec hi,float aspect=4.f){
 Vec heading=direction(car,{0,0,1});heading.y=0;heading=unit(heading,{0,0,1});
 Vec origin=point(car,{});float rear=std::max(.5f,-lo.z)+.25f;
 Vec eye=origin-heading*rear+Vec{0,std::max(1.1f,hi.y+.25f),0};
 Vec target=eye-heading*60.f;
 float ratio=std::max(.1f,aspect);float vfov=2*std::atan(std::tan(100.f*3.14159265f/360.f)/ratio);
 return{eye,target,look_at(eye,target),perspective(vfov,ratio,.15f,450.f)};
}
}
