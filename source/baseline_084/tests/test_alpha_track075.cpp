#include "alpha_track.hpp"
#include "simulation.hpp"
#include "sky_environment.hpp"
#include <iostream>
#include <set>
using namespace ofo;
namespace { void need(bool b,const char* message){if(!b)throw std::runtime_error(message);} }
int main(int argc,char**argv){try{
 if(argc!=3)return 2;
 Resources r(argv[1]);r.mount(argv[2],"alpha_content");
 for(int i=1;i<=2;++i){
  std::string stage="alpha_content/tracks/track"+std::to_string(i);
  auto g=load_geometry(r,stage_geometry(stage),stage);
  const auto p=alpha_track_spawn(r,stage,g);
  size_t collision=0;
  for(const auto&m:g.collision_meshes)collision+=m.indices.size()/3;
  need(g.native_tree_batches>200&&g.triangles>300000&&collision>100000,"incomplete alpha track geometry or CDB");
  need(g.props.size()>650&&g.props.size()==g.native_props.size(),"missing alpha placements");
  for(auto&m:g.meshes)if(m.prop_index>=0)need(size_t(m.prop_index)<g.props.size()&&!m.solid,"dynamic prop must be linked to the matching body");
  for(const auto&m:g.materials)for(auto&tex:m.texture)if(!tex.empty()&&tex!="colormap.tga")need(r.exists(tex),"track texture not found in original archive");
  need(r.exists(g.lighting_source),"original alpha lightmap is present");
  auto sky=load_sky_environment(r,stage);
  need(sky.native_name=="day","alpha track uses the retail day skybox");
  for(size_t face=0;face<5;++face)
   need(sky.day[face].rfind("tracks/shared/skybox/day_",0)==0&&r.exists(sky.day[face]),"retail skybox face resolves on alpha tracks");
  for(int car=17;car<=20;++car){
  Simulation sim(g,load_vehicle(r,car),p);
  for(int frame=0;frame<220;++frame)sim.step({});
  auto settled=sim.state();
  unsigned grounded=0;for(const auto&w:settled.wheel_contacts)grounded+=w.grounded&&w.load>100;
  need(grounded>=3&&std::abs(settled.car[13]-p[13])<1.2f,"spawn must settle on the native alpha road");
  Input gas;gas.throttle=1;
  for(int frame=0;frame<260;++frame)sim.step(gas);
  auto driven=sim.state();auto distance=length(point(driven.car,{})-point(settled.car,{}));
  need(distance>3&&!driven.driver_ejected,"alpha car must be drivable from alpha road spawn");
  std::cout<<stage<<" car "<<car<<": "<<g.meshes.size()<<" meshes, "<<g.triangles<<" triangles, "<<collision<<" collision triangles, "<<g.props.size()<<" props; drove "<<distance<<" m\n";
  }
 }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
