#pragma once
#include "assets.hpp"
#include "vehicle_damage.hpp"
#include <algorithm>
#include <cmath>
namespace ofo {
// Paint uses Tex2 only for the damage atlas paired with its selected livery.
// Prototype dirt masks are not replacement diffuse colors.
inline std::string native_runtime_layer(const Resources&r,const std::string&geometry,const Material&m,const std::string&base){
 if(m.shader!=5&&m.shader!=30){
  if(!m.texture[2].empty())return r.texture(geometry,m.texture[2]);
  return m.shader==10&&r.exists("cars/shared/lights_glow.dds")?"cars/shared/lights_glow.dds":"";
 }
 const auto path=fs::u8path(base);
 const auto stem=lower(path.stem().string());
 if(stem.empty()||stem.find("_damaged")!=stem.npos)return {};
 const auto damage_stem=stem+"_damaged";
 if(!m.texture[2].empty()){
  const auto authored=r.texture(geometry,m.texture[2]);
  if(!authored.empty()&&lower(fs::u8path(authored).stem().string())==damage_stem)return authored;
 }
 if(stem.rfind("skin",0)!=0)return {};
 const auto candidate=(path.parent_path()/(damage_stem+".dds")).generic_u8string();
 return r.exists(candidate)?candidate:std::string();
}
// Native PRO body oD1.a is a live damage blend. BGM stores alpha=255 even for
// clean vehicles, so reconstruct that input from the existing local contacts.
inline uint8_t native_paint_damage_alpha(Vec root,float damage,const std::vector<VehicleDamageContact>&contacts){
 if(!std::isfinite(damage))return 0;float weight=contacts.empty()?1.f:0.f;
 for(const auto&c:contacts){if(!(c.radius>0)||!std::isfinite(c.radius))continue;auto d=root-c.position;float f=dot(d,d)/(c.radius*c.radius);if(std::isfinite(f)&&f<1)weight=std::max(weight,(1-f)*(1-f));}
 return uint8_t(std::clamp(std::min(1.f,std::max(0.f,damage)*1.7f)*weight*255.f+.5f,0.f,255.f));
}
}
