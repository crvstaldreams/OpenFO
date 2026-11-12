#pragma once
#include "assets.hpp"
#include "race_camera.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ofo {
// camera.ini is the original FlatOut Lua/BED-style Cameras={[1]={...},...} document.
// Each camera is independent. Extra metadata and additional animation frames remain
// in the on-disk file, while the runtime uses the first authored keyframe.
struct VehicleCameraINI {
 int index=0;
 Vec position{},target{0,0,1};
 float fov=80,near_clipping=.1f,far_clipping=1800;
 int target_type=-1,animation_type=1,position_type=2,tracker_type=1;
};
inline std::vector<VehicleCameraINI> load_vehicle_cameras_ini(const Resources& resources,int id){
 std::vector<VehicleCameraINI> result;
 if(id<1||id>99)return result;
 const std::string path="cars/car_"+std::to_string(id)+"/camera.ini";
 if(!resources.exists(path))return result;
 try{
  const Json root=bed_file(resources,path);
  if(!root.contains("Cameras")||!root.at("Cameras").is_object())return result;
  const auto& records=root.at("Cameras");
  for(auto it=records.begin();it!=records.end();++it){
   if(result.size()>=64)break;
   const auto& entry=it.value();if(!entry.is_object())continue;
   VehicleCameraINI cam;
   try{cam.index=std::stoi(it.key());}catch(...){continue;}
   const auto valid=[](float a){return std::isfinite(a)&&std::abs(a)<10000;};
   auto number=[&](const char*key,float fallback){
    if(!entry.contains(key)||!entry.at(key).is_number())return fallback;
    return entry.at(key).get<float>();
   };
   auto integer=[&](const char*key,int fallback){return int(number(key,float(fallback)));};
   cam.animation_type=integer("AnimationType",1);
   cam.position_type=integer("PositionType",2);
   cam.target_type=integer("TargetType",-1);
   cam.tracker_type=integer("TrackerType",1);
   cam.near_clipping=number("NearClipping",.1f);
   cam.far_clipping=number("FarClipping",1800.f);
   if(!entry.contains("PositionFrames")||!entry.at("PositionFrames").contains("1"))continue;
   cam.position=vector3(entry.at("PositionFrames").at("1").at("Offset"));
   if(entry.contains("TargetFrames")&&entry.at("TargetFrames").contains("1"))
    cam.target=vector3(entry.at("TargetFrames").at("1").at("Offset"));
   else cam.target=cam.position+Vec{0,0,1};
   if(entry.contains("ZoomFrames")&&entry.at("ZoomFrames").contains("1"))
    cam.fov=entry.at("ZoomFrames").at("1").value("FOV",80.f);
   if(valid(cam.position.x)&&valid(cam.position.y)&&valid(cam.position.z)&&
      valid(cam.target.x)&&valid(cam.target.y)&&valid(cam.target.z)&&
      std::isfinite(cam.fov)&&cam.fov>=20&&cam.fov<=145&&
      std::isfinite(cam.near_clipping)&&cam.near_clipping>=.005f&&cam.near_clipping<100&&
      std::isfinite(cam.far_clipping)&&cam.far_clipping>cam.near_clipping+1&&cam.far_clipping<=10000.f)
    result.push_back(cam);
  }
  std::sort(result.begin(),result.end(),[](auto&a,auto&b){return a.index<b.index;});
 }catch(...){/* Invalid authored camera.ini must not crash the race. */}
 return result;
}
inline CameraPose ini_camera_pose(const VehicleCameraINI&cam,const Mat&car){
 const Vec eye=point(car,cam.position);
 Vec aim=cam.target;
 if(length(aim-cam.position)<1e-4f)aim=cam.position+Vec{0,0,1};
 return {eye,point(car,aim),cam.fov,unit(direction(car,{0,1,0}))};
}
}
