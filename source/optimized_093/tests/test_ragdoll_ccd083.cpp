#include "driver_ragdoll.hpp"
#include <btBulletDynamicsCommon.h>
#include <iostream>
#include <stdexcept>

using namespace ofo;

int main() {
 try {
  NativeDriver model;
  DriverPose pose;
  for (size_t i=0;i<driver_bone_count;++i) {
   const char index=char('a'+i);
   const char upper=char('A'+i);
   auto& segment=model.segments[i];
   segment.name=std::string("bone_")+index;
   segment.center={0.f,.15f+float(i)*.23f,0.f};
   segment.joint=segment.center-Vec{0,.115f,0};
   segment.dimension={.14f,.16f,.11f};
   segment.orientation=rotate_y(.28f);
   segment.mass=1;
   pose.skin[i]=identity();
   pose.parts[i]=translate(segment.center);
   if(i) model.native_behavior+=std::string("ParentId=SEGMENT_BONE_")+char(upper-1)+", ChildId=SEGMENT_BONE_"+upper+
    ", Type=HINGE, Position=Bones[Segments.bone_"+index+".BoneIndex], Axis1={1,0,0}, Limit1={-60,60}, MaxForce={1,1,1}\n";
  }
  btDefaultCollisionConfiguration config;
  btCollisionDispatcher dispatch(&config);
  btDbvtBroadphase broad;
  btSequentialImpulseConstraintSolver solver;
  btDiscreteDynamicsWorld world(&dispatch,&broad,&solver,&config);
  world.setGravity({0,0,0});
  btBoxShape wall_shape({1.f,5.f,.01f});
  btRigidBody::btRigidBodyConstructionInfo wall_info(0,nullptr,&wall_shape);
  btRigidBody wall(wall_info);
  btTransform wall_pose;
  wall_pose.setIdentity();
  wall_pose.setOrigin({0,0,.30f});
  wall.setWorldTransform(wall_pose);
  world.addRigidBody(&wall,1,8);
  {
   NativeDriverRagdoll rag(world,model,pose,identity(),{0,0,60},{0,0,0},80);
   const auto initial=rag.snapshot();
   for(size_t i=0;i<driver_bone_count;++i)
    for(size_t j=0;j<16;++j)
     if(std::abs(initial[i][j]-pose.parts[i][j])>1e-5f)throw std::runtime_error("collision frame must not rotate the rendered pose");
   world.stepSimulation(.01f,1,.01f);
   const auto landed=rag.snapshot();
   for(const auto& body:landed)
    if(body[14]>.34f) throw std::runtime_error("driver segment tunneled through a thin wall at 60 m/s");
  }
  world.removeRigidBody(&wall);
  std::cout<<"ragdoll swept contact and native render frames verified\n";
  return 0;
 } catch(const std::exception& e) {
  std::cerr<<e.what()<<'\n';
  return 1;
 }
}
