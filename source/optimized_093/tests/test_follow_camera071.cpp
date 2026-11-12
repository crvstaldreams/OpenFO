#include "follow_camera.hpp"
#include "race_camera.hpp"
#include "simulation.hpp"
#include "course_ui.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
// Independent lens-sphere oracle, deliberately includes the player's chassis.
bool lens_clear(Vec eye,const std::vector<CameraObstacle>& obstacles,float radius){
 for(const auto&o:obstacles){Vec p=eye-point(o.transform,{});Vec q{dot(p,direction(o.transform,{1,0,0})),dot(p,direction(o.transform,{0,1,0})),dot(p,direction(o.transform,{0,0,1}))};
  if(q.x>=o.lo.x-radius&&q.x<=o.hi.x+radius&&q.y>=o.lo.y-radius&&q.y<=o.hi.y+radius&&q.z>=o.lo.z-radius&&q.z<=o.hi.z+radius)return false;
 }return true;
}
void check_pose(CameraPose desired,CameraPose safe,const std::vector<CameraObstacle>& obstacles,float aspect){
 Vec behind=desired.eye-desired.target;behind.y=0;behind=unit(behind);
 Vec delta=safe.eye-safe.target;float planar=length(Vec{delta.x,0,delta.z});
 need(dot(delta,behind)>3.5f,"follow eye remains behind player");
 need(std::abs(delta.y)/planar<.7f,"follow pitch stays below35 degrees");
 need(safe.eye.y<=desired.eye.y+1.51f,"collision correction stays local, no global grid roof rise");
 need(length(safe.eye-desired.eye)<=3,"bounded lens correction");
 need(lens_clear(safe.eye,obstacles,camera_clearance(safe.fov,aspect)),"follow eye and near plane outside every car");
 need(length(safe.target-desired.target)<1e-6f&&safe.fov==desired.fov,"collision correction retains target and lens");
}
int main(int argc,char**argv){try{
 Mat roll=identity();roll[0]=roll[5]=std::cos(.6f);roll[1]=std::sin(.6f);roll[4]=-roll[1];
 Mat tilted=multiply(translate({4,3,2}),multiply(rotate_y(.8f),multiply(rotate_x(.35f),roll)));
 FollowCamera level;auto level_pose=level.update(tilted,{0,2,-5},{},80,0,0,0,0);
 Vec forward=direction(tilted,{0,0,1});forward.y=0;forward=unit(forward);
 need(std::abs(level_pose.eye.y-tilted[13]-2)<1e-5f&&dot(level_pose.up,Vec{0,1,0})>.999f,"follow horizon ignores chassis pitch and roll");
 need(dot(level_pose.eye-point(tilted,{}),forward)<-4.99f,"yaw-leveled follow stays behind actual forward");
 for(float aspect:{4.f/3,16.f/9,32.f/9})for(int heading=0;heading<8;++heading){
  Mat car=multiply(translate({12,1,-18}),rotate_y(heading*3.14159265f/4));
  FollowCamera follow;auto desired=follow.update(car,{0,2,-5},{},80,0,0,0,0);
  std::vector<CameraObstacle> grid;
  for(int row=-2;row<=2;++row)for(int col=-2;col<=2;++col)grid.push_back({multiply(car,translate({col*2.5f,0,row*5.f})),{-.95f,.1f,-2.1f},{.95f,1.65f,2.1f}});
  auto safe=camera_safe_pose(desired,grid,aspect);check_pose(desired,safe,grid,aspect);
  std::vector<CameraObstacle> tall{{car,{-.95f,.1f,-2.1f},{.95f,1.65f,2.1f}},{multiply(car,translate({0,0,-5})),{-.9f,0,-2},{.9f,6,2}}};
  check_pose(desired,camera_safe_pose(desired,tall,aspect),tall,aspect);
  // An unrelated high vehicle must never lift the gameplay camera to its roof.
  grid.push_back({multiply(car,translate({50,0,0})),{-1,0,-2},{1,25,2}});
  need(length(camera_safe_pose(desired,grid,aspect).eye-safe.eye)<1e-5f,"distant roof cannot move follow lens");
  for(int frame=0;frame<=240;++frame){
   std::vector<CameraObstacle> moving{{car,{-.95f,.1f,-2.1f},{.95f,1.65f,2.1f}},{multiply(car,translate({0,0,-2-frame/40.f})),{-.95f,.1f,-2.1f},{.95f,1.65f,2.1f}}};
   auto pose=camera_safe_pose(desired,moving,aspect);check_pose(desired,pose,moving,aspect);
  }
 }
 if(argc>1){Resources r(argv[1]);auto authored=bed_file(r,"scripts/cameratemplates.bed").at("Templates").at("1").at("Camera");Vec offset=vector3(authored.at("PositionFrames").at("1").at("Offset")),target=vector3(authored.at("TargetFrames").at("1").at("Offset"));float fov=authored.at("ZoomFrames").at("1").at("FOV");need(length(offset-Vec{0,2,-5})<1e-5f&&length(target)<1e-5f,"actual native follow template");
  for(const auto&stage:first_build_stages(r)){
   auto cfg=load_vehicle(r,3);auto car=load_spawn(r,stage);auto roster=load_race_opponents(r,1,1,1,7,stage);need(roster.size()==7,"seven actual native AI spawns");
   need(cfg.lo.y>target.y,"regression target lies below chassis, not contained by it");
   auto bounds_ok=[](const VehicleConfig&v){Vec span=v.hi-v.lo;return span.x>1&&span.x<3&&span.y>.3f&&span.y<3&&span.z>2&&span.z<7;};need(bounds_ok(cfg),"player cfg bounds use meters");
   std::vector<CameraObstacle> grid{{car,cfg.lo,cfg.hi}};for(const auto&b:roster){need(bounds_ok(b.vehicle),"AI cfg bounds use meters");grid.push_back({b.spawn,b.vehicle.lo,b.vehicle.hi});}
   FollowCamera follow;auto desired=follow.update(car,offset,target,fov,0,0,0,0);auto safe=camera_safe_pose(desired,grid);std::cout<<stage<<" native spawn eye height desired="<<desired.eye.y-car[13]<<" safe="<<safe.eye.y-car[13]<<'\n';check_pose(desired,safe,grid,16.f/9);need(length(safe.eye-desired.eye)<1e-4f,"native grid preserves unobstructed authored eye");
   // Real track collision geometry, roster, and physics at the reported four-second instant.
   auto geometry=load_geometry(r,stage_geometry(stage),stage);Simulation sim(geometry,cfg,car);sim.set_opponents(roster,race_ai_route(race_course(r,stage)));
   for(int frame=0;frame<400;++frame){sim.step({});auto state=sim.state();desired=follow.update(state.car,offset,target,fov,state.speed,state.steer,state.time,state.recovery_generation);grid={{state.car,cfg.lo,cfg.hi}};for(size_t i=0;i<roster.size();++i)grid.push_back({state.opponents->at(i).car,roster[i].vehicle.lo,roster[i].vehicle.hi});safe=camera_safe_pose(desired,grid);check_pose(desired,safe,grid,16.f/9);}
   auto state=sim.state();need(std::abs(state.time-4)<.001,"actual simulation time4");std::cout<<stage<<" time="<<state.time<<" eye above car="<<safe.eye.y-state.car[13]<<'\n';
  }
 }
 std::cout<<"follow071: native grid/time4, bounded lens clearance, moving neighbor and distant roofs passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
