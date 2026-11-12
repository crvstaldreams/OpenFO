#include "prop_physics.hpp"
#include <set>
namespace ofo {
namespace {btVector3 bt(Vec v){return{v.x,v.y,v.z};}}
struct PropPhysics::Impl {
 struct Body {PropDefinition def;std::unique_ptr<btConvexShape>collision;std::unique_ptr<btCompoundShape>shape;std::unique_ptr<btRigidBody>body;std::unique_ptr<btTypedConstraint>joint;bool joint_broken=false,bonus_claimed=false;bool released=false,ever_released=false,supported=false;float quiet=0;int support_parent=-1,resting_on=-1;std::vector<size_t>dependents;};
 btDiscreteDynamicsWorld&w;float contact_step=.005f;std::vector<Body>bodies;std::map<const btCollisionObject*,size_t>indices;std::vector<size_t>released,active;mutable std::shared_ptr<const std::vector<PropPose>>cached_snapshot;size_t car_hits=0,driver_hits=0,chain_hits=0,freezes=0,reactivations=0,support_wakes=0;
 Impl(btDiscreteDynamicsWorld&world,const std::vector<PropDefinition>&defs):w(world){bodies.reserve(defs.size());try{for(const auto&d:defs){if(!std::isfinite(d.mass)||d.mass<=0||d.mass>1000000)throw std::runtime_error("Invalid native prop mass");for(int k=0;k<3;k++)if(!std::isfinite(d.half[k])||d.half[k]<=0||d.half[k]>1000||!std::isfinite(d.center[k])||!std::isfinite(d.inertia[k])||d.inertia[k]<=0)throw std::runtime_error("Invalid native prop bounds/inertia");Body b;b.def=d;if(d.log_contact){
  // Native wood-log models are cylinders about local Y. Round the tread
  // contact without changing the authored placement, mass or inertia.
  const float radius=(d.half.x+d.half.z)*.5f;
  b.collision=std::make_unique<btCylinderShape>(btVector3(radius,d.half.y,radius));
 }else b.collision=std::make_unique<btBoxShape>(bt(d.half));
 b.collision->setMargin(std::min(.01f,std::min({d.half.x,d.half.y,d.half.z})*.1f));b.shape=std::make_unique<btCompoundShape>();btTransform child;child.setIdentity();child.setOrigin(bt(d.center));b.shape->addChildShape(child,b.collision.get());btRigidBody::btRigidBodyConstructionInfo ci(0,nullptr,b.shape.get());b.body=std::make_unique<btRigidBody>(ci);btTransform t;for(float x:d.transform)if(!std::isfinite(x))throw std::runtime_error("Invalid native prop transform");t.setFromOpenGLMatrix(d.transform.data());b.body->setWorldTransform(t);b.body->setInterpolationWorldTransform(t);b.body->setFriction(.6f);b.body->setRestitution(std::clamp(d.restitution,0.f,1.f));b.body->setDamping(.04f,.1f);b.body->setSleepingThresholds(.025f,.035f);b.body->setCcdMotionThreshold(std::max(.02f,std::min({d.half.x,d.half.y,d.half.z})));b.body->setCcdSweptSphereRadius(std::max(.005f,std::min({d.half.x,d.half.y,d.half.z})*.5f));auto index=bodies.size();indices[b.body.get()]=index;bodies.push_back(std::move(b));w.addRigidBody(bodies.back().body.get(),4,2|8|16);}}catch(...){for(auto&b:bodies)w.removeRigidBody(b.body.get());throw;}}
 ~Impl(){for(auto&b:bodies){if(b.joint)w.removeConstraint(b.joint.get());w.removeRigidBody(b.body.get());}}
 void release(size_t i){auto&b=bodies.at(i);if(b.released)return;
  if(b.support_parent>=0){auto&old=bodies[size_t(b.support_parent)].dependents;old.erase(std::remove(old.begin(),old.end(),i),old.end());b.support_parent=-1;}
  w.removeRigidBody(b.body.get());b.body->setCollisionFlags(b.body->getCollisionFlags()&~btCollisionObject::CF_STATIC_OBJECT);b.body->setMassProps(b.def.mass,bt(b.def.inertia));b.body->updateInertiaTensor();b.body->setActivationState(ACTIVE_TAG);w.addRigidBody(b.body.get(),16,1|2|4|8|16);b.body->activate(true);
  if(b.def.joint.kind!=PropJointDefinition::None&&!b.joint&&!b.joint_broken){const auto&d=b.def.joint;
   // Native compact model origin is the attachment pivot; its local Y is the
   // gate axis. Pole joints retain local X/Z limits around the same origin.
   if(d.kind==PropJointDefinition::Hinge){auto j=std::make_unique<btHingeConstraint>(*b.body,bt(d.pivot),bt(d.axis),true);j->setLimit(d.x_limits[0],d.x_limits[1]);if(d.max_force>0)j->enableAngularMotor(true,d.velocity,d.max_force*contact_step);b.joint=std::move(j);}
   else{btTransform frame;frame.setIdentity();frame.setOrigin(bt(d.pivot));auto j=std::make_unique<btGeneric6DofConstraint>(*b.body,frame,true);j->setLinearLowerLimit({0,0,0});j->setLinearUpperLimit({0,0,0});j->setAngularLowerLimit({d.x_limits[0],0,d.z_limits[0]});j->setAngularUpperLimit({d.x_limits[1],0,d.z_limits[1]});if(d.max_force>0)for(int axis:{0,2}){auto*m=j->getRotationalLimitMotor(axis);m->m_enableMotor=true;m->m_targetVelocity=d.velocity;m->m_maxMotorForce=d.max_force;}b.joint=std::move(j);}
   float threshold=d.break_force>0?d.break_force*contact_step:BT_LARGE_FLOAT;if(d.break_torque>0)threshold=std::min(threshold,d.break_torque*contact_step);b.joint->setBreakingImpulseThreshold(threshold);w.addConstraint(b.joint.get(),true);
  }
  b.released=true;b.quiet=0;active.push_back(i);if(!b.ever_released){b.ever_released=true;released.push_back(i);}else ++reactivations;
  // Static stacks must not remain suspended after their supporting prop wakes.
  auto children=std::move(b.dependents);b.dependents.clear();for(auto child:children)if(!bodies[child].released&&bodies[child].support_parent==int(i)){++support_wakes;release(child);}
 }

