#include "water.hpp"
namespace ofo {
bool water_material(const Material&m){if(m.shader==25)return true;auto n=lower(m.name);if(n=="water"||n.rfind("water_",0)==0)return true;for(auto&t:m.texture){auto s=lower(fs::path(clean_path(t)).stem().string());if(s=="water"||s.rfind("water_",0)==0)return true;}return false;}
WaterAnimation water_animation(double t,size_t n){if(!std::isfinite(t)||t<0)t=0;t=std::fmod(t,86400.);return{n?size_t(std::floor(t*12))%n:0,float(std::fmod(t*.02,1.)),float(std::fmod(t*-.016,1.))};}
std::vector<std::string>water_frames(const Resources&r){std::vector<std::string>v;for(int i=0;i<32;i++){char p[64];std::snprintf(p,sizeof p,"global/water/caust%02d.dds",i);if(r.exists(p))v.push_back(p);}return v;}
}
