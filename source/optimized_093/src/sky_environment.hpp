#pragma once
#include "alpha_track.hpp"
#include <algorithm>
namespace ofo {
inline std::string sky_stage_key(std::string stage){stage=clean_path(stage);if(stage.rfind("tracks/",0)==0)stage.erase(0,7);std::replace(stage.begin(),stage.end(),'/','_');return stage;}
inline float sky_side_v(float y_over_horizontal){return 1-y_over_horizontal;}
// Original rectangular side images cover horizon..45 degrees; v=1 is the
// horizon, not y=-extent. Extend their last row below the horizon with CLAMP.
inline std::array<Vertex,4> sky_face_vertices(unsigned face,float extent=500,bool full_sphere=false){
 static const Vec p[6][4]={{{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}},{{1,-1,-1},{-1,-1,-1},{-1,1,-1},{1,1,-1}},{{-1,-1,-1},{-1,-1,1},{-1,1,1},{-1,1,-1}},{{1,-1,1},{1,-1,-1},{1,1,-1},{1,1,1}},{{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}},{{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1}}};
 std::array<Vertex,4> v{};for(unsigned i=0;i<4;i++){v[i].position=p[face%6][i]*extent;v[i].u=(i==0||i==3)?0:1;v[i].v=i<2?(face<4&&!full_sphere?2.f:1.f):0.f;}return v;
}
inline std::string sky_night_family(const std::string&stage){if(stage.find("/winter/")!=stage.npos)return "winter";if(stage.find("/pit/")!=stage.npos||stage.find("/derby/")!=stage.npos)return "industrial";if(stage.find("/forest/")!=stage.npos||stage.find("/menu/")!=stage.npos)return "forest";return "urban";}
inline float sky_night_yaw(const std::string&family){if(family=="forest")return -1.960540771484375f;if(family=="industrial")return 0.38674384355545044f;if(family=="urban")return 2.8005051612854004f;if(family=="winter")return -2.4294745922088623f;return 0;}
struct SkyEnvironment {std::string stage,native_name="day";std::array<std::string,6>day{},night{};Vec sun{-.6f,-1,-.2f};float yaw=0;};
inline std::string native_sky_face(const Resources&r,const std::string&name,const std::string&face){
 for(const auto&ext:{".dds",".dds.tga",".tga"}){auto key="tracks/shared/skybox/"+name+"_"+face+ext;if(r.exists(key))return key;}
 if(name!="day")return native_sky_face(r,"day",face);return{};
}
inline SkyEnvironment load_sky_environment(const Resources&r,const std::string&stage){
 SkyEnvironment out;out.stage=clean_path(stage);
 const bool alpha=alpha_track_path(out.stage);
 const std::string lighting_stage=alpha?"tracks/forest/forest1/a":out.stage;
 try{auto lighting=bed_file(r,lighting_stage+"/data/stage1.ini").at("Lighting");auto entry=lighting.at(lighting.contains("2")?"2":"1");if(!alpha&&entry.contains("SkyBox"))out.native_name=entry["SkyBox"].get<std::string>();if(entry.contains("SunPosition")){auto pos=vector3(entry["SunPosition"]);out.sun=unit(pos)*-1.f;out.yaw=std::atan2(pos.x,pos.z);}}
 catch(const std::exception&){/* Original stages without lighting use the native day family. */}
 auto family=sky_night_family(lighting_stage);out.yaw=sky_night_yaw(family);
 const char*faces[]={"ft","bk","lf","rt","up","dn"};for(int i=0;i<6;i++){auto own=out.stage+"/skybox/"+out.native_name+"_"+faces[i]+".dds";out.day[i]=!alpha&&r.exists(own)?own:native_sky_face(r,out.native_name,faces[i]);out.night[i]="tracks/shared/skybox/openfo_night/"+family+"_"+faces[i]+".dds";}
 return out;
}
}
