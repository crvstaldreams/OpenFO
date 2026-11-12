#include "vehicle_panel_physics.hpp"
#include <btBulletDynamicsCommon.h>
namespace ofo {namespace {btVector3 bt(Vec v){return{v.x,v.y,v.z};}}
struct VehiclePanelPhysics::Impl {
 struct Part{VehiclePanelDefinition def;VehiclePanelDamage state;std::unique_ptr<btBoxShape>shape;std::unique_ptr<btRigidBody>body;std::unique_ptr<btTypedConstraint>hinge;float detached_age=0;bool ignored=false;};
 btDiscreteDynamicsWorld&w;btRigidBody&car;Vec com;std::vector<Part>parts;std::shared_ptr<const std::vector<VehiclePanelPose>>cached;
 Impl(btDiscreteDynamicsWorld&world,btRigidBody&c,Vec center,std::vector<VehiclePanelDefinition>defs):w(world),car(c),com(center){for(auto&d:defs){Part p;p.def=std::move(d);parts.push_back(std::move(p));}}
 void clear(){for(auto&p:parts){if(p.hinge){w.removeConstraint(p.hinge.get());p.hinge.reset();}if(p.body){if(p.ignored)p.body->setIgnoreCollisionCheck(&car,false);w.removeRigidBody(p.body.get());p.body.reset();}p.shape.reset();p.state={};p.detached_age=0;p.ignored=false;}cached=std::make_shared<const std::vector<VehiclePanelPose>>();}
 ~Impl(){clear();}
 void create(Part&p){Vec half=(p.def.hi-p.def.lo)*.5f;for(int k=0;k<3;k++)half[k]=std::max(.025f,half[k]);p.shape=std::make_unique<btBoxShape>(bt(half));p.shape->setMargin(.008f);btVector3 inertia;p.shape->calculateLocalInertia(std::max(.1f,p.def.mass),inertia);btRigidBody::btRigidBodyConstructionInfo ci(std::max(.1f,p.def.mass),nullptr,p.shape.get(),inertia);p.body=std::make_unique<btRigidBody>(ci);auto frame=car.getWorldTransform();frame.setOrigin(frame*bt(p.def.center-com));p.body->setWorldTransform(frame);p.body->setInterpolationWorldTransform(frame);p.body->setLinearVelocity(car.getVelocityInLocalPoint(frame.getOrigin()-car.getCenterOfMassPosition()));p.body->setAngularVelocity(car.getAngularVelocity());p.body->setFriction(.6f);p.body->setDamping(.04f,.08f);p.body->setCcdMotionThreshold(.03f);p.body->setCcdSweptSphereRadius(std::max(.01f,std::min({half.x,half.y,half.z})*.5f));w.addRigidBody(p.body.get(),16,1|2|4|8|16);
 if(p.def.hinge&&!p.state.detached){
  if(p.def.has_joint_limits){
   btTransform car_frame,part_frame;
   car_frame.setFromOpenGLMatrix(p.def.joint_frame.data());
   part_frame=car_frame;
   car_frame.setOrigin(bt(p.def.pivot-com));
   part_frame.setOrigin(bt(p.def.pivot-p.def.center));
   auto joint=std::make_unique<btGeneric6DofSpring2Constraint>(car,*p.body,car_frame,part_frame);
   joint->setLinearLowerLimit({0,0,0});joint->setLinearUpperLimit({0,0,0});
   joint->setAngularLowerLimit(bt(p.def.joint_min));
   joint->setAngularUpperLimit(bt(p.def.joint_max));
   for(int axis=0;axis<3;++axis){
    if(p.def.joint_stiffness[axis]<=0||p.def.joint_min[axis]>=p.def.joint_max[axis])continue;
    joint->enableSpring(axis+3,true);
    joint->setStiffness(axis+3,p.def.joint_stiffness[axis]);
    joint->setDamping(axis+3,.65f);
    joint->setEquilibriumPoint(axis+3,p.def.joint_rest[axis]);
   }
   p.hinge=std::move(joint);
  }else{
   auto hinge=std::make_unique<btHingeConstraint>(car,*p.body,bt(p.def.pivot-com),bt(p.def.pivot-p.def.center),bt(p.def.axis),bt(p.def.axis),true);
   hinge->setLimit(p.def.lower_limit,p.def.upper_limit,.9f,.3f,1);
   p.hinge=std::move(hinge);
  }
  w.addConstraint(p.hinge.get(),true);
 }else{
  p.body->setIgnoreCollisionCheck(&car,true);p.ignored=true;
 }
 }