 void freeze(size_t i){auto&b=bodies.at(i);w.removeRigidBody(b.body.get());b.body->setLinearVelocity({0,0,0});b.body->setAngularVelocity({0,0,0});b.body->clearForces();b.body->setMassProps(0,{0,0,0});b.body->setCollisionFlags(b.body->getCollisionFlags()|btCollisionObject::CF_STATIC_OBJECT);b.body->updateInertiaTensor();b.body->setInterpolationWorldTransform(b.body->getWorldTransform());b.body->setActivationState(ISLAND_SLEEPING);w.addRigidBody(b.body.get(),4,2|8|16);b.released=false;b.quiet=0;b.support_parent=b.resting_on;if(b.support_parent>=0)bodies[size_t(b.support_parent)].dependents.push_back(i);++freezes;}
 // Predict the normal impulse with the held body's authored mass/inertia. A
 // held zero-mass Bullet proxy itself cannot provide this effective mass.
 bool sufficient_impact(const Body&target,const btRigidBody&source,const btVector3&at,const btVector3&normal)const{
  if(source.getInvMass()<=0)return false;
  float closing=-source.getVelocityInLocalPoint(at-source.getCenterOfMassPosition()).dot(normal);
  float min_closing=.25f;
  if(target.def.log_contact){
   const auto&log=*target.def.log_contact;
   min_closing=target.ever_released?log.reactivation_velocity:log.wakeup_velocity;
  }
  if(closing<std::max(.25f,min_closing))return false;
  auto r=at-target.body->getCenterOfMassPosition();auto torque=target.body->getWorldTransform().getBasis().transpose()*(r.cross(normal));
  float inv=source.computeImpulseDenominator(at,normal)+1.f/target.def.mass+torque.x()*torque.x()/target.def.inertia.x+torque.y()*torque.y()/target.def.inertia.y+torque.z()*torque.z()/target.def.inertia.z;
  float restitution=std::clamp(source.getRestitution()*target.def.restitution,0.f,1.f);
  float impulse=(1+restitution)*closing/std::max(inv,1e-8f);
  return impulse>=std::max(.2f,target.def.mass*.12f);
 }

};
PropPhysics::PropPhysics(btDiscreteDynamicsWorld&w,const std::vector<PropDefinition>&d):p(std::make_unique<Impl>(w,d)){}PropPhysics::~PropPhysics()=default;
bool PropPhysics::activate_contacts(const btCollisionObject*car,const btCollisionObject*driver){
 std::set<size_t>wake;auto*d=p->w.getDispatcher();
 for(int i=0;i<d->getNumManifolds();i++){
  auto*m=d->getManifoldByIndexInternal(i);auto*a=m->getBody0(),*b=m->getBody1();
  auto moving_debris=[](const btCollisionObject*o){auto*r=btRigidBody::upcast(o);auto*proxy=o->getBroadphaseHandle();return r&&r->getInvMass()>0&&proxy&&(proxy->m_collisionFilterGroup&16);};
  const btCollisionObject*other=nullptr,*source=nullptr;int reason=0;
  if((car&&a==car)||(driver&&a==driver)){other=b;source=a;reason=a==driver?2:1;}
  else if((car&&b==car)||(driver&&b==driver)){other=a;source=b;reason=b==driver?2:1;}
  else if(moving_debris(a)){other=b;source=a;reason=3;}
  else if(moving_debris(b)){other=a;source=b;reason=3;}
  else continue;
  auto found=p->indices.find(other);if(found==p->indices.end()||p->bodies[found->second].released)continue;
  bool touching=false;
  for(int j=0;j<m->getNumContacts();j++){
   const auto&point=m->getContactPoint(j);if(point.getDistance()>0)continue;
   auto*r=btRigidBody::upcast(source);if(!r)continue;auto at=source==a?point.getPositionWorldOnA():point.getPositionWorldOnB();
   auto normal=source==a?point.m_normalWorldOnB:-point.m_normalWorldOnB;
   if(!p->sufficient_impact(p->bodies[found->second],*r,at,normal))continue;
   touching=true;break;
  }
  if(touching&&wake.insert(found->second).second){if(reason==1)++p->car_hits;else if(reason==2)++p->driver_hits;else ++p->chain_hits;}
 }
 // Changing mass/broadphase during manifold iteration invalidates manifold pointers.
 // Release only after the scan; ContactWorld rebuilds contacts before solving.
 for(auto i:wake)p->release(i);return !wake.empty();
}
std::optional<size_t> PropPhysics::index(const btCollisionObject* object)const{
 auto it=p->indices.find(object);
 if(it==p->indices.end())return std::nullopt;
 return it->second;
}
bool PropPhysics::is_prop(const btCollisionObject*o)const{return p->indices.count(o)!=0;}
bool PropPhysics::is_wood_log(const btCollisionObject*o)const{
 auto i=p->indices.find(o);return i!=p->indices.end()&&p->bodies[i->second].def.log_contact.has_value();
}
float PropPhysics::pending_bonus_impulse(const btCollisionObject*o)const{
 auto i=p->indices.find(o);if(i==p->indices.end())return 0;
 const auto&body=p->bodies[i->second];const auto&b=body.def.bonus;
 return !body.bonus_claimed&&body.released&&b.type>0&&(b.cents>0||b.nitro>0)?std::max(.2f,body.def.mass*.12f):0;
}
std::optional<PropBonus> PropPhysics::claim_bonus(const btCollisionObject*o){
 if(pending_bonus_impulse(o)<=0)return std::nullopt;
 auto&b=p->bodies[p->indices.at(o)];b.bonus_claimed=true;return b.def.bonus;
}
int PropPhysics::collision_sound(const btCollisionObject*o)const{auto i=p->indices.find(o);return i==p->indices.end()?-1:p->bodies[i->second].def.collision_sound;}
void PropPhysics::set_contact_step(float dt){if(!std::isfinite(dt)||dt<=0||dt>.02f)return;p->contact_step=dt;for(auto i:p->active){auto&b=p->bodies[i];if(!b.joint)continue;const auto&d=b.def.joint;if(d.kind==PropJointDefinition::Hinge&&d.max_force>0)static_cast<btHingeConstraint*>(b.joint.get())->setMaxMotorImpulse(d.max_force*dt);float threshold=d.break_force>0?d.break_force*dt:BT_LARGE_FLOAT;if(d.break_torque>0)threshold=std::min(threshold,d.break_torque*dt);b.joint->setBreakingImpulseThreshold(threshold);}}
void PropPhysics::update_rest(float dt){
 if(!std::isfinite(dt)||dt<=0||p->active.empty())return;dt=std::min(dt,.1f);
 const auto gravity=p->w.getGravity();const bool needs_support=gravity.length2()>1e-6f;const auto up=needs_support?-gravity.normalized():btVector3(0,1,0);
 for(auto i:p->active){auto&body=p->bodies[i];if(body.joint&&!body.joint->isEnabled()){p->w.removeConstraint(body.joint.get());body.joint.reset();body.joint_broken=true;}p->bodies[i].supported=bool(body.joint)||!needs_support;p->bodies[i].resting_on=-1;}
 if(needs_support){auto*d=p->w.getDispatcher();for(int k=0;k<d->getNumManifolds();k++){auto*m=d->getManifoldByIndexInternal(k);
  for(int side=0;side<2;side++){const auto*self=side?m->getBody1():m->getBody0();auto found=p->indices.find(self);if(found==p->indices.end()||!p->bodies[found->second].released)continue;auto*other=btRigidBody::upcast(side?m->getBody0():m->getBody1());
   for(int j=0;j<m->getNumContacts();j++){const auto&c=m->getContactPoint(j);if(c.getDistance()>.02f)continue;auto normal=side?-c.m_normalWorldOnB:c.m_normalWorldOnB;if(normal.dot(up)<.35f)continue;if(other&&other->getInvMass()>0)continue;auto owner=p->indices.find(side?m->getBody0():m->getBody1());int parent=owner==p->indices.end()?-1:int(owner->second);auto&body=p->bodies[found->second];if(!body.supported||parent<0){body.supported=true;body.resting_on=parent;}break;}
  }
 }}
 // Freeze outside collision detection/solver iteration; broadphase removal
 // invalidates manifolds. Preserve every changed transform in sparse snapshots.
 for(size_t n=0;n<p->active.size();){auto i=p->active[n];auto&b=p->bodies[i];bool quiet=b.supported&&b.body->getLinearVelocity().length2()<=.025f*.025f&&b.body->getAngularVelocity().length2()<=.035f*.035f;
  b.quiet=quiet?b.quiet+dt:0;
  if(b.quiet>=.75f){p->freeze(i);p->active[n]=p->active.back();p->active.pop_back();}else ++n;
 }
}
size_t PropPhysics::released()const{return p->active.size();}Mat PropPhysics::pose(size_t i)const{Mat m;p->bodies.at(i).body->getWorldTransform().getOpenGLMatrix(m.data());return m;}Vec PropPhysics::velocity(size_t i)const{auto v=p->bodies.at(i).body->getLinearVelocity();return{v.x(),v.y(),v.z()};}
std::shared_ptr<const std::vector<PropPose>> PropPhysics::snapshot()const{
 // Replay owns prior snapshots. Publish a new immutable vector only when a
 // released transform changes; stationary props and the empty case share one.
 std::shared_ptr<std::vector<PropPose>>out;
 const auto&previous=p->cached_snapshot;
 if(!previous||previous->size()!=p->released.size()){
  out=std::make_shared<std::vector<PropPose>>();out->reserve(p->released.size());
 }
 for(size_t n=0;n<p->released.size();++n){
  PropPose current{p->released[n],pose(p->released[n])};
  if(!out&&((*previous)[n].index!=current.index||(*previous)[n].transform!=current.transform)){
   out=std::make_shared<std::vector<PropPose>>();out->reserve(p->released.size());
   out->insert(out->end(),previous->begin(),previous->begin()+n);
  }
  if(out)out->push_back(current);
 }
 if(out)p->cached_snapshot=std::move(out);
 return p->cached_snapshot;
}
Json PropPhysics::diagnostics()const{return{{"jointed_native_props",std::count_if(p->bodies.begin(),p->bodies.end(),[](const auto&b){return b.def.joint.kind!=PropJointDefinition::None;})},{"joint_adapter","native Hinge/Joint limits and break forces; compact origin pivot/local Y hinge axis; force-to-impulse conversion uses current internal contact timestep"},{"total",p->bodies.size()},{"held",p->bodies.size()-p->active.size()},{"released",p->active.size()},{"ever_moved",p->released.size()},{"static_returns",p->freezes},{"reactivations",p->reactivations},{"support_loss_wakes",p->support_wakes},{"car_activations",p->car_hits},{"driver_activations",p->driver_hits},{"chain_activations",p->chain_hits},{"rest_policy",{{"linear_mps",.025},{"angular_radps",.035},{"stable_seconds",.75},{"support_required_with_gravity",true},{"moving_platform","stay dynamic/asleep; freeze only on static support"},{"stack_support","wake frozen dependents when supporting prop wakes"},{"position","current settled pose, not original placement"}}},{"shape","native local OBB bounds; rounded cylinders for authored wood logs"},{"mass_inertia","exact captured inertia when available; native type mass and model-bound inertia otherwise"},{"activation","normal impulse estimate from authored mass/inertia; closing >=0.25m/s (wood logs use authored wake/reactivation minimums) and impulse >=max(0.2 Ns, mass*0.12); contact alone never activates"}};}
}
