#pragma once
#include "presentation_camera.hpp"
#include "native_driver.hpp"
namespace ofo {
constexpr int race_camera_count=3;
inline int next_race_camera(int camera){return (camera+1)%race_camera_count;}
// Shared mode order for both renderers: native follow, front bumper, seated head.
inline CameraPose race_camera_pose(int camera,const Mat&car,Vec lo,Vec hi,Vec follow_offset,Vec follow_target,float fov,const DriverPose* seated=nullptr){
 if(camera==0)return {point(car,follow_offset),point(car,follow_target),fov};
 Vec eye{},forward{0,0,1},up{0,1,0};
 if(camera==1){eye={0,lo.y+(hi.y-lo.y)*.36f,hi.z+.22f};}
 else if(seated){
  // Head position follows the fitted seat. Aim through the windscreen in
  // chassis space so the neutral BSA head tilt does not pitch the horizon.
  eye=point(seated->parts[6],{})+Vec{0,.018f,.157f};
  forward=unit({0,-.045f,1});
  up=unit({0,1,.045f});
 }
 else{eye={-.3f,lo.y+(hi.y-lo.y)*.75f,(lo.z+hi.z)*.15f};}
 Vec world_eye=point(car,eye);return {world_eye,world_eye+unit(direction(car,forward))*12,fov,unit(direction(car,up))};
}
inline Mat race_camera_view(const CameraPose&p){
 Vec f=unit(p.target-p.eye,{0,0,1}),r=unit(cross(p.up,f),{1,0,0}),u=cross(f,r);
 return {r.x,u.x,f.x,0,r.y,u.y,f.y,0,r.z,u.z,f.z,0,-dot(r,p.eye),-dot(u,p.eye),-dot(f,p.eye),1};
}
}
