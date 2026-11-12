#include "simulation.hpp"
#include "captured_scene.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace ofo;
namespace {
void quad(Geometry&g,float a,float ay,float b,float by){
 Mesh m;m.vertices={{{-20,ay,a}},{{-20,by,b}},{{20,by,b}},{{20,ay,a}}};
 m.indices={0,1,2,0,2,3};g.meshes.push_back(std::move(m));
}
Geometry course(){
 Geometry g;g.materials.push_back({"asphalt"});
 quad(g,-30,0,75,0);
 quad(g,75,0,79,1.4f);
 quad(g,79,1.4f,87,4.2f);
 quad(g,87,0,230,0);
 return g;
}
Geometry log_deck(){
 Geometry g;g.materials.push_back({"asphalt"});
 quad(g,-20,0,210,0);
 constexpr float spacing=.39f;
 for(int i=0;i<21;++i){
  PropDefinition log;
  log.id=i+1;log.mass=450;log.half={.203f,2.25f,.203f};log.inertia={760,76,760};log.restitution=.3f;
  log.log_contact=LogContact{12,1};
  // Native log dimensions and local Y axis. Lay the logs across the lane
  // and use successive placements to make a 16.7-degree rising deck.
  log.transform=identity();log.transform[0]=0;log.transform[1]=-1;
  log.transform[4]=1;log.transform[5]=0;
  log.transform[13]=-.11f+i*spacing*.30f;log.transform[14]=60+i*spacing;
  g.props.push_back(log);
 }
 return g;
}
void test_log_deck(const Resources&r){
 const auto track=log_deck();
 constexpr float dt=.01f;
 for(int id=1;id<=16;++id){
  Simulation s(track,load_vehicle(r,id),translate({0,1,0}));
  for(int i=0;i<200;++i)s.step({});
  Input in;in.throttle=1;
  float air=0,start=0,range=0,min_up=1;
  int wood_contacts=0,recovered=0;
  bool airborne=false,landed=false;
  for(int i=0;i<2400;++i){
   s.step(in,dt);const auto p=s.state();
   int loaded=0;
   for(const auto&w:p.wheel_contacts)if(w.grounded&&w.load>100){
    ++loaded;wood_contacts+=w.surface_id==24;
   }
   const float z=p.car[14];
   if(z>=69.2f&&!loaded&&!landed){
    if(!airborne)start=z;
    airborne=true;air+=dt;min_up=std::min(min_up,p.car[5]);
   }else if(airborne&&!landed&&loaded){
    landed=true;range=z-start;
   }
   if(landed&&loaded>=3&&p.velocity.z>1)++recovered;
   if(recovered>=40||p.driver_ejected)break;
  }
  const auto moved=s.diagnostics()["props"]["ever_moved"].get<int>();
  std::cout<<"wood car="<<id<<" flight_s="<<air<<" distance="<<range<<" min_up="<<min_up
           <<" wood_contacts="<<wood_contacts<<" recovered="<<recovered<<" logs_released="<<moved<<'\n';
  if(wood_contacts<10||air<.35f||range<3||min_up<.75f||recovered<40)
   throw std::runtime_error("Expected a supported launch and drivable landing from a wooden log deck");
 }
}
void test_forest_log(const Resources&r){
 const std::string stage="tracks/forest/forest2/a";
 auto track=load_geometry(r,stage_geometry(stage),stage);
 append_captured_scene(track,r);
 const std::string name="dyn_log_b_i_886";
 const auto mesh=std::find_if(track.meshes.begin(),track.meshes.end(),[&](const auto&m){return m.name==name&&m.prop_index>=0;});
 if(mesh==track.meshes.end())throw std::runtime_error("Native forest log not loaded");
 const auto& log=track.props.at(mesh->prop_index);
 if(!log.log_contact||log.log_contact->wakeup_velocity!=12||log.log_contact->reactivation_velocity!=1)
  throw std::runtime_error("Native wood-log wake thresholds missing");
 // Cross the unmodified site at the edge of the forest2/a log pile.
 const Vec center{-254.2991f,-6.9718f,2.8983f};
 if(length(point(log.transform,log.center)-center)>.02f)
  throw std::runtime_error("Native forest log moved away from the test approach");
 const Vec heading{.89256f,0,-.44613f},right{heading.z,0,-heading.x};
 auto approach=center-heading*25-right*.5f;approach.y=-7.55f;
 Simulation s(track,load_vehicle(r,3),multiply(translate(approach),rotate_y(std::atan2(heading.x,heading.z))));
 for(int i=0;i<140;++i)s.step({},.01f);
 Input in;in.throttle=1;in.nitro=true;
 float air=0,post_speed=0,min_up=1;
 int wood_contacts=0;
 for(int i=0;i<1000;++i){
  s.step(in,.01f);const auto p=s.state();
  const auto offset=point(p.car,{})-center;const float along=dot(offset,heading);
  if(along>10||std::abs(dot(offset,right))>6)break;
  int loaded=0;
  for(const auto&w:p.wheel_contacts)if(w.grounded&&w.load>100){
   ++loaded;
   if(std::abs(along)<1.8f)wood_contacts+=w.surface_id==24;
  }
  if(along>-.5f&&along<8&&!loaded)air+=.01f;
  if(along>5.f)post_speed=std::max(post_speed,dot(p.velocity,heading));
  if(along>-1)min_up=std::min(min_up,p.car[5]);
 }
 std::cout<<stage<<' '<<name<<" flight_s="<<air<<" wood_contacts="<<wood_contacts
          <<" post_speed_mps="<<post_speed<<" min_up="<<min_up<<'\n';
 if(wood_contacts<4||air<.08f||post_speed<10||min_up<.75f)
  throw std::runtime_error("Expected a loaded wood contact, flight and retained forward momentum on forest2/a");
}
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("Original data required");
 Resources r(argv[1]);
 r.mount(fs::path(__FILE__).parent_path().parent_path()/"capture_assets","capture_assets");
 const auto track=course();
 for(int id=1;id<=16;++id){
  Simulation s(track,load_vehicle(r,id),translate({0,1,0}));
  for(int i=0;i<200;++i)s.step({});
  Input in;in.throttle=1;
  float air=0,range=0,peak=0,vy=0,entry=0,min_up=1;int recovered=0;bool flying=false,landed=false;
  constexpr float dt=.01f;
  for(int i=0;i<2400;++i){
   s.step(in,dt);auto p=s.state();float z=p.car[14];
   if(z>72&&z<76)entry=std::max(entry,p.velocity.z);
   int loaded=0;for(const auto&w:p.wheel_contacts)loaded+=w.grounded&&w.load>100;
   if(z>=86&&!loaded&&!landed){
    if(!flying){vy=p.velocity.y;range=-z;}
    flying=true;air+=dt;peak=std::max(peak,p.car[13]);
   }else if(flying&&!landed&&loaded){landed=true;range+=z;}
   if(flying)min_up=std::min(min_up,p.car[5]);
   if(landed&&loaded>=3&&p.velocity.z>5)++recovered;
   if(recovered>=40||p.driver_ejected||z>165)break;
  }
  std::cout<<"car="<<id<<" approach="<<entry<<" flight_s="<<air<<" distance="<<range<<" peak="<<peak<<" launch_vy="<<vy<<" min_up="<<min_up<<" recovered="<<recovered<<std::endl;
  if(air<.40f||range<8||vy<1.2f||min_up<.80f||recovered<40)
   throw std::runtime_error("Expected a clean launch off the ramp and a drivable landing");
 }
 test_log_deck(r);
 test_forest_log(r);
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
