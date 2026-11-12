#include "native_material_runtime.hpp"
#include "vehicle_texture_resolver.hpp"
#include <iostream>
#include <stdexcept>
void need(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){try{using namespace ofo;need(argc>1,"data required");Resources r(argv[1]);if(argc>2)r.mount(argv[2],"alpha_content");Material body;body.shader=5;
 size_t skins=0;for(int car=1;car<=16;car++)for(auto prefix:{"skin","skinai"})for(int skin=1;skin<=7;skin++){
 auto base="cars/car_"+std::to_string(car)+"/"+prefix+std::to_string(skin)+".dds";if(!r.exists(base))continue;
 auto damaged=fs::path(base).replace_extension().generic_string()+"_damaged.dds";
 need(native_runtime_layer(r,"cars/car_1/body.bgm",body,base)==(r.exists(damaged)?damaged:std::string()),"selected skin must bind matching native damage texture");++skins;
 }
 const int cars=argc>2?20:16;
 for(int id=1;id<=cars;++id){
  const auto cfg=load_vehicle(r,id);
  const auto showroom=id<=16?"menu/cars/menucar_"+std::to_string(id)+".bgm":
   fs::path(cfg.body).replace_filename("model.showcar").generic_string();
  for(const auto& model:{cfg.body,showroom}){
   const auto geometry=load_geometry(r,model);
   bool paint_found=false;
   for(const auto& authored:geometry.materials){
    if(authored.shader!=5&&authored.shader!=30)continue;
    paint_found=true;
    const auto base=resolve_vehicle_diffuse(r,geometry.source,authored,1);
    need(base==fs::path(cfg.body).replace_filename(id<=16?"skin1.dds":"body.dds").generic_string(),
         "Race and showroom bind the correct clean livery");
    if(argc>2){
     auto paint=authored;
     paint.texture[2]="alpha_content/cars/car_1/dirt.dds";
     need(r.exists(paint.texture[2]),"A real authored dirt map is available for the test");
     const auto expected=id<=16?fs::path(base).replace_filename("skin1_damaged.dds").generic_string():std::string();
     need(native_runtime_layer(r,geometry.source,paint,base)==expected,
          "Paint rejects an authored dirt map and keeps only its own matching damage atlas");
    }
   }
   need(paint_found,"Every real body and showroom has a paint material");
  }
  std::cout<<id<<" "<<cfg.name<<" race and showroom paint passed\n";
 }
 Material lights;lights.shader=10;need(native_runtime_layer(r,"cars/car_1/body.bgm",lights,"cars/shared/lights.dds")=="cars/shared/lights_glow.dds","native running lamp Tex2");
 need(native_runtime_layer(r,"cars/car_1/body.bgm",body,"cars/car_1/skin1_damaged.dds").empty(),"never blend damaged skin into itself");
 need(native_paint_damage_alpha({0,0,0},1,{})==255,"full damage fallback");need(native_paint_damage_alpha({0,0,0},0,{})==0,"clean paint must not inherit BGM alpha 255");
 VehicleDamageContact hit;hit.position={0,0,0};hit.radius=1;hit.depth=.1;
 need(native_paint_damage_alpha({0,0,0},.25,{hit})>100,"partial localized damage");need(native_paint_damage_alpha({2,0,0},1,{hit})==0,"far paint stays clean");
 std::cout<<skins<<" native skin/damage pairs; lamp layer and localized alpha passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
