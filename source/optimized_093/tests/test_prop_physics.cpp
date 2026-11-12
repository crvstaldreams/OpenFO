#include "prop_physics.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
using namespace ofo;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void native_approach(){
 // Use factory-captured mass, inertia, local bounds and rotated world basis.
 auto path=std::filesystem::path(__FILE__).parent_path().parent_path()/"capture_assets/manifest.json";
 std::ifstream input(path);check(bool(input),"native capture manifest available");Json capture;input>>capture;
 const auto&o=capture.at("objects").at(0);PropDefinition p;p.id=o.at("id");p.mass=o.at("mass_native");p.restitution=o.at("restitution_native");
 auto v3=[](const Json&a){return Vec{a.at(0),a.at(1),a.at(2)};};p.half=v3(o.at("collision_half_extents"));p.center=v3(o.at("collision_center_local"));p.inertia=v3(o.at("inertia_diagonal_native"));for(int k=0;k<16;k++)p.transform[k]=o.at("world_matrix").at(k);
 btDefaultCollisionConfiguration c;btCollisionDispatcher dispatcher(&c);btDbvtBroadphase broadphase;btSequentialImpulseConstraintSolver solver;ContactWorld world(&dispatcher,&broadphase,&solver,&c);world.setGravity({0,0,0});PropPhysics props(world,{p});
 for(int k=0;k<20;k++)world.stepSimulation(.005f,0);check(props.released()==0&&props.pose(0)==p.transform,"captured body held at exact native pose before contact");
 btBoxShape car_shape({.8f,.3f,1.5f});btVector3 inertia;car_shape.calculateLocalInertia(1000,inertia);btRigidBody::btRigidBodyConstructionInfo ci(1000,nullptr,&car_shape,inertia);btRigidBody car(ci);btTransform start;start.setFromOpenGLMatrix(p.transform.data());auto local_center=btVector3(p.center.x,p.center.y,p.center.z);start.setOrigin(start*(local_center+btVector3(-p.half.x-.8f-.2f,0,0)));car.setWorldTransform(start);car.setInterpolationWorldTransform(start);auto direction=start.getBasis()*btVector3(1,0,0);car.setLinearVelocity(direction*3);world.addRigidBody(&car,2,4|16);world.contacts=[&]{return props.activate_contacts(&car,nullptr);};
 for(int k=0;k<100;k++)world.stepSimulation(.005f,0);
 check(props.released()==1,"approaching car releases native captured body");check(btVector3(props.velocity(0).x,props.velocity(0).y,props.velocity(0).z).dot(direction)>.01f,"native captured body receives car momentum");check(length(point(props.pose(0),{})-point(p.transform,{}))>.01f,"native captured pose changes after actual car contact");check(props.snapshot()->size()==1,"native body moved pose published to renderer/replay");world.contacts={};world.removeRigidBody(&car);
}
void resting_neighbor(){
 btDefaultCollisionConfiguration c;btCollisionDispatcher d(&c);btDbvtBroadphase b;btSequentialImpulseConstraintSolver s;ContactWorld w(&d,&b,&s,&c);w.setGravity({0,0,0});PropDefinition def;def.mass=20;def.half={.5f,.5f,.5f};def.inertia={4,4,4};def.transform=translate({0,2,0});PropPhysics props(w,{def});
 btSphereShape shape(.5f);btVector3 inertia;shape.calculateLocalInertia(80,inertia);btRigidBody::btRigidBodyConstructionInfo ci(80,nullptr,&shape,inertia);btRigidBody debris(ci);btTransform at;at.setIdentity();at.setOrigin({-1,2,0});debris.setWorldTransform(at);w.addRigidBody(&debris,16,4|16);w.contacts=[&]{return props.activate_contacts(nullptr,nullptr);};for(int i=0;i<20;i++)w.stepSimulation(.01f,0);check(props.released()==0,"resting debris touching a held neighbor does not release it");w.contacts={};w.removeRigidBody(&debris);
}
void chain_reaction(){
 btDefaultCollisionConfiguration c;btCollisionDispatcher d(&c);btDbvtBroadphase b;btSequentialImpulseConstraintSolver s;ContactWorld w(&d,&b,&s,&c);w.setGravity({0,0,0});
 std::vector<PropDefinition>defs;for(float x:{0.f,1.5f,3.f,20.f}){PropDefinition p;p.mass=20;p.half={.5f,.5f,.5f};p.inertia={4,4,4};p.transform=translate({x,2,0});defs.push_back(p);}PropPhysics props(w,defs);
 btSphereShape shape(.5f);btVector3 inertia;shape.calculateLocalInertia(100,inertia);btRigidBody::btRigidBodyConstructionInfo ci(100,nullptr,&shape,inertia);btRigidBody car(ci);btTransform at;at.setIdentity();at.setOrigin({-1.8f,2,0});car.setWorldTransform(at);car.setLinearVelocity({8,0,0});w.addRigidBody(&car,2,4|16);w.contacts=[&]{return props.activate_contacts(&car,nullptr);};
 for(int i=0;i<150;i++)w.stepSimulation(.01f,0);check(props.released()==3,"three-item chain wakes exactly the contacted neighbors");check(props.pose(3)==defs[3].transform,"distant object remains pinned");check(props.diagnostics()["chain_activations"].get<int>()>=2,"chain activations counted independently");check(props.velocity(2).x>0,"last neighbor receives transferred momentum");w.contacts={};w.removeRigidBody(&car);
}
void native_hinge(){
 btDefaultCollisionConfiguration c;btCollisionDispatcher d(&c);btDbvtBroadphase b;btSequentialImpulseConstraintSolver solver;ContactWorld w(&d,&b,&solver,&c);w.setGravity({0,-9.81f,0});
 PropDefinition gate;gate.mass=40;gate.half={1,.5f,.1f};gate.center={1,0,0};gate.inertia={4,14,14};gate.transform=translate({0,2,0});gate.joint.kind=PropJointDefinition::Hinge;gate.joint.x_limits={-.5f,.5f};PropPhysics props(w,{gate});
 for(int i=0;i<30;i++)w.stepSimulation(.01f,0);check(props.released()==0&&w.getNumConstraints()==0,"native joint stays held until sufficient impact");
 btSphereShape shape(.25f);btVector3 inertia;shape.calculateLocalInertia(100,inertia);btRigidBody::btRigidBodyConstructionInfo ci(100,nullptr,&shape,inertia);btRigidBody car(ci);btTransform at;at.setIdentity();at.setOrigin({1.5f,2,-.35f});car.setWorldTransform(at);car.setLinearVelocity({0,0,8});w.addRigidBody(&car,2,4|16);w.contacts=[&]{return props.activate_contacts(&car,nullptr);};for(int i=0;i<20;i++)w.stepSimulation(.005f,0);check(props.released()==1&&w.getNumConstraints()==1,"only authored gate gets a world hinge after impact");w.removeRigidBody(&car);w.contacts={};for(int i=0;i<200;i++){w.stepSimulation(.005f,0);props.update_rest(.005f);}check(length(point(props.pose(0),{})-Vec{0,2,0})<.015f,"native hinge pivot retained under impact/gravity");check(props.diagnostics().at("jointed_native_props")==1,"selected native joint diagnostic");
}
void native_wood_log(){
 btDefaultCollisionConfiguration c;btCollisionDispatcher dispatcher(&c);btDbvtBroadphase broadphase;
 btSequentialImpulseConstraintSolver solver;ContactWorld world(&dispatcher,&broadphase,&solver,&c);
 world.setGravity({0,0,0});
 PropDefinition log;
 log.mass=450;log.half={.2f,2.25f,.2f};log.inertia={760,76,760};log.log_contact=LogContact{12,1};
 PropPhysics props(world,{log});
 btSphereShape shape(.4f);
 btRigidBody::btRigidBodyConstructionInfo ci(1000,nullptr,&shape,{64,64,64});
 btRigidBody car(ci);
 btTransform pose;pose.setIdentity();pose.setOrigin({-.47f,0,-.47f});
 car.setWorldTransform(pose);car.setLinearVelocity({14,0,14});world.addRigidBody(&car,2,4|16);
 world.performDiscreteCollisionDetection();
 check(!props.activate_contacts(&car,nullptr),"rounded log has no collision at an OBB-only corner");
 pose.setOrigin({-.58f,0,0});car.setWorldTransform(pose);car.setLinearVelocity({8,0,0});
 world.performDiscreteCollisionDetection();
 check(!props.activate_contacts(&car,nullptr),"grazing wood-log impact stays below authored wake speed");
 car.setLinearVelocity({15,0,0});world.performDiscreteCollisionDetection();
 check(props.activate_contacts(&car,nullptr)&&props.released()==1,"direct strong hit releases the wood log");
 world.removeRigidBody(&car);
}
int main(){try{native_wood_log();native_hinge();native_approach();resting_neighbor();chain_reaction();btDefaultCollisionConfiguration c;btCollisionDispatcher d(&c);btDbvtBroadphase b;btSequentialImpulseConstraintSolver s;ContactWorld w(&d,&b,&s,&c);w.setGravity({0,-9.81f,0});PropDefinition p;p.mass=40;p.half={.5f,.5f,.5f};p.inertia={7,7,7};p.transform=translate({0,5,0});PropPhysics props(w,{p});for(int i=0;i<100;i++)w.stepSimulation(.01f,0);check(props.released()==0,"gravity must not activate fixed prop");check(props.pose(0)==p.transform,"held pose exact");
 btSphereShape sphere(.5f);btVector3 inertia;sphere.calculateLocalInertia(80,inertia);btRigidBody::btRigidBodyConstructionInfo ci(80,nullptr,&sphere,inertia);btRigidBody actor(ci);btTransform at;at.setIdentity();at.setOrigin({-.9f,5,0});actor.setWorldTransform(at);actor.setLinearVelocity({5,0,0});w.addRigidBody(&actor,2,4|16);w.contacts=[&]{return props.activate_contacts(&actor,nullptr);};w.stepSimulation(.01f,0);check(props.released()==1,"car contact releases prop");check(props.velocity(0).x>0,"incoming car impulse transfers in same step");check(props.pose(0)!=p.transform,"released body moves");w.removeRigidBody(&actor);
 PropDefinition p2=p;p2.transform=translate({4,5,0});PropPhysics second(w,{p2});actor.setWorldTransform(at);at.setOrigin({3.1f,5,0});actor.setWorldTransform(at);actor.setLinearVelocity({4,0,0});w.addRigidBody(&actor,8,4|16);w.contacts=[&]{return second.activate_contacts(nullptr,&actor);};w.stepSimulation(.01f,0);check(second.released()==1,"driver collision releases prop");w.removeRigidBody(&actor);PropDefinition p3=p;p3.transform=translate({8,5,0});PropPhysics third(w,{p3});at.setOrigin({7.1f,5,0});actor.setWorldTransform(at);actor.setLinearVelocity({4,0,0});w.addRigidBody(&actor,16,4|16);w.contacts=[&]{return third.activate_contacts(nullptr,nullptr);};w.stepSimulation(.01f,0);check(third.released()==1,"moving dynamic debris releases a held neighboring object");check(third.pose(0)!=p3.transform,"neighbor receives momentum in the same solver step");w.removeRigidBody(&actor);
 PropDefinition fast=p;fast.transform=translate({15,5,0});PropPhysics fourth(w,{fast});at.setOrigin({12,5,0});actor.setWorldTransform(at);actor.setInterpolationWorldTransform(at);actor.setLinearVelocity({200,0,0});actor.setCcdMotionThreshold(.1f);actor.setCcdSweptSphereRadius(.4f);w.addRigidBody(&actor,2,4|16);w.contacts=[&]{return fourth.activate_contacts(&actor,nullptr);};for(int i=0;i<5;i++)w.stepSimulation(.01f,0);check(fourth.released()==1,"CCD car impact from separation releases prop");check(fourth.velocity(0).x>0,"CCD impact transfers momentum");w.removeRigidBody(&actor);w.contacts={};std::cout<<"held gravity, car release, same-step impulse, driver release passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
