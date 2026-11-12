#pragma once
#include "assets.hpp"

namespace ofo {
inline bool vehicle_paint_alias(const Material& material){
 const auto filename=lower(fs::u8path(clean_path(material.texture[0])).filename().u8string());
 return filename=="body.tga"||filename=="body.dds"||filename=="skin1.tga"||
        filename=="skin1.dds"||filename=="skinai1.tga"||filename=="skinai1.dds"||
        (filename.empty()&&lower(material.name)=="body");
}

inline std::string resolve_vehicle_diffuse(const Resources& resources,const std::string& geometry,
                                           const Material& material,int skin){
 if(vehicle_paint_alias(material)){
  const auto source=fs::u8path(clean_path(geometry));
  auto folder=source.parent_path().generic_u8string();
  const auto model=source.stem().u8string();
  if(model.rfind("menucar_",0)==0)folder="cars/car_"+model.substr(8);
  if(folder.rfind("cars/car_",0)==0||folder.rfind("alpha_content/cars/car_",0)==0){
   const bool ai=skin<0||skin>=101;
   const auto index=std::max(1ll,skin<0?-static_cast<long long>(skin):skin>=101?skin-100:skin);
   const auto chosen=std::string(ai?"skinai":"skin")+std::to_string(index);
   for(const auto& alias:{chosen,std::string("skin1"),std::string("body")}){
    for(const auto* extension:{".dds",".tga"}){
     auto path=folder+"/"+alias+extension;
     if(resources.exists(path))return path;
    }
   }
  }
 }
 auto reference=clean_path(material.texture[0]);
 auto stem=fs::path(reference).stem().u8string();
 // Crash variants belong to the collision update, never the clean livery.
 const auto suffix=stem.rfind("_damaged");
 if(suffix!=stem.npos&&suffix+8==stem.size()){
  auto clean=fs::path(reference).parent_path()/(stem.substr(0,suffix)+fs::path(reference).extension().string());
  if(auto texture=resources.texture(geometry,clean.generic_u8string());!texture.empty())return texture;
 }
 return resources.texture(geometry,reference);
}
}
