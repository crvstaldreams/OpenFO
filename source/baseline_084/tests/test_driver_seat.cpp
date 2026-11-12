#include "native_driver.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 NativeDriver d;d.duration=100;
 for(size_t i=0;i<driver_bone_count;i++){
  auto&t=d.tracks[i];t.parent=i?0:-1;
  t.keys={{0,{0,0,0},{0,0,0,1}},{100,{0,float(i+1)*.01f,0},{0,0,0,1}}};
  d.segments[i].center={0,float(i)*.1f,0};
 }
 DriverPlacement p;p.seat={-.382f,.2f,.027f};
 auto neutral=sample_driver_pose(d,p,.5f,0,false);
 for(float angle:{-.7f,0.f,.7f}){
  auto seated=sample_seated_driver_pose(d,p,angle);
  for(size_t i=0;i<driver_bone_count;i++)need(seated.skin[i]==neutral.skin[i],"Steering must not resample torso, legs or head animation");
  auto zero=sample_seated_driver_pose(d,DriverPlacement{},angle);
  need(length(point(seated.skin[0],{})-point(zero.skin[0],{})-p.seat)<1e-6f,"DriverLoc applied exactly once");
  auto rag=driver_ragdoll_pose(d,seated.parts);
  for(size_t i=0;i<driver_bone_count;i++)for(size_t k=0;k<16;k++)need(std::abs(rag.skin[i][k]-seated.skin[i][k])<1e-6f,"Release preserves exact seated palette");
 }
 // Authored animation remains independently sampleable for inspection/reuse.
 need(sample_driver_pose(d,p,0,0,false).skin[6]!=sample_driver_pose(d,p,1,0,false).skin[6],"Authored BSA track sampling preserved");
 std::cout<<"Driver neutral seat, steering isolation and release continuity passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
