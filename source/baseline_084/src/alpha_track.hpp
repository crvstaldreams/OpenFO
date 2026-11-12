#pragma once
#include "assets.hpp"
namespace ofo {
inline bool alpha_track_path(const std::string& path){return path.rfind("alpha_content/tracks/",0)==0;}
Geometry load_alpha_track(const Resources&,const std::string& path,bool props_only=false);
Mat alpha_track_spawn(const Resources&,const std::string& stage,const Geometry&);
}
