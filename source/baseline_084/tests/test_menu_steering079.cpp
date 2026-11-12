#include "native_driver.hpp"
#include "menu_steering.hpp"
#include <iostream>

using namespace ofo;
namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
}

int main(int argc,char** argv){try{
 if(argc!=3)return 2;
 Resources r(argv[1]);r.mount(argv[2],"alpha_content");
 const auto donor=load_menu_steering(r);
 require(donor.rim.vertices.size()==393,"menu donor contains only the authored steering wheel");
 for(int id=1;id<=20;++id){
  auto model=load_geometry(r,load_vehicle(r,id).body);
  const auto before=steering_mount(model);
  const auto placement=load_driver_placement(r,id,model);
  const auto count=model.meshes.size();
  require(before&&placement.has_steering,"native steering placement");
  require(donor.apply(model),"replace native rim");
  const auto after=steering_mount(model);
  require(after&&std::abs(after->radius-before->radius)<.0001f&&
          std::abs(after->rim_z-before->rim_z)<.0001f&&
          length(point(after->frame,{})-point(placement.steering,{}))<.0001f,
          "keep the stock steering axis, hand radius and rim depth");
  require(!donor.apply(model),"mesh replacement is idempotent");
  if(id<=16||id==18)
   require(model.meshes.size()==count+1&&model.meshes.back().name=="steering_column","keep original shaft");
  else require(model.meshes.size()==count,"one-piece cockpit wheel needs no separate shaft");
  auto wheel=std::find_if(model.meshes.begin(),model.meshes.end(),[](const Mesh& mesh){return mesh.name.find("steering_wheel")!=mesh.name.npos;});
  require(wheel!=model.meshes.end(),"car has a dedicated wheel mesh");
  require(model.materials.at(wheel->material).texture[0]=="tracks/menu/textures/dashboard.dds",
          "wheel uses the dashboard atlas from the menu track");
  require(wheel->indices.size()==508*3,"wheel is the authored mesh from the menu track");
 }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
