#pragma once
#include "assets.hpp"

namespace ofo {
inline bool alpha_car_id(int id){return id>=17&&id<=20;}
inline bool alpha_car_resource(const std::string& p){
 if(p.rfind("alpha_content/cars/car_",0)!=0)return false;
 auto name=fs::path(p).filename().string();
 return name=="model.car"||name=="model.wheels"||name=="model.showcar";
}
inline int wheel_hub_corner(const std::string& name){
 constexpr std::array<const char*,4> hubs{{"wheelhub_fl","wheelhub_fr","wheelhub_rl","wheelhub_rr"}};
 for(size_t i=0;i<hubs.size();++i)if(name==hubs[i])return int(i);
 return -1;
}
std::string alpha_car_directory(int id);
Geometry load_alpha_car_geometry(const Resources&,const std::string& canonical_path);
VehicleConfig load_alpha_vehicle(const Resources&,int id);
}
