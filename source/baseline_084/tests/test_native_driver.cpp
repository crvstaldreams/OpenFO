#include "native_driver.hpp"
#include "driver_seat_fit.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{Resources r(argc>1?argv[1]:"/tmp/ofo049-data");if(argc>2)r.mount(argv[2],"alpha_content");auto d=load_native_driver(r);need(d.geometry.triangles>1000,"Original male mesh triangles");need(d.blended_vertices>100,"Native blend vertices retained");need(d.tracks[0].parent==-1,"Native root hierarchy");need(d.segments[6].mass==41,"Native head mass");auto g=make_driver_geometry(d);auto*vertices=g.meshes[0].vertices.data();auto*indices=g.meshes[0].indices.data();auto topology=g.meshes[0].indices;size_t total=0;for(auto&m:g.meshes)total+=m.vertices.size();need(total==1474,"Original male vertex count");for(int id=1;id<=16;id++){auto body=load_geometry(r,"cars/car_"+std::to_string(id)+"/body.bgm");auto p=load_driver_placement(r,id,body);auto n=std::to_string(id);auto native=vector3(bed_file(r,"equipmnt/car"+n+"/body_car"+n+".bed").at("Car.Body.Car"+n).at("Settings").at("DriverLoc"));need(length(p.seat-native)<1e-7f,"Seat exactly native DriverLoc");need(p.has_steering,"Authored steering geometry all cars");for(float steer:{-.4f,.4f}){auto pose=sample_seated_driver_pose(d,p,steer);auto center=point(p.steering,{0,0,p.rim_z});auto right=unit(direction(p.steering,{1,0,0})),up=unit(direction(p.steering,{0,1,0}));auto hand=pose.hand_target[1]-center;const auto neutral_grip=point(p.steering,p.grip[1])-center;need(dot(hand-neutral_grip,up)*steer<0,"Right road steer moves right grip down: wheel agrees with actual turn");need(dot(hand,right)>0,"Grip remains on right of rim");}for(float phase:{0.f,.25f,.5f,.75f,1.f})for(float steering:{-.7f,0.f,.7f}){auto pose=sample_driver_pose(d,p,phase,steering);deform_driver(d,pose,g);for(auto&m:g.meshes)for(auto&v:m.vertices){need(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z),"Finite skin positions");need(std::abs(length(v.normal)-1)<1e-4f,"Normalized skinned normals");}need(g.meshes[0].vertices.data()==vertices&&g.meshes[0].indices.data()==indices,"No frame allocations");need(g.meshes[0].indices==topology,"Skin topology unchanged");auto rag=driver_ragdoll_pose(d,pose.parts);for(size_t i=0;i<11;i++)for(size_t j=0;j<16;j++)need(std::abs(rag.skin[i][j]-pose.skin[i][j])<1e-5f,"Ragdoll ejection preserves exact skin pose");}}
 auto p=load_driver_placement(r,1,load_geometry(r,"cars/car_1/body.bgm"));need(length(p.seat-Vec{-.382f,.2f,.027f})<1e-6f,"Car1 native seat axes");p=load_driver_placement(r,1,load_geometry(r,"cars/car_1/body.bgm"),&d);auto neutral=sample_seated_driver_pose(d,p,0);for(float steering:{-.7f,.7f}){auto turning=sample_seated_driver_pose(d,p,steering);for(size_t bone=0;bone<7;bone++)need(turning.skin[bone]==neutral.skin[bone],"Gameplay steering keeps authored seated torso/head/legs stable");need(turning.hands_locked,"Gameplay arms use authored steering geometry");for(int hand=0;hand<2;hand++){size_t upper=hand?9:7,lower=hand?10:8;auto shoulder=point(turning.skin[upper],d.segments[upper].joint);auto elbow=point(turning.skin[lower],d.segments[lower].joint);auto grip=point(turning.skin[lower],d.hands[hand].wrist);need(length(grip-turning.wrist_target[hand])<.001f,"Hands reach actual native rim");need(elbow.y<shoulder.y,"Elbow pole bends down inside cabin");need(std::abs(elbow.x-shoulder.x)<.15f,"Elbows do not spread through cabin sides");}auto raw=sample_driver_pose(d,DriverPlacement{},.5f,0,false);for(size_t bone=0;bone<7;bone++)need(length(point(turning.skin[bone],{})-point(raw.skin[bone],{})-p.seat)<1e-5f,"Gameplay seat applied once to neutral authored animation");}auto a=sample_driver_pose(d,p,0,0,false),b=sample_driver_pose(d,p,1,0,false);bool changed=false;for(size_t i=0;i<11;i++)for(size_t j=0;j<16;j++)changed=changed||std::abs(a.skin[i][j]-b.skin[i][j])>1e-4f;need(changed,"Authored BSA animation sampled rather than static bind");auto female=load_native_driver(r,true);need(female.geometry.triangles>1000&&female.blended_vertices>0,"Female native skin also decodes");
 auto alpha=load_native_driver(r,driver_model_from_id(2));
 auto alpha_geometry=make_driver_geometry(alpha);
 if(argc>2){
  need(alpha_geometry.source=="alpha_content/driver/johnny.ofad"&&alpha_geometry.triangles==2258&&alpha_geometry.meshes.at(0).vertices.size()==1663,"Prototype Johnny geometry imported");
  const auto& atlas=alpha_geometry.materials.at(alpha_geometry.meshes.at(0).material).texture[0];
  need(atlas=="alpha_content/driver/johnny_b.dds"&&r.texture(alpha_geometry.source,atlas)==atlas,"Johnny draws with prototype atlas");
  size_t multi=0;for(const auto&w:alpha.weights.at(0)){int count=0;for(auto q:w.weight)count+=q>0;multi+=count>=3;}
  need(multi>0,"Mapped joints retain the original multi-bone influence");
  for(int id=17;id<=20;id++){
   const auto base="alpha_content/cars/car_"+std::to_string(id-16);
   auto prototype=load_geometry(r,base+"/model.car");
   auto seat=load_driver_placement(r,id,prototype);
   need(length(seat.seat-vector3(bed_file(r,base+"/car.ini").at("DriverLoc")))<1e-6f,"Alpha seat uses the original prototype coordinates");
   need(seat.has_steering,"Alpha steering wheel placement is available");
   auto displaced=seat;
   displaced.seat={100,100,100};
   const auto repaired=fit_driver_placement(prototype,displaced,true);
   need(length(repaired.seat-point(seat.steering,{0,0,seat.rim_z}))<1.f,"Misaligned alpha seat falls back behind the rim");
   seat=load_driver_placement(r,id,prototype,&alpha);
   for(float steer:{-.45f,0.f,.45f}){
    auto seated=sample_seated_driver_pose(alpha,seat,steer);
    need(seated.hands_locked,"Johnny uses the original driver steering rig");
    if(!steer){
     need(length(seated.hand_target[0]-point(seat.steering,seat.grip[0]))<1e-5f,"Left prototype grip matches the rim tube");
     need(length(seated.hand_target[1]-point(seat.steering,seat.grip[1]))<1e-5f,"Right prototype grip matches the rim tube");
    }
   }
  }
 }else need(alpha_geometry.source==d.geometry.source&&alpha_geometry.triangles==d.geometry.triangles,"Missing alpha resource uses retail driver");
 auto alpha_pose=sample_seated_driver_pose(alpha,p,.4f);
 auto male_pose=sample_seated_driver_pose(d,p,.4f);
 need(alpha_pose.hands_locked&&male_pose.hands_locked,"Both driver meshes lock their fitted wrists to the wheel");
 for(size_t bone=0;bone<7;++bone)need(alpha_pose.skin[bone]==male_pose.skin[bone],"Alpha model shares stock torso and leg palette");
 auto rag=driver_ragdoll_pose(alpha,alpha_pose.parts);
 auto ejected=alpha_geometry;
 deform_driver(alpha,alpha_pose,alpha_geometry);deform_driver(alpha,rag,ejected);
 for(size_t mesh=0;mesh<alpha_geometry.meshes.size();++mesh){
  const auto& s=alpha_geometry.meshes[mesh];const auto& e=ejected.meshes[mesh];
  for(size_t v=0;v<s.vertices.size();++v){
   const auto& w=alpha.weights[mesh][v];
   bool hand=false;
   for(size_t k=0;k<4;++k)hand=hand||(w.weight[k]&&(w.bone[k]==8||w.bone[k]==10));
   if(!hand)need(length(s.vertices[v].position-e.vertices[v].position)<1e-5f,"Ejection keeps the same pose outside the released hands");
   need(std::abs(length(s.vertices[v].normal)-1)<1e-4f,"Alpha pose has finite unit normals");
  }
 }
 std::cout<<"Native male "<<total<<" vertices, "<<d.geometry.triangles<<" triangles, "<<d.blended_vertices<<" blended vertices; 16 seats/steering, 240 skin poses and ejection continuity passed\n";if(argc>2)std::cout<<"Johnny: 1663 vertices, 2258 triangles, native atlas, 11-joint animation and ejection, four prototype seats/grips passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
