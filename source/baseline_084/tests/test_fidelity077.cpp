#include "captured_scene.hpp"
#include "editor_scene_query.hpp"
#include "frontend.hpp"
#include "native_driver.hpp"
#include "simulation.hpp"
#include "engine_effects.hpp"
#include "vehicle_dynamics.hpp"
#include <iostream>
using namespace ofo;

static void need(bool condition,const char*message){
 if(!condition)throw std::runtime_error(message);
}
int main(int argc,char**argv){try{
 need(argc==3,"native resources and alpha directory required");
 Resources r(argv[1]);r.mount(argv[2],"alpha_content");
 Frontend menu(r);
 menu.career_progress.initialize();
 menu.career_progress.highest_class=3;
 need(!menu.career_car_unlocked(2),"a class marker alone cannot unlock showroom purchases");
 for(const auto&event:menu.career_rules.events)
  if(event.stage==3&&event.car_class==1)menu.career_progress.positions[event.level]=3;
 need(menu.career_car_unlocked(2)&&!menu.career_car_unlocked(3),"podium in every class C final unlocks B only");
 ImpactDamage attacker,defender;
 attacker.hit(15,9000,1000,.1f);
 defender.hit(15,9000,1000);
 need(std::abs(attacker.value()-.1f*defender.value())<1e-6f,"derby impact damage split");

 auto driver=load_native_driver(r,DriverModel::JohnnyAlpha);
 for(int id=17;id<=20;++id){
  auto cfg=load_vehicle(r,id);
  auto body=load_geometry(r,cfg.body);
  auto placement=load_driver_placement(r,id,body,&driver);
  auto pose=sample_seated_driver_pose(driver,placement,0);
  auto model=make_driver_geometry(driver);deform_driver(driver,pose,model);
  float head_top=-INFINITY;
  for(size_t m=0;m<model.meshes.size();++m)
   for(size_t v=0;v<model.meshes[m].vertices.size();++v)
    for(int i=0;i<4;++i)
     if(driver.weights[m][v].bone[i]==6&&driver.weights[m][v].weight[i]>=128)
      head_top=std::max(head_top,model.meshes[m].vertices[v].position.y);
  std::cout<<"alpha "<<id<<" seat="<<placement.seat.y<<" head="<<head_top<<" roof="<<cfg.top_hi.y<<std::endl;
  need(head_top+.015f<cfg.top_hi.y,"seated alpha driver must fit under roof");
  auto showroom=load_geometry(r,fs::path(cfg.body).parent_path().generic_string()+"/model.showcar");
  for(const auto&w:cfg.wheels){
   bool present=false;
   for(const auto&m:showroom.meshes)if(m.name==w.mesh_name){
    present=true;need(length(point(m.transform,{})-suspension_mount(w))<.001f,"showroom wheel shares the fitted race axle");
   }
   need(present,"each showroom wheel present");
  }
 }

 const std::string stage="tracks/stunt/stunt1/b";
 auto track=load_geometry(r,stage_geometry(stage),stage);
 append_captured_scene(track,r);
 const auto target=vector3(bed_file(r,stage+"/data/dummies.bed").at("Dummies").at("Dummy_HiJump_01").at("Position"));
 const float x=target.x,z=target.z+141;
 EditorSceneQuery floor(track);
 auto hit=floor.ray({x,12,z},{0,-1,0},20);
 need(bool(hit),"Highjump lane ground present");
 for(int id:{3,6,9,15,16}){
  bool launched=false;
  for(const float offset:{0.f,.35f,-.35f}){
   const auto spawn=multiply(translate({x+offset,hit->y+.55f,z}),rotate_y(3.14159265f));
   Simulation sim(track,load_vehicle(r,id),spawn);
   float rise=0,peak=0,approach=0;bool left_ramp=false;
   for(int step=0;step<1400;++step){
    auto s=sim.state();Input in;in.throttle=step>=100;in.nitro=step>=220;
    if(offset!=0&&s.car[14]>-52){
     const float heading=std::atan2(s.car[8],-s.car[10]);
     in.steer=std::clamp((s.car[12]-x-offset)*.045f+heading*.6f,-.25f,.25f);
    }
    sim.step(in);
    s=sim.state();
    if(s.car[14]<-52&&s.car[14]>-63)approach=std::max(approach,s.speed);
    if(s.car[14]<-62&&s.car[14]>-105){
     peak=std::max(peak,s.car[13]);rise=std::max(rise,s.velocity.y);
     int support=0;for(auto&w:s.wheel_contacts)support+=w.grounded&&w.load>100;
     if(!support&&s.car[14]<-68&&s.velocity.y>1)left_ramp=true;
    }
   }
   std::cout<<"Highjump "<<id<<" offset="<<offset<<" approach="<<approach<<" rise="<<rise<<" peak="<<peak<<" flight="<<left_ramp<<std::endl;
   if(left_ramp&&peak>10.6f&&rise>2){launched=true;break;}
  }
  need(launched,"native Highjump ramp launches car with aligned approach");
 }

}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