 void publish(){size_t count=0;for(auto&p:parts)count+=bool(p.body);std::shared_ptr<std::vector<VehiclePanelPose>>out;if(!cached||cached->size()!=count){out=std::make_shared<std::vector<VehiclePanelPose>>();out->reserve(count);}size_t n=0;for(size_t i=0;i<parts.size();i++){const auto&p=parts[i];if(!p.body)continue;Mat m;p.body->getWorldTransform().getOpenGLMatrix(m.data());VehiclePanelPose current{i,multiply(m,translate(p.def.center*-1)),p.state.damage,p.state.detached,p.state.unlocked};if(!out){auto&old=(*cached)[n];if(current.panel!=old.panel||current.transform!=old.transform||current.damage!=old.damage||current.detached!=old.detached||current.unlocked!=old.unlocked){out=std::make_shared<std::vector<VehiclePanelPose>>();out->reserve(count);out->insert(out->end(),cached->begin(),cached->begin()+n);}}if(out)out->push_back(current);++n;}if(out)cached=std::move(out);}

};
VehiclePanelPhysics::VehiclePanelPhysics(btDiscreteDynamicsWorld&w,btRigidBody&c,Vec com,std::vector<VehiclePanelDefinition>d):p_(std::make_unique<Impl>(w,c,com,std::move(d))){p_->clear();}
VehiclePanelPhysics::~VehiclePanelPhysics()=default;
void VehiclePanelPhysics::clear(){p_->clear();}
void VehiclePanelPhysics::recover_attached(const Mat&previous_car){
 btTransform old;
 old.setFromOpenGLMatrix(previous_car.data());
 old.setOrigin(old*bt(p_->com));
 const auto delta=p_->car.getWorldTransform()*old.inverse();
 for(auto&part:p_->parts){
  if(!part.body||part.state.detached)continue;
  const auto frame=delta*part.body->getWorldTransform();
  part.body->setWorldTransform(frame);
  part.body->setInterpolationWorldTransform(frame);
  part.body->setLinearVelocity({0,0,0});
  part.body->setAngularVelocity({0,0,0});
  part.body->clearForces();
  part.body->activate(true);
  p_->w.getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(part.body->getBroadphaseHandle(),p_->w.getDispatcher());
  p_->w.updateSingleAabb(part.body.get());
 }
 p_->publish();
}
void VehiclePanelPhysics::update(const std::vector<VehicleDamageContact>& events,float dt){
 dt=std::clamp(dt,0.f,.1f);
 const float mass=p_->car.getInvMass()>0?1/p_->car.getInvMass():1000;
 for(auto& part:p_->parts){
  const VehicleDamageContact* activation=nullptr;
  for(const auto& event:events){
   const float before=part.state.attachment_momentum;
   hit_vehicle_panel(part.state,part.def,event,mass);
   if(part.state.attachment_momentum>before)activation=&event;
  }
  tick_vehicle_panel(part.state,part.def,dt);
  if((part.state.unlocked||part.state.detached)&&!part.body){
   p_->create(part);
   if(activation){
    const auto& frame=p_->car.getWorldTransform();
    const auto at=frame*bt(activation->position-p_->com);
    const auto direction=frame.getBasis()*bt(activation->normal);
    const float impulse=std::min(activation->impulse,part.def.mass*std::min(12.f,activation->closing_speed));
    part.body->applyImpulse(direction*impulse,at-part.body->getCenterOfMassPosition());
   }
  }
  if(part.state.detached&&part.hinge){
   p_->w.removeConstraint(part.hinge.get());
   part.hinge.reset();
   part.body->setIgnoreCollisionCheck(&p_->car,true);
   part.ignored=true;
   part.body->activate(true);
  }
  if(part.state.detached&&part.ignored){
   part.detached_age+=dt;
   if(part.detached_age>=.2f){
    part.body->setIgnoreCollisionCheck(&p_->car,false);
    part.ignored=false;
   }
  }
 }
 p_->publish();
}
std::shared_ptr<const std::vector<VehiclePanelPose>> VehiclePanelPhysics::snapshot()const{return p_->cached;}
size_t VehiclePanelPhysics::detached()const{return std::count_if(p_->parts.begin(),p_->parts.end(),[](const auto&p){return p.state.detached;});}
}
