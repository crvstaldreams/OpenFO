#include "simulation.hpp"
#include "smash_award.hpp"
#include "prop_physics.hpp"
#include "engine_effects.hpp"
#include "vehicle_engine.hpp"
#include "driver_ragdoll.hpp"
#include "water.hpp"
#include "vehicle_dynamics.hpp"
#include "track_support.hpp"
#include "race_event.hpp"
#include "cpu_parallel.hpp"
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <chrono>
namespace ofo {
namespace {
struct WheelSweep final:btCollisionWorld::ClosestConvexResultCallback {
 const btCollisionObject*self;btVector3 up;int triangle=-1;
 WheelSweep(btVector3 from,btVector3 to,const btCollisionObject*ignore,btVector3 axis):ClosestConvexResultCallback(from,to),self(ignore),up(axis){m_collisionFilterGroup=2;m_collisionFilterMask=1|4|16;}
 bool needsCollision(btBroadphaseProxy*proxy)const override{return proxy->m_clientObject!=self&&ClosestConvexResultCallback::needsCollision(proxy);}
 btScalar addSingleResult(btCollisionWorld::LocalConvexResult&r,bool normal_world)override{auto n=normal_world?r.m_hitNormalLocal:r.m_hitCollisionObject->getWorldTransform().getBasis()*r.m_hitNormalLocal;if(n.dot(up)<=.2f)return 1;if(r.m_hitFraction<=m_closestHitFraction)triangle=r.m_localShapeInfo?r.m_localShapeInfo->m_triangleIndex:-1;return ClosestConvexResultCallback::addSingleResult(r,normal_world);}
};
 btVector3 bt(Vec v){return{v.x,v.y,v.z};}Vec vec(const btVector3&v){return{v.x(),v.y(),v.z()};}float sign(float v){return v<0?-1.f:v>0?1.f:0.f;}}
struct PhysicsScene {
 TrackSupportNormals support_normals;
 std::vector<Vec>track_normals;std::vector<float>track_planes;std::vector<size_t>track_materials,water_materials;std::vector<int>material_sounds,material_surfaces;
 std::unique_ptr<btTriangleMesh>water_triangles,triangles;std::unique_ptr<btBvhTriangleMeshShape>water_shape,track_shape;std::unique_ptr<btCollisionObject>water_object;std::unique_ptr<btRigidBody>track;std::unique_ptr<PropPhysics>props;
 btDefaultCollisionConfiguration configuration;btCollisionDispatcher dispatcher{&configuration};btDbvtBroadphase broadphase;btSequentialImpulseConstraintSolver solver;ContactWorld world{&dispatcher,&broadphase,&solver,&configuration};
 explicit PhysicsScene(const Geometry&g){for(const auto&m:g.materials){material_sounds.push_back(collision_category_for_material(m));std::string n=lower(m.name+" "+m.texture[0]);int id=2;for(const auto&p:{std::make_pair("ice",10),{"snow",12},{"grass",15},{"forest",16},{"sand",17},{"gravel",4},{"mud",19},{"rock",18},{"concrete",21},{"metal",23},{"wood",24},{"rubber",27}})if(n.find(p.first)!=n.npos){id=p.second;break;}material_surfaces.push_back(id);}world.setGravity({0,-9.81f,0});world.getSolverInfo().m_numIterations=12;world.getSolverInfo().m_splitImpulse=true;triangles=std::make_unique<btTriangleMesh>();const auto& collision=g.collision_meshes.empty()?g.meshes:g.collision_meshes;for(const auto&m:collision){if(m.captured||!m.solid)continue;auto name=lower(g.materials.at(m.material).name);if(water_material(g.materials.at(m.material))||name.find("shadow")!=name.npos)continue;for(size_t i=0;i+2<m.indices.size();i+=3){Vec a=point(m.transform,m.vertices[m.indices[i]].position),b=point(m.transform,m.vertices[m.indices[i+1]].position),c0=point(m.transform,m.vertices[m.indices[i+2]].position);if(length(cross(b-a,c0-a))>1e-6f){triangles->addTriangle(bt(a),bt(b),bt(c0),false);support_normals.add(a,b,c0);track_normals.push_back(unit(cross(b-a,c0-a)));track_planes.push_back(dot(track_normals.back(),a));track_materials.push_back(m.material);}}}
 water_triangles=std::make_unique<btTriangleMesh>();for(const auto&m:collision)if(!m.captured&&water_material(g.materials.at(m.material)))for(size_t i=0;i+2<m.indices.size();i+=3){Vec a=point(m.transform,m.vertices[m.indices[i]].position),b=point(m.transform,m.vertices[m.indices[i+1]].position),c=point(m.transform,m.vertices[m.indices[i+2]].position);if(length(cross(b-a,c-a))>1e-6f){water_triangles->addTriangle(bt(a),bt(b),bt(c),false);water_materials.push_back(m.material);}}
 if(water_triangles->getNumTriangles()){water_shape=std::make_unique<btBvhTriangleMeshShape>(water_triangles.get(),true);water_object=std::make_unique<btCollisionObject>();water_object->setCollisionShape(water_shape.get());water_object->setWorldTransform(btTransform::getIdentity());}
 support_normals.build();if(triangles->getNumTriangles()==0)throw std::runtime_error("No collidable static triangles");track_shape=std::make_unique<btBvhTriangleMeshShape>(triangles.get(),true);btRigidBody::btRigidBodyConstructionInfo floor(0,nullptr,track_shape.get());track=std::make_unique<btRigidBody>(floor);track->setFriction(.6f);world.addRigidBody(track.get(),1,2|8|16);props=std::make_unique<PropPhysics>(world,g.props);}
 ~PhysicsScene(){props.reset();world.removeRigidBody(track.get());}
};
struct Simulation::Impl {
 std::shared_ptr<const NativeDriver>driver_model;DriverPlacement driver_placement;std::unique_ptr<NativeDriverRagdoll>ragdoll;std::vector<std::pair<btPersistentManifold*,float>>sound_candidates;std::vector<int>material_sounds;uint64_t sound_id=0;std::unique_ptr<VehiclePanelPhysics>panels;bool derby=false,stunt=false,primary=true;Vec previous_driver_velocity{},filtered_driver_acceleration{};bool driver_velocity_valid=false;ImpactDamage engine_damage;struct Launch {Mat transform;Vec linear,angular,center;};
 struct ImpactCandidate {btPersistentManifold*first;float second;Launch launch;float damage_fraction=1,damage_mass=0;};
 std::vector<ImpactCandidate>impact_candidates;std::optional<Launch>pending_ejection;float ejection_cooldown=0;bool eject_held=false;
 std::shared_ptr<PhysicsScene>scene;ContactWorld&world;btCollisionDispatcher&dispatcher;std::vector<Vec>&track_normals;std::vector<float>&track_planes;std::vector<size_t>&track_materials;std::vector<size_t>&water_materials;std::unique_ptr<btCollisionObject>&water_object;std::unique_ptr<btBvhTriangleMeshShape>&water_shape;std::unique_ptr<btRigidBody>&track;std::unique_ptr<PropPhysics>&props;
 VehicleConfig cfg;HandlingTuning handling;float steering_input=0,grip_scale=1,recent_log_support=0;bool ramp_flight=false;Mat spawn;State pose;std::unique_ptr<btRigidBody>car;std::unique_ptr<btConvexShape>box;std::unique_ptr<btBoxShape>top_box;std::array<std::unique_ptr<btConvexShape>,4>wheel_shapes;std::array<int,4>wheel_children{};std::unique_ptr<btCompoundShape>shape;std::array<float,4>omega{},angle{},last_length{};VehicleEngine drivetrain;size_t recovery_count=0;std::unique_ptr<btCapsuleShape>driver_shape;std::unique_ptr<btRigidBody>driver;
 struct NitroTouch {double last_seen=-1e9,last_award=-1e9;bool awarded=false;};
 struct NitroContact {btPersistentManifold*manifold;const btCollisionObject*other;float closing,bonus_threshold;};
 std::vector<btPersistentManifold*>car_manifolds;
 std::map<const btCollisionObject*,NitroTouch>nitro_touches;std::vector<NitroContact>nitro_contacts;float nitro_earned=0;uint64_t nitro_awards=0;
 struct RemoteBody {
  std::unique_ptr<btBoxShape> shape;
  std::unique_ptr<btRigidBody> body;
  float mass=0;
 };
 std::map<int,RemoteBody> remote_bodies;
 static constexpr int remote_group=64;
 void update_remote_bodies(const std::vector<RemoteCollider>& remote){
  for(auto it=remote_bodies.begin();it!=remote_bodies.end();){
   if(std::any_of(remote.begin(),remote.end(),[&](const auto& p){return p.id==it->first;})){++it;continue;}
   world.removeRigidBody(it->second.body.get());
   it=remote_bodies.erase(it);
  }
  for(const auto& p:remote){
   auto& slot=remote_bodies[p.id];
   if(!slot.body){
    slot.shape=std::make_unique<btBoxShape>(bt((p.hi-p.lo)*.5f));
    slot.body=std::make_unique<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0,nullptr,slot.shape.get()));
    slot.body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
    slot.body->setActivationState(DISABLE_DEACTIVATION);
    slot.mass=p.mass;
    world.addRigidBody(slot.body.get(),remote_group,2);
   }
   btTransform transform;transform.setFromOpenGLMatrix(p.car.data());
   transform.setOrigin(transform*bt((p.lo+p.hi)*.5f));
   slot.body->setWorldTransform(transform);
   transform.setOrigin(transform.getOrigin()-bt(p.velocity)*.01f);
   slot.body->setInterpolationWorldTransform(transform);
   slot.body->setLinearVelocity(bt(p.velocity));
   world.updateSingleAabb(slot.body.get());
  }
 }
 bool prop_rewards_enabled=true;int64_t prop_bonus_cents=0;
 void collect_nitro_contacts(){
  nitro_contacts.clear();if(pose.driver_ejected)return;
  const bool wall_nitro=!derby&&cfg.nitro_capacity>0&&cfg.nitro_g>0;
  const bool player_smash=primary&&prop_rewards_enabled;
  if(!wall_nitro&&!player_smash)return;
  for(auto it=nitro_touches.begin();it!=nitro_touches.end();)if(pose.time-it->second.last_seen>2)it=nitro_touches.erase(it);else ++it;
  for(auto*m:car_manifolds){
   bool first=m->getBody0()==car.get();
   auto*other=first?m->getBody1():m->getBody0();if(other==driver.get())continue;
   bool prop=props->is_prop(other);float threshold=0;
   if(prop){if(!player_smash||(threshold=props->pending_bonus_impulse(other))<=0)continue;}
   else if(!wall_nitro)continue;
   bool touching=false,lateral=false;float closing=0;auto*rigid=btRigidBody::upcast(other);
   for(int j=0;j<m->getNumContacts();j++){
    const auto&contact=m->getContactPoint(j);if(contact.getDistance()>.02f)continue;
    touching=true;auto at=first?contact.getPositionWorldOnA():contact.getPositionWorldOnB();auto normal=first?contact.m_normalWorldOnB:-contact.m_normalWorldOnB;
    const bool side=std::abs(normal.y())<.65f;lateral=lateral||side;
    auto relative=car->getVelocityInLocalPoint(at-car->getCenterOfMassPosition());
    // Incoming debris striking a stationary car is not a player smash.
    if(prop&&-relative.dot(normal)<.25f)continue;
    if(rigid)relative-=rigid->getVelocityInLocalPoint(at-rigid->getCenterOfMassPosition());
    if(prop||side)closing=std::max(closing,float(-relative.dot(normal)));
   }
   if(prop){if(closing>=.25f)nitro_contacts.push_back({m,other,closing,threshold});continue;}
   if(!touching||!lateral)continue;
   auto it=nitro_touches.find(other);if(it==nitro_touches.end()){if(nitro_touches.size()>=512)continue;it=nitro_touches.emplace(other,NitroTouch{}).first;}
   auto&touch=it->second;if(pose.time-touch.last_seen>.2)touch.awarded=false;touch.last_seen=pose.time;
   if(!touch.awarded&&pose.time-touch.last_award>=.75&&closing>=2.f)nitro_contacts.push_back({m,other,closing,0});
  }
 }
 void award_collision_nitro(){
  auto add_nitro=[&](float gain){float before=pose.nitro;pose.nitro=std::clamp(before+gain,0.f,1.f);nitro_earned+=pose.nitro-before;if(pose.nitro>before)++nitro_awards;};
  for(const auto&event:nitro_contacts){
   float impulse=0;
   for(int j=0;j<event.manifold->getNumContacts();j++){const auto&contact=event.manifold->getContactPoint(j);if(contact.getDistance()<=.02f)impulse+=std::max(0.f,float(contact.getAppliedImpulse()));}
   if(event.bonus_threshold>0){
    if(impulse<event.bonus_threshold)continue;
    auto bonus=props->claim_bonus(event.other);
    if(!bonus)continue;
    ++pose.prop_hits;
    prop_bonus_cents+=bonus->cents;
    pose.prop_bonus=smash_dollars(prop_bonus_cents);
    if(bonus->type>0&&bonus->type<int(pose.prop_hits_by_type.size())){
     ++pose.prop_hits_by_type[bonus->type];
     auto&cents=pose.prop_bonus_cents_by_type[bonus->type];cents=int(std::min<int64_t>(2000000000,int64_t(cents)+bonus->cents));
    }
    // Claim even at full capacity or in derby, which has no nitro.
    if(!derby&&cfg.nitro_capacity>0&&cfg.nitro_g>0)add_nitro(bonus->nitro);
    continue;
   }
   auto&touch=nitro_touches.at(event.other);if(touch.awarded||impulse<cfg.mass*.15f)continue;
   float delta=impulse/std::max(1.f,cfg.mass);
   add_nitro(.01f+std::min(.15f,event.closing*.004f+delta*.015f));
   // Preserve the existing wall-contact separation and cooldown policy.
   touch.awarded=true;touch.last_award=pose.time;
  }
 }
 struct Bot {bool finished=false;RaceOpponentConfig config;RaceAIController controller;std::unique_ptr<Impl>vehicle;Bot(RaceOpponentConfig c,RaceRoute r):config(std::move(c)),controller(std::move(r),config.behavior,size_t(config.id),config.vehicle){} };std::vector<std::unique_ptr<Bot>>bots;

 Impl(const Geometry&g,VehicleConfig c,Mat start,std::vector<VehiclePanelDefinition>panel_defs,std::shared_ptr<const NativeDriver>dm,DriverPlacement dp,std::shared_ptr<PhysicsScene> shared={}):driver_model(std::move(dm)),driver_placement(dp),scene(shared?shared:std::make_shared<PhysicsScene>(g)),world(scene->world),dispatcher(scene->dispatcher),track_normals(scene->track_normals),track_planes(scene->track_planes),track_materials(scene->track_materials),water_materials(scene->water_materials),water_object(scene->water_object),water_shape(scene->water_shape),track(scene->track),props(scene->props),cfg(std::move(c)),handling(cfg),spawn(start){primary=!shared;material_sounds=scene->material_sounds;derby=race_mode(g.source)==RaceMode::Battle;stunt=race_mode(g.source)==RaceMode::Stunt;

 const Vec half=(cfg.hi-cfg.lo)*.5f;
 if(stunt||(cfg.id>=1&&cfg.id<=16&&cfg.has_top_collision)){
  // The full-width lower box used to put a square edge below each bumper.
  // Keep the authored roof and center belly, bevel the end overhangs so a
  // tire can climb the ramp before the bumper meets the next triangle.
  auto hull=std::make_unique<btConvexHullShape>();
  const float margin=.005f,bevel_y=std::min(stunt?.25f:.11f,half.y*(stunt?.88f:.6f));
  const float bevel_z=std::min(stunt?.64f:.32f,half.z*(stunt?.33f:.17f));
  for(float x:{-half.x+margin,half.x-margin})for(float sign:{-1.f,1.f}){
   const float end=sign*(half.z-margin);
   hull->addPoint(bt({x,half.y-margin,end}),false);
   hull->addPoint(bt({x,-half.y+margin+bevel_y,end}),false);
   hull->addPoint(bt({x,-half.y+margin,end-sign*bevel_z}),false);
  }
  hull->setMargin(margin);hull->recalcLocalAabb();box=std::move(hull);
 }else{
  box=std::make_unique<btBoxShape>(bt(half));
  box->setMargin(std::min(.065f,std::min({half.x,half.y,half.z})*.3f));
 }
 shape=std::make_unique<btCompoundShape>();btTransform child;child.setIdentity();child.setOrigin(bt((cfg.lo+cfg.hi)*.5f-cfg.center_mass));shape->addChildShape(child,box.get());
 if(cfg.has_top_collision){top_box=std::make_unique<btBoxShape>(bt((cfg.top_hi-cfg.top_lo)*.5f));top_box->setMargin(.035f);child.setOrigin(bt((cfg.top_lo+cfg.top_hi)*.5f-cfg.center_mass));shape->addChildShape(child,top_box.get());}
 for(int i=0;i<4;i++){auto&w=cfg.wheels[i];if(!w.collision_vertices.empty()){auto hull=std::make_unique<btConvexHullShape>();for(auto v:w.collision_vertices)hull->addPoint(bt(v),false);hull->recalcLocalAabb();hull->optimizeConvexHull();wheel_shapes[i]=std::move(hull);}else wheel_shapes[i]=std::make_unique<btCylinderShapeX>(btVector3(w.width*.5f,w.radius,w.radius));wheel_shapes[i]->setMargin(.001f);child.setIdentity();child.setOrigin(bt(suspension_mount(w)-cfg.center_mass));wheel_children[i]=shape->getNumChildShapes();shape->addChildShape(child,wheel_shapes[i].get());}
 btRigidBody::btRigidBodyConstructionInfo ci(cfg.mass,nullptr,shape.get(),bt(cfg.inertia));car=std::make_unique<btRigidBody>(ci);car->setDamping(cfg.damping[0],cfg.damping[1]);car->setFriction(.4f);car->setRestitution(.05f);car->setActivationState(DISABLE_DEACTIVATION);car->setCcdMotionThreshold(.4f);car->setCcdSweptSphereRadius(.15f);world.addRigidBody(car.get(),2,1|2|4|8|16|(primary?remote_group:0));panels=std::make_unique<VehiclePanelPhysics>(world,*car,cfg.center_mass,std::move(panel_defs));if(!shared){world.contacts=[this]{bool changed=props->activate_contacts(car.get(),driver.get());if(ragdoll)for(auto*b:ragdoll->bodies())changed=props->activate_contacts(car.get(),b)||changed;for(auto&bot:bots)changed=props->activate_contacts(bot->vehicle->car.get(),nullptr)||changed;return changed;};world.before_solve=[this]{before_impact_solve();for(auto&b:bots)b->vehicle->before_impact_solve();};world.after_solve=[this]{after_impact_solve();for(auto&b:bots)b->vehicle->after_impact_solve();update_stunt_contacts();};}recover();}
 ~Impl(){update_remote_bodies({});bots.clear();ragdoll.reset();panels.reset();if(scene.use_count()==1){world.contacts={};world.before_solve={};world.after_solve={};}if(driver)world.removeRigidBody(driver.get());world.removeRigidBody(car.get());}

 void before_impact_solve(){
 // Reuse one car-filtered dispatcher pass for support, nitro and damage.
 car_manifolds.clear();for(int i=0;i<dispatcher.getNumManifolds();++i){auto*m=dispatcher.getManifoldByIndexInternal(i);if(m->getBody0()==car.get()||m->getBody1()==car.get())car_manifolds.push_back(m);}
 // Suspension supplies support forces. Rigid tire support would hold the body
 // at full droop and bypass the spring; retain only side/obstacle contacts.
 auto up=car->getWorldTransform().getBasis()*btVector3(0,1,0);
 for(auto*m:car_manifolds){bool first=m->getBody0()==car.get();for(int j=m->getNumContacts()-1;j>=0;j--){const auto&p=m->getContactPoint(j);int child=first?p.m_index0:p.m_index1;auto normal=first?p.m_normalWorldOnB:-p.m_normalWorldOnB;bool support=normal.dot(up)>.2f;auto*other=first?m->getBody1():m->getBody0();int triangle=first?p.m_index1:p.m_index0;
 if(other==track.get()&&triangle>=0&&size_t(triangle)<track_normals.size()){
  auto authored=track_normals[triangle];
  support=support||std::abs(bt(authored).dot(up))>.55f;
  // At a rising ramp seam a low front bumper can briefly enter the
  // adjacent triangle from above and get an underside normal. Keep the
  // real wheel support and omit that inverted chassis contact only.
  if(stunt&&normal.y()<-.2f&&std::abs(authored.y)>.2f&&
     car->getWorldTransform().getBasis().getColumn(2).y()>.2f){
   const auto at=first?p.getPositionWorldOnA():p.getPositionWorldOnB();
   const auto local=vec(car->getWorldTransform().invXform(at))+cfg.center_mass;
   const float winding=authored.y>0?1.f:-1.f;
   const float above=winding*(dot(authored,vec(car->getCenterOfMassPosition()))-track_planes[triangle]);
   if(above>0&&local.y<cfg.lo.y+.18f&&local.z>cfg.wheels[0].mount.z){
    m->removeContactPoint(j);continue;
   }
  }
 }
 if(child>=wheel_children[0]&&child<=wheel_children[3]){int wheel=child-wheel_children[0];if(other==track.get()){auto axle=car->getWorldTransform()*shape->getChildTransform(child).getOrigin();auto at=first?p.getPositionWorldOnA():p.getPositionWorldOnB();const auto&wc=pose.wheel_contacts[size_t(wheel)];const auto&w=cfg.wheels[size_t(wheel)];
 if(wc.grounded&&wc.load>0&&(at-axle).dot(up)<-w.radius*.55f)support=true;
 // A briefly unloaded tire still has a valid sweep contact. Its low tread
 // must not catch a tiny road riser and replace suspension with a wall impulse.
 if(!support&&wc.grounded&&dot(wc.normal,vec(up))>.55f&&(at-axle).dot(up)<w.radius*.1f&&scene->support_normals.low_riser(triangle,wc.point,wc.normal,std::min(.045f,w.radius*.15f)))support=true;}if(support)m->removeContactPoint(j);}}}
 collect_nitro_contacts();
 impact_candidates.clear();for(auto*m:car_manifolds){bool first=m->getBody0()==car.get();auto*other=first?m->getBody1():m->getBody0();if(other==driver.get())continue;auto*rigid=btRigidBody::upcast(other);float closing=0;for(int j=0;j<m->getNumContacts();j++){auto&point=m->getContactPoint(j);if(point.getDistance()>.02f)continue;auto at=first?point.getPositionWorldOnA():point.getPositionWorldOnB();auto n=first?point.m_normalWorldOnB:-point.m_normalWorldOnB;auto relative=car->getVelocityInLocalPoint(at-car->getCenterOfMassPosition());if(rigid)relative-=rigid->getVelocityInLocalPoint(at-rigid->getCenterOfMassPosition());closing=std::max(closing,float(-relative.dot(n)));}if(closing>=2){
  ImpactCandidate impact{m,closing,{model(),vec(car->getLinearVelocity()),vec(car->getAngularVelocity()),vec(car->getCenterOfMassPosition())},1,cfg.mass};
  if(derby&&rigid&&rigid->getInvMass()>0&&other->getBroadphaseHandle()&&other->getBroadphaseHandle()->m_collisionFilterGroup==2){
   // Read both speeds before Bullet solves this pair. The symmetric mass
   // keeps the same base damage on both sides even with different car masses.
   impact.damage_fraction=car->getLinearVelocity().length2()>rigid->getLinearVelocity().length2()? .1f:1.f;
   impact.damage_mass=2.f/(car->getInvMass()+rigid->getInvMass());
  }
  if(derby&&rigid&&other->getBroadphaseHandle()&&other->getBroadphaseHandle()->m_collisionFilterGroup==remote_group){
   for(const auto& entry:remote_bodies)if(entry.second.body.get()==rigid){
    impact.damage_fraction=car->getLinearVelocity().length2()>rigid->getLinearVelocity().length2()?.1f:1.f;
    impact.damage_mass=2.f/(car->getInvMass()+1.f/std::max(1.f,entry.second.mass));
    break;
   }
  }
  impact_candidates.push_back(impact);
 }}
 sound_candidates.clear();if(!primary)return;for(int i=0;i<dispatcher.getNumManifolds();i++){auto*m=dispatcher.getManifoldByIndexInternal(i);auto*a=btRigidBody::upcast(m->getBody0()),*b=btRigidBody::upcast(m->getBody1());float closing=0;for(int j=0;j<m->getNumContacts();j++){auto&p=m->getContactPoint(j);if(p.getDistance()>.02f)continue;auto av=a?a->getVelocityInLocalPoint(p.getPositionWorldOnA()-a->getCenterOfMassPosition()):btVector3(0,0,0),bv=b?b->getVelocityInLocalPoint(p.getPositionWorldOnB()-b->getCenterOfMassPosition()):btVector3(0,0,0);closing=std::max(closing,float(-(av-bv).dot(p.m_normalWorldOnB)));}if(closing>.5f)sound_candidates.push_back({m,closing});}}

 void after_impact_solve(){
 award_collision_nitro();
 std::shared_ptr<std::vector<CollisionEvent>>sound_events;for(const auto&candidate:sound_candidates){auto*m=candidate.first;float impulse=0;btVector3 point(0,0,0);int material=-1;for(int j=0;j<m->getNumContacts();j++){auto&p=m->getContactPoint(j);float force=std::max(0.f,float(p.getAppliedImpulse()));if(p.getDistance()>.02f)continue;impulse+=force;point+=(p.getPositionWorldOnA()+p.getPositionWorldOnB())*(force*.5f);int tri=m->getBody0()==track.get()?p.m_index0:m->getBody1()==track.get()?p.m_index1:-1;if(tri>=0&&size_t(tri)<track_materials.size())material=int(track_materials[tri]);}if(impulse<1)continue;int first_sound=props->collision_sound(m->getBody0()),second_sound=props->collision_sound(m->getBody1());int category=std::max(first_sound,second_sound);if(category==0&&(first_sound>=0||second_sound>=0))continue;if(category<0)category=0;if(!category&&ragdoll){for(auto*body:ragdoll->bodies())if(body==m->getBody0()||body==m->getBody1()){category=8;break;}}if(!category&&material>=0&&size_t(material)<material_sounds.size())category=material_sounds[material];if(!category)category=4;if(!sound_events){sound_events=std::make_shared<std::vector<CollisionEvent>>();if(pose.collision_events)*sound_events=*pose.collision_events;}if(sound_events->size()>=128)sound_events->erase(sound_events->begin());sound_events->push_back({++sound_id,vec(point/impulse),impulse,candidate.second,category});}if(sound_events)pose.collision_events=std::move(sound_events);

  std::shared_ptr<std::vector<VehicleDamageContact>> history,events;
  for(auto&candidate:impact_candidates){
   auto*manifold=candidate.first;const bool first=manifold->getBody0()==car.get();
   float impulse=0;btVector3 location(0,0,0),normal(0,0,0);
   for(int j=0;j<manifold->getNumContacts();j++){
    auto&contact=manifold->getContactPoint(j);const float applied=std::max(0.f,float(contact.getAppliedImpulse()));
    if(contact.getDistance()>.02f||applied<=0)continue;
    impulse+=applied;location+=(first?contact.getPositionWorldOnA():contact.getPositionWorldOnB())*applied;
    normal+=(first?contact.m_normalWorldOnB:-contact.m_normalWorldOnB)*applied;
   }
   engine_damage.hit(candidate.second,impulse,candidate.damage_mass,candidate.damage_fraction);
   // Resting support has no incoming collision speed. Record only an external
   // solved impact, and use that contact's chassis-root coordinates for rendering.
   const float delta_speed=impulse/std::max(1.f,cfg.mass);
   if(delta_speed<.15f||normal.length2()<1e-8f)continue;
   const auto&body=car->getWorldTransform();VehicleDamageContact hit;
   hit.position=vec(body.inverse()*(location/impulse))+cfg.center_mass;
   hit.normal=vec(body.getBasis().transpose()*normal.normalized());
   // Adapter thresholds: a solved frontal impact must remove substantial speed.
   // Original native release thresholds have not been recovered from the assets.
   if(primary&&!stunt&&!pose.driver_ejected&&!pending_ejection&&ejection_cooldown<=0&&candidate.second>=13.5f&&delta_speed>=8.f&&hit.normal.z<-.65f&&hit.position.z>cfg.center_mass.z+.3f&&dot(candidate.launch.linear,unit(direction(candidate.launch.transform,{0,0,1})))>=12.f)pending_ejection=candidate.launch;
   hit.impulse=impulse*candidate.damage_fraction;hit.closing_speed=candidate.second;
   hit.radius=std::clamp(.3f+candidate.second*.025f,.3f,1.2f);
   hit.depth=std::clamp(delta_speed*.025f,.005f,.45f)*candidate.damage_fraction;
   if(!events){events=std::make_shared<std::vector<VehicleDamageContact>>();if(pose.collision_contacts)*events=*pose.collision_contacts;}
   if(events->size()<32)events->push_back(hit);
   if(!history){history=std::make_shared<std::vector<VehicleDamageContact>>();if(pose.impacts)*history=*pose.impacts;}
   // Repeated manifold points in the same damaged patch share one bounded dent.
   auto nearest=std::find_if(history->begin(),history->end(),[&](const auto&old){return length(old.position-hit.position)<.25f&&dot(old.normal,hit.normal)>.7f;});
   if(nearest!=history->end()){if(hit.depth>nearest->depth)*nearest=hit;}
   else if(history->size()<32)history->push_back(hit);
   else{auto weakest=std::min_element(history->begin(),history->end(),[](const auto&a,const auto&b){return a.depth<b.depth;});if(hit.depth>weakest->depth)*weakest=hit;}
  }
  if(history)pose.impacts=std::move(history);
  if(events)pose.collision_contacts=std::move(events);
 }
 Mat model()const{Mat m;car->getWorldTransform().getOpenGLMatrix(m.data());auto p=point(m,cfg.center_mass*-1);m[12]=p.x;m[13]=p.y;m[14]=p.z;return m;}
 void recover(){pose.stunt_hits.reset();pose.stunt_grounded=false;const Mat previous_car=model();ramp_flight=false;recent_log_support=0;pending_ejection.reset();ejection_cooldown=1.f;ragdoll.reset();pose.driver_bones.reset();pose.collision_events.reset();sound_id=0;pose.collision_contacts.reset();if(driver){world.removeRigidBody(driver.get());driver.reset();driver_shape.reset();}pose.driver_ejected=false;pose.driver_velocity={};pose.driver_acceleration={};pose.driver_segment_speed=0;previous_driver_velocity={};filtered_driver_acceleration={};driver_velocity_valid=false;pose.damage=engine_damage.value();pose.nitro_active=false;pose.velocity={};pose.speed=0;btTransform t;t.setFromOpenGLMatrix(spawn.data());t.setOrigin(t.getOrigin()+t.getBasis()*bt(cfg.center_mass));car->setWorldTransform(t);car->setInterpolationWorldTransform(t);car->setLinearVelocity({0,0,0});car->setAngularVelocity({0,0,0});car->clearForces();if(panels){panels->recover_attached(previous_car);pose.panels=panels->snapshot();}world.getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(car->getBroadphaseHandle(),&dispatcher);world.updateSingleAabb(car.get());omega.fill(0);angle.fill(0);steering_input=0;pose.steer=0;for(int i=0;i<4;i++){btTransform child;child.setIdentity();child.setOrigin(bt(suspension_mount(cfg.wheels[i])-cfg.center_mass));shape->updateChildTransform(wheel_children[i],child,false);}shape->recalculateLocalAabb();world.updateSingleAabb(car.get());for(int i=0;i<4;i++)last_length[i]=cfg.wheels[i].rest;pose.car=model();pose.rpm=cfg.idle_rpm;pose.gear=1;drivetrain.reset(cfg);++recovery_count;pose.recovery_generation=recovery_count;pose.wheel_contacts={};pose.slip={};pose.wheel_load={};for(size_t i=0;i<4;i++){pose.suspension[i]=cfg.wheels[i].rest;pose.wheels[i]=pose.wheel_mounts[i]=multiply(pose.car,translate(suspension_mount(cfg.wheels[i])));}}
 bool recover_at(const Mat&target,bool anchor_height){
  if(derby&&engine_damage.value()>=1)return false;
  for(float v:target)if(!std::isfinite(v))return false;
  auto forward=direction(target,{0,0,1});forward.y=0;if(length(forward)<.01f)return false;
  Mat candidate=rotate_y(std::atan2(forward.x,forward.z));candidate[12]=target[12];candidate[14]=target[14];
  const float level=anchor_height?target[13]:model()[13];float floor=-1e20f,low=1e20f;
  float lowest=cfg.lo.y;for(const auto&w:cfg.wheels)lowest=std::min(lowest,suspension_mount(w).y-w.radius);
  btVector3 bounds_low,bounds_high;track->getCollisionShape()->getAabb(track->getWorldTransform(),bounds_low,bounds_high);
  auto ground_at=[&](Vec p,float&height){
   for(auto range:{std::pair<float,float>{level+6,level-12},{bounds_high.y()+1,bounds_low.y()-1}}){
    btVector3 from(p.x,range.first,p.z),to(p.x,range.second,p.z);
    btCollisionWorld::AllHitsRayResultCallback ray(from,to);ray.m_collisionFilterGroup=2;ray.m_collisionFilterMask=1;world.rayTest(from,to,ray);
    float nearest=1e20f;bool found=false;for(int i=0;i<ray.m_hitPointWorld.size();i++)if(std::abs(ray.m_hitNormalWorld[i].y())>=.65f){float h=ray.m_hitPointWorld[i].y(),distance=std::abs(h-(level+lowest));if(distance<nearest){nearest=distance;height=h;found=true;}}if(found)return true;
   }return false;
  };
  for(size_t i=0;i<5;i++){auto p=i?point(candidate,cfg.wheels[i-1].mount):point(candidate,{});float h;if(!ground_at(p,h))return false;floor=std::max(floor,h);low=std::min(low,h);}
  if(floor-low>1.2f)return false;candidate[13]=floor-lowest+.08f;
  // Probe the restored wheel poses, not a crashed wheel's previous droop/steer.
  btCompoundShape restored_shape;for(int i=0;i<shape->getNumChildShapes();i++){auto child=shape->getChildTransform(i);if(i>=wheel_children[0]&&i<=wheel_children[3]){child.setIdentity();child.setOrigin(bt(suspension_mount(cfg.wheels[size_t(i-wheel_children[0])])-cfg.center_mass));}restored_shape.addChildShape(child,shape->getChildShape(i));}
  btCollisionObject probe;probe.setCollisionShape(&restored_shape);btTransform transform;transform.setFromOpenGLMatrix(candidate.data());transform.setOrigin(transform.getOrigin()+transform.getBasis()*bt(cfg.center_mass));probe.setWorldTransform(transform);
  struct Obstacle:btCollisionWorld::ContactResultCallback {const btCollisionObject*car;bool blocked=false;explicit Obstacle(const btCollisionObject*c):car(c){m_collisionFilterGroup=2;m_collisionFilterMask=1|2|4|16;}bool needsCollision(btBroadphaseProxy*p)const override{return p->m_clientObject!=car&&ContactResultCallback::needsCollision(p);}btScalar addSingleResult(btManifoldPoint&p,const btCollisionObjectWrapper*,int,int,const btCollisionObjectWrapper*,int,int)override{if(p.getDistance()<-.015f)blocked=true;return 0;}} obstacle(car.get());world.contactTest(&probe,obstacle);if(obstacle.blocked)return false;
  auto old_spawn=spawn;spawn=candidate;recover();spawn=old_spawn;return true;
 }
 void eject_driver(std::optional<Launch>source={},float stunt_angle=-1){if(driver||ragdoll)return;
  auto m=source?source->transform:model();Vec linear=source?source->linear:vec(car->getLinearVelocity()),angular=source?source->angular:vec(car->getAngularVelocity()),center=source?source->center:vec(car->getCenterOfMassPosition());
  // Preserve pre-impact COM/angular motion and the exact seated pose. The small
  // forward/up release is an explicit adapter, not an authored native impulse:
  // old OpenGL used a minimum 4 m/s forward boost; manual release uses the same.
  Vec launch=linear+direction(m,Vec{0,2,4});
  DriverPose seated;
  if(driver_model)seated=sample_seated_driver_pose(*driver_model,driver_placement,pose.steer);
  if(stunt&&stunt_angle>=0){
   const float angle=std::clamp(stunt_angle,0.f,stunt_max_launch_angle)*3.14159265f/180;
   const Vec heading=unit({m[8],0,m[10]},{0,0,1});
   const float power=std::clamp(length(linear)*1.20f+6.f,6.f,54.f);
   launch=heading*(std::cos(angle)*power)+Vec{0,std::sin(angle)*power+std::max(0.f,linear.y),0};
   angular={};
  }
  if(driver_model){float native_mass=0;for(const auto&part:driver_model->segments)native_mass+=part.mass;ragdoll=std::make_unique<NativeDriverRagdoll>(world,*driver_model,seated,m,launch,angular,native_mass,center,car.get());pose.driver_ejected=true;pose.driver_bones=std::make_shared<const std::array<Mat,11>>(ragdoll->snapshot());pose.driver=(*pose.driver_bones)[0];pose.nitro_active=false;return;}
  driver_shape=std::make_unique<btCapsuleShape>(.22f,1.1f);btVector3 inertia;driver_shape->calculateLocalInertia(80,inertia);btRigidBody::btRigidBodyConstructionInfo ci(80,nullptr,driver_shape.get(),inertia);driver=std::make_unique<btRigidBody>(ci);btTransform t;t.setFromOpenGLMatrix(m.data());t.setOrigin(bt(point(m,{0,1.25f,1.2f})));driver->setWorldTransform(t);driver->setInterpolationWorldTransform(t);driver->setLinearVelocity(bt(launch)+bt(angular).cross(t.getOrigin()-bt(center)));driver->setAngularVelocity(bt(angular));driver->setDamping(.03f,.08f);driver->setFriction(.6f);driver->setCcdMotionThreshold(.1f);driver->setCcdSweptSphereRadius(.18f);driver->setIgnoreCollisionCheck(car.get(),true);world.addRigidBody(driver.get(),8,1|2|4|16);pose.driver_ejected=true;pose.nitro_active=false;
 }
 void prepare(Input in,float dt){if(!std::isfinite(dt)||dt<=0||dt>.02f)throw std::runtime_error("Invalid physics step");in.throttle=std::clamp(in.throttle,-1.f,1.f);in.steer=std::clamp(in.steer,-1.f,1.f);in.brake=std::clamp(in.brake,0.f,1.f);in.handbrake=std::clamp(in.handbrake,0.f,1.f);
 pose.collision_contacts.reset();engine_damage.tick(dt);ejection_cooldown=std::max(0.f,ejection_cooldown-dt);if(in.eject&&!eject_held)eject_driver({},in.stunt_angle);eject_held=in.eject;if(pose.driver_ejected){in.throttle=0;in.steer=0;in.nitro=false;in.shift=0;steering_input=0;}Mat m=model();Vec up=unit(direction(m,{0,1,0})),forward=unit(direction(m,{0,0,1}));float speed=dot(vec(car->getLinearVelocity()),forward);float motor=drivetrain.step(cfg,omega,speed,in.throttle,in.brake,in.handbrake,in.automatic,in.shift,in.clutch,dt);pose.rpm=drivetrain.rpm;pose.gear=drivetrain.gear;in.brake=std::max(in.brake,drivetrain.direction_brake);
 float steer=steering_step(steering_input,in.steer,speed,cfg,handling,dt);pose.steer=steer;
 Vec vlocal=vec(car->getWorldTransform().getBasis().transpose()*car->getLinearVelocity());car->applyForce(bt(direction(m,aerodynamic_drag(cfg,vlocal))),bt(direction(m,cfg.aero_location-cfg.center_mass)));car->applyForce(bt(up*(-cfg.downforce*speed*speed)),bt(direction(m,cfg.downforce_location-cfg.center_mass)));
 pose.nitro_active=!derby&&cfg.nitro_capacity>0&&cfg.nitro_g>0&&in.nitro&&pose.nitro>0&&in.throttle>.1f;if(pose.nitro_active){car->applyCentralForce(bt(forward*(cfg.mass*9.81f*cfg.nitro_g)));pose.nitro=std::max(0.f,pose.nitro-dt/cfg.nitro_capacity);}
 auto previous_suspension=last_length;bool on_log=false;pose.wheel_contacts={};for(int i=0;i<4;i++){auto&w=cfg.wheels[i];Vec origin=point(m,suspension_mount(w)+Vec{0,w.rest,0});Vec fw=unit(direction(m,direction(rotate_y(i<2?steer:0),{0,0,1})));Vec hit{},normal{};const btCollisionObject*support=nullptr;
  auto orientation=multiply(m,rotate_y(i<2?steer:0));btTransform from,to;from.setFromOpenGLMatrix(orientation.data());to=from;
  // Sweep from the attachment through the complete authored suspension travel.
  from.setOrigin(bt(origin));to.setOrigin(bt(origin-up*w.max));WheelSweep sweep(from.getOrigin(),to.getOrigin(),car.get(),bt(up));world.convexSweepTest(wheel_shapes[i].get(),from,to,sweep,0);
  auto plane=scene->support_normals.plane_support(*scene->track_shape,*wheel_shapes[i],from,up,w.max);
  // Prefer exact interior support on the same shallow plane; retain earlier
  // convex edge/curb hits, and any nearer dynamic obstacle.
  if(plane.hit){bool replace=!sweep.hasHit();if(sweep.hasHit()&&sweep.m_hitCollisionObject==track.get()){
   if(sweep.triangle==plane.triangle)replace=true;
   else if(sweep.triangle>=0&&size_t(sweep.triangle)<track_normals.size()){
    Vec authored=track_normals[size_t(sweep.triangle)],n=dot(authored,up)<0?authored*-1:authored;
    // Compare authored plane equations, not GJK's approximate hit position.
    // Coplanar triangle seams must not defeat the exact interior support with
    // an off-plane convex result; distinct curb/edge planes remain sweeps.
    const bool same_plane=dot(n,plane.normal)>.999f&&std::abs(dot(authored,plane.point)-track_planes[size_t(sweep.triangle)])<.002f;
    replace=same_plane||plane.travel+.015f<w.max*float(sweep.m_closestHitFraction);
   }
  }else if(sweep.hasHit())replace=plane.travel<w.max*float(sweep.m_closestHitFraction);
  if(replace){sweep.m_closestHitFraction=plane.travel/w.max;sweep.m_hitCollisionObject=track.get();sweep.triangle=plane.triangle;sweep.m_hitPointWorld=bt(plane.point);sweep.m_hitNormalWorld=bt(plane.normal);}}
  bool contact=sweep.hasHit();float suspension=contact?std::clamp(float(sweep.m_closestHitFraction)*w.max,w.min,w.max):w.max;
  if(contact){hit=vec(sweep.m_hitPointWorld);normal=unit(vec(sweep.m_hitNormalWorld));support=sweep.m_hitCollisionObject;if(support==track.get()&&sweep.triangle>=0&&size_t(sweep.triangle)<track_normals.size()){auto authored=track_normals[size_t(sweep.triangle)];auto n=dot(authored,up)<0?authored*-1:authored;if(dot(n,up)>min_wheel_support_up){normal=n;hit=hit-authored*(dot(authored,hit)-track_planes[size_t(sweep.triangle)]);normal=scene->support_normals.sample(*scene->track_shape,hit,up,fw,w.width,w.radius,sweep.triangle);}}}
  pose.suspension[i]=suspension;float wheel_inertia=.5f*w.mass*w.inertia_factor*w.radius*w.radius;float share=i<2?cfg.front_drive*.5f:(1-cfg.front_drive)*.5f;omega[i]+=motor*share/wheel_inertia*dt;
  float brake_torque=cfg.brake_torque*in.brake*(i<2?cfg.brake_front*.5f:(1-cfg.brake_front)*.5f)+(i>=2?cfg.handbrake*in.handbrake*.5f:0);float brake=brake_torque*dt/wheel_inertia;omega[i]-=sign(omega[i])*std::min(std::abs(omega[i]),brake);float load=0;
  if(contact){auto relative=bt(hit)-car->getCenterOfMassPosition();auto*other=const_cast<btRigidBody*>(btRigidBody::upcast(support));btVector3 other_relative=other?bt(hit)-other->getCenterOfMassPosition():btVector3(0,0,0);Vec velocity=vec(car->getVelocityInLocalPoint(relative));if(other)velocity=velocity-vec(other->getVelocityInLocalPoint(other_relative));float vertical=dot(velocity,normal);float antiroll=std::clamp((previous_suspension[i^1]-previous_suspension[i])*std::min(w.k,cfg.wheels[i^1].k)*handling.rollbar[size_t(i/2)],-cfg.mass*9.81f*.2f,cfg.mass*9.81f*.2f);load=suspension_force(w,suspension,vertical,cfg.mass,antiroll)/std::max(.5f,dot(normal,up));car->applyForce(bt(normal*load),relative);if(other&&other->getInvMass()>0){other->activate();other->applyForce(bt(normal*(-load)),other_relative);}
   Vec longitudinal=unit(fw-normal*dot(fw,normal),fw),lateral=unit(cross(normal,longitudinal));float vl=dot(velocity,longitudinal),vt=dot(velocity,lateral);TireSurfaceDynamics surface;int surface_id=2;if(support==track.get()&&sweep.triangle>=0&&size_t(sweep.triangle)<track_materials.size()){auto material=track_materials[size_t(sweep.triangle)];if(material<scene->material_surfaces.size())surface_id=scene->material_surfaces[material];}else if(props->is_wood_log(support)){surface_id=24;on_log=on_log||load>100;}auto surface_it=cfg.tire_surfaces.find(surface_id);if(surface_it!=cfg.tire_surfaces.end())surface=surface_it->second;float maximum=cfg.mu*load*dt*grip_scale;float maximum_long=maximum*cfg.tire_dynamics.longitudinal*surface.longitudinal,maximum_lat=maximum*cfg.tire_dynamics.lateral*surface.lateral;float long_denom=car->computeImpulseDenominator(bt(hit),bt(longitudinal))+(other?other->computeImpulseDenominator(bt(hit),bt(longitudinal)):0)+w.radius*w.radius/wheel_inertia;float desired=(omega[i]*w.radius-vl)/std::max(long_denom,1e-6f);float chassis_denom=long_denom-w.radius*w.radius/wheel_inertia;if(brake_torque>0&&std::abs(vl)>.01f)desired-=sign(vl)*std::min(std::abs(vl)/std::max(chassis_denom,1e-6f),brake_torque*dt/std::max(.01f,w.radius));auto lateral_relative=bt(lateral_roll_point(vec(relative),up));auto lateral_at=car->getCenterOfMassPosition()+lateral_relative;float lat_denom=std::max(float(car->computeImpulseDenominator(lateral_at,bt(lateral))+(other?other->computeImpulseDenominator(bt(hit),bt(lateral)):0)),1e-6f);float lateral_demand=-vt*(1-std::exp(-dt/.040f))/lat_denom;if(cfg.tire_dynamics.authored){const auto&t=cfg.tire_dynamics;float slip=(omega[i]*w.radius-vl)/std::max(2.f,std::abs(vl));float angle=std::atan2(vt,std::max(2.f,std::abs(vl)));float brush_long=maximum_long*tire_brush_response(slip/std::max(.001f,t.optimal_slip_ratio));float brush_lat=-maximum_lat*tire_lateral_response(angle/std::max(.001f,t.optimal_slip_angle*(3.14159265f/180)));if(brake_torque<=0)desired=std::copysign(std::min(std::abs(desired),std::abs(brush_long)),desired);lateral_demand=std::copysign(std::min(std::abs(lateral_demand),std::abs(brush_lat)),lateral_demand);}auto grip=combined_tire_impulse(desired,lateral_demand,maximum_long,maximum_lat);float impulse=grip.longitudinal,lateral_impulse=grip.lateral;car->applyImpulse(bt(longitudinal*impulse),relative);car->applyImpulse(bt(lateral*lateral_impulse),lateral_relative);if(other&&other->getInvMass()>0)other->applyImpulse(bt(longitudinal*(-impulse)+lateral*(-lateral_impulse)),other_relative);omega[i]-=impulse*w.radius/wheel_inertia;omega[i]-=sign(omega[i])*std::min(std::abs(omega[i]),surface.rolling_resistance*load*w.radius*dt/wheel_inertia);pose.slip[i]=(omega[i]*w.radius-vl)/std::max(2.f,std::abs(vl));auto&c=pose.wheel_contacts[i];c.surface_id=surface_id;c.grounded=true;c.point=hit;c.normal=normal;c.velocity=velocity;c.load=load;c.slip=omega[i]*w.radius-vl;c.lateral_speed=vt;c.speed=std::abs(vl);c.width=w.width;if(support==track.get()&&sweep.triangle>=0&&size_t(sweep.triangle)<track_materials.size()){auto id=size_t(sweep.triangle);c.material=track_materials[id];auto n=track_normals[id];c.point=hit-n*(dot(n,hit)-track_planes[id]);c.normal=normal;}
 if(water_object){struct Ray:btCollisionWorld::ClosestRayResultCallback{int triangle=-1;using ClosestRayResultCallback::ClosestRayResultCallback;btScalar addSingleResult(btCollisionWorld::LocalRayResult&r,bool n)override{if(r.m_hitFraction<=m_closestHitFraction)triangle=r.m_localShapeInfo?r.m_localShapeInfo->m_triangleIndex:-1;return ClosestRayResultCallback::addSingleResult(r,n);}};auto a=bt(hit+up*(2*w.radius)),b=bt(hit-up*.015f);Ray ray(a,b);btTransform from,to;from.setIdentity();to.setIdentity();from.setOrigin(a);to.setOrigin(b);btCollisionWorld::rayTestSingle(from,to,water_object.get(),water_shape.get(),btTransform::getIdentity(),ray);if(ray.hasHit()&&ray.triangle>=0&&size_t(ray.triangle)<water_materials.size()){c.material=water_materials[ray.triangle];c.surface_id=28;auto water_point=vec(ray.m_hitPointWorld);c.water_depth=std::max(0.f,dot(water_point-c.point,up));c.point=water_point;c.normal=unit(vec(ray.m_hitNormalWorld));if(dot(c.normal,up)<0)c.normal=c.normal*-1;}}

  }else{omega[i]*=std::exp(-dt*.4f);pose.slip[i]=0;}angle[i]=std::fmod(angle[i]+omega[i]*dt,6.2831853f);pose.wheel_load[i]=load;last_length[i]=suspension;auto local=multiply(translate(suspension_mount(w)+Vec{0,w.rest-suspension,0}-cfg.center_mass),rotate_y(i<2?steer:0));btTransform child;child.setFromOpenGLMatrix(local.data());shape->updateChildTransform(wheel_children[i],child,false);
 }
 // Contacts are cached in chassis space, while suspension changes child poses.
 // Discard old wheel-child poses; retain chassis contact impulses for stable warmstarting.
 for(int n=0;n<dispatcher.getNumManifolds();n++){auto*manifold=dispatcher.getManifoldByIndexInternal(n);if(manifold->getBody0()==car.get()||manifold->getBody1()==car.get()){bool first=manifold->getBody0()==car.get();for(int j=manifold->getNumContacts()-1;j>=0;--j){auto&c=manifold->getContactPoint(j);int child=first?c.m_index0:c.m_index1;if(child>=wheel_children[0]&&child<=wheel_children[3])manifold->removeContactPoint(j);}}}
 shape->recalculateLocalAabb();world.updateSingleAabb(car.get());int grounded=0;
 for(const auto&c:pose.wheel_contacts)grounded+=c.grounded&&c.load>100;
 recent_log_support=on_log?.10f:std::max(0.f,recent_log_support-dt);
 if(grounded>=2){float yaw=float(car->getAngularVelocity().dot(bt(up)));float assist=handling_assist(cfg,speed);float desired_yaw=std::tan(steer)*speed/std::max(1.f,cfg.wheels[0].mount.z-cfg.wheels[2].mount.z);desired_yaw=std::clamp(desired_yaw,-1.5f,1.5f);car->applyTorque(bt(up*((-yaw*.31f+(desired_yaw-yaw)*assist)*cfg.inertia.y)));if(grounded>=3){float roll=float(car->getAngularVelocity().dot(bt(forward)));car->applyTorque(bt(forward*(-roll*cfg.inertia.z*.75f)));}}
 // Dampen pitch and roll once all tires unload during an upward launch
 // or when leaving a log crest at speed. Small log crests can unload
 // the wheels before the chassis reaches the ordinary vertical threshold.
 // No lift force, steering in the air or forced chassis rotation.
 // A touchdown ends the assistance; tire/suspension forces take over.
 if(grounded||pose.driver_ejected||up.y<.55f)ramp_flight=false;
 else if((speed>7.f&&car->getLinearVelocity().y()>1.25f)||
         (recent_log_support>0&&std::abs(speed)>7.f))ramp_flight=true;
 if(ramp_flight){
  const Vec right=unit(direction(m,{1,0,0}));
  const auto angular=vec(car->getAngularVelocity());
  car->applyTorque(bt(right*(-2.5f*dot(angular,right)*cfg.inertia.x)
                      +forward*(-2.f*dot(angular,forward)*cfg.inertia.z)));
 }
 // Two contact substeps are a stability choice, not a recovered native solver.
 }
 void update_stunt_contacts(){
  if(!stunt||!pose.driver_ejected)return;
  const auto* bones=ragdoll?&ragdoll->bodies():nullptr;
  auto belongs=[&](const btCollisionObject* body){
   return body==driver.get()||(bones&&std::find(bones->begin(),bones->end(),body)!=bones->end());
  };
  std::shared_ptr<std::vector<size_t>> hits;
  for(int i=0;i<dispatcher.getNumManifolds();++i){
   const auto* contact=dispatcher.getManifoldByIndexInternal(i);
   const auto* a=contact->getBody0();const auto* b=contact->getBody1();
   const btCollisionObject* other=belongs(a)?b:belongs(b)?a:nullptr;
   if(!other)continue;
   const auto index=props->index(other);
   for(int j=0;j<contact->getNumContacts();++j){
    const auto& p=contact->getContactPoint(j);
    if(p.getDistance()>.015f||p.getAppliedImpulse()<=.05f)continue;
    if(other==track.get()||index){
     const auto normal=other==b?p.m_normalWorldOnB:-p.m_normalWorldOnB;
     if(normal.y()>.35f)pose.stunt_grounded=true;
    }
    if(index){
     if(!hits)hits=pose.stunt_hits?std::make_shared<std::vector<size_t>>(*pose.stunt_hits):std::make_shared<std::vector<size_t>>();
     if(std::find(hits->begin(),hits->end(),*index)==hits->end())hits->push_back(*index);
    }
    break;
   }
  }
  if(hits)pose.stunt_hits=std::move(hits);
 }
 void reset_stunt_attempt(const std::vector<PropDefinition>& definitions){
  if(!stunt)return;
  panels->clear();engine_damage.reset();pose.impacts.reset();pose.damage=0;pose.nitro=1;pose.headlight_health={1,1};
  eject_held=false;
  recover();
  props.reset();props=std::make_unique<PropPhysics>(world,definitions);
  pose.props=props->snapshot();nitro_touches.clear();
 }
 void finish(float dt){panels->update(pose.collision_contacts?*pose.collision_contacts:std::vector<VehicleDamageContact>{},dt);pose.panels=panels->snapshot();pose.damage=engine_damage.value();if(pending_ejection){eject_driver(pending_ejection);pending_ejection.reset();}if(primary&&derby&&pose.damage>=1&&!driver&&!ragdoll)eject_driver();pose.velocity=vec(car->getLinearVelocity());pose.props=props->snapshot();if(driver)driver->getWorldTransform().getOpenGLMatrix(pose.driver.data());
 // In the 11-body native ragdoll the root segment supplies driver motion.
 // Never infer driver acceleration from the car or elapsed launch time.
 btRigidBody* root=ragdoll&&!ragdoll->bodies().empty()?ragdoll->bodies().front():driver.get();
 if(root){
  // Mass-weighted ragdoll center-of-mass velocity prevents one motionless
  // segment from ending a stunt while the limbs are still in motion.
  Vec mean{};float total_mass=0,max_segment_speed=0;
  if(ragdoll)for(const auto* part:ragdoll->bodies()){
   const float inv_mass=float(part->getInvMass());
   if(inv_mass<=0)continue;
   const float mass=1.f/inv_mass;
   const Vec velocity=vec(part->getLinearVelocity());
   mean=mean+velocity*mass;total_mass+=mass;
   max_segment_speed=std::max(max_segment_speed,length(velocity));
  }
  else{mean=vec(root->getLinearVelocity());total_mass=1;max_segment_speed=length(mean);}
  pose.driver_velocity=total_mass>0?mean*(1.f/total_mass):Vec{};
  pose.driver_segment_speed=max_segment_speed;
  const Vec raw_acceleration=driver_velocity_valid&&dt>0?
      (pose.driver_velocity-previous_driver_velocity)*(1.f/dt):Vec{};
  const float blend=1.f-std::exp(-dt/.12f);
  filtered_driver_acceleration=filtered_driver_acceleration+(raw_acceleration-filtered_driver_acceleration)*blend;
  pose.driver_acceleration=filtered_driver_acceleration;
  previous_driver_velocity=pose.driver_velocity;driver_velocity_valid=true;
 }
 else{pose.driver_velocity={};pose.driver_acceleration={};pose.driver_segment_speed=0;driver_velocity_valid=false;filtered_driver_acceleration={};}
 if(ragdoll){auto bones=ragdoll->snapshot();if(!pose.driver_bones||*pose.driver_bones!=bones)pose.driver_bones=std::make_shared<const std::array<Mat,11>>(bones);pose.driver=(*pose.driver_bones)[0];}pose.car=model();pose.speed=dot(vec(car->getLinearVelocity()),unit(direction(pose.car,{0,0,1})));pose.time+=dt;
 for(int i=0;i<4;i++){auto&w=cfg.wheels[i];auto local=multiply(translate(suspension_mount(w)+Vec{0,w.rest-pose.suspension[i],0}),multiply(rotate_y(i<2?pose.steer:0),rotate_x(angle[i])));pose.wheels[i]=multiply(pose.car,local);pose.wheel_mounts[i]=multiply(pose.car,multiply(translate(suspension_mount(w)+Vec{0,w.rest-pose.suspension[i],0}),rotate_y(i<2?pose.steer:0)));}for(float x:pose.car)if(!std::isfinite(x))throw std::runtime_error("Nonfinite simulation state");
 }
 void step(Input in,float dt){
  pose.stunt_grounded=false;pose.stunt_hits.reset();
  prepare(in,dt);std::vector<AIVehiclePose>traffic;traffic.push_back({pose.car,vec(car->getLinearVelocity()),pose.damage});for(auto&b:bots)traffic.push_back({b->vehicle->model(),vec(b->vehicle->car->getLinearVelocity()),b->finished?1.f:b->vehicle->pose.damage});
  ai_commands.resize(bots.size());
  const auto ai_start=std::chrono::steady_clock::now();
  ai_lanes.for_each(bots.size(),[&](size_t i){auto&b=*bots[i];ai_commands[i]=b.finished?AICommand{}:b.controller.update(traffic[i+1],traffic,dt);});
  ai_plan_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-ai_start).count();
  for(size_t i=0;i<bots.size();++i){auto&b=*bots[i];const auto&command=ai_commands[i];if(command.recover){
   // Native spline Y is often an editor handle height (0/20), not road elevation.
   // Recover against actual shared track triangles at the current road level.
   auto&vehicle=*b.vehicle;auto p=point(command.recovery,{});float level=vehicle.model()[13];btVector3 from(p.x,level+8,p.z),to(p.x,level-60,p.z);btCollisionWorld::ClosestRayResultCallback ground(from,to);ground.m_collisionFilterGroup=2;ground.m_collisionFilterMask=1;world.rayTest(from,to,ground);if(ground.hasHit()){command.recovery[13]=ground.m_hitPointWorld.y()+.6f;vehicle.spawn=command.recovery;}vehicle.recover();}if(b.vehicle->drivetrain.gear<1){b.vehicle->drivetrain.gear=1;b.vehicle->pose.gear=1;}Input control;control.throttle=command.throttle;control.brake=command.brake;control.steer=command.steer;if(b.finished){control.brake=1;control.handbrake=1;}b.vehicle->prepare(control,dt);}
  props->set_contact_step(dt*.5f);world.stepSimulation(dt,2,dt*.5f);props->update_rest(dt);finish(dt);for(auto&b:bots)b->vehicle->finish(dt);publish_opponents();
 }
 void publish_opponents(){auto snapshot=std::make_shared<std::vector<OpponentState>>();for(auto&b:bots){const auto&p=b->vehicle->pose;OpponentState state;state.id=b->config.id;state.car_id=b->config.car_id;state.skin=b->config.skin;state.name=b->config.name;state.car=p.car;state.wheels=p.wheels;state.panels=p.panels;state.velocity=p.velocity;state.speed=p.speed;state.damage=p.damage;state.headlight_health=p.headlight_health;state.recovery_generation=p.recovery_generation;snapshot->push_back(std::move(state));}pose.opponents=std::move(snapshot);
 }
 void set_opponents(std::vector<RaceOpponentConfig>configs,RaceRoute route){bots.clear();pose.opponents.reset();for(auto&c:configs){auto b=std::make_unique<Bot>(std::move(c),route);Geometry empty;empty.source=derby?"/derby/":"";b->vehicle=std::make_unique<Impl>(empty,b->config.vehicle,b->config.spawn,b->config.panels,nullptr,DriverPlacement{},scene);b->vehicle->grip_scale=grip_scale;b->controller.set_grip_scale(grip_scale);b->vehicle->finish(0);bots.push_back(std::move(b));}publish_opponents();}

};
Simulation::Simulation(const Geometry&g,VehicleConfig c,Mat m,std::vector<VehiclePanelDefinition>p,std::shared_ptr<const NativeDriver>dm,DriverPlacement dp):p_(std::make_unique<Impl>(g,std::move(c),m,std::move(p),std::move(dm),dp)){}Simulation::~Simulation()=default;void Simulation::step(Input i,float dt){p_->step(i,dt);}
void Simulation::set_cpu_lanes(unsigned lanes){p_->ai_lanes.configure(p_->bots.size()>=4?lanes:1);}
unsigned Simulation::cpu_lanes()const{return p_->ai_lanes.lanes();}State Simulation::state()const{return p_->pose;}void Simulation::set_opponents(std::vector<RaceOpponentConfig>c,RaceRoute r){p_->set_opponents(std::move(c),std::move(r));}void Simulation::set_opponent_finished(size_t index,bool finished){if(index<p_->bots.size())p_->bots[index]->finished=finished;}void Simulation::recover(){if(!(p_->derby&&p_->engine_damage.value()>=1))p_->recover();}bool Simulation::recover(const Mat&target,bool anchor_height){return p_->recover_at(target,anchor_height);}void Simulation::reset_stunt_attempt(const std::vector<PropDefinition>&props){p_->reset_stunt_attempt(props);}
void Simulation::set_remote_colliders(const std::vector<RemoteCollider>& remote){p_->update_remote_bodies(remote);}
void Simulation::set_prop_rewards_enabled(bool enabled){p_->prop_rewards_enabled=enabled;}void Simulation::set_grip_scale(float scale){p_->grip_scale=std::isfinite(scale)?std::clamp(scale,.75f,1.f):1.f;for(auto&bot:p_->bots){bot->vehicle->grip_scale=p_->grip_scale;bot->controller.set_grip_scale(p_->grip_scale);}}Json Simulation::diagnostics()const{return{{"drivetrain",{{"model","native-parameter rotor with slipping clutch adapter"},{"gear",p_->drivetrain.gear},{"clutch_engagement",p_->drivetrain.clutch},{"clutch_torque_nm",p_->drivetrain.transmitted},{"shift_seconds",p_->drivetrain.shift_remaining},{"rev_limiter",p_->drivetrain.limiter},{"redline_rpm",p_->cfg.max_rpm},{"curve_points",p_->cfg.torque_curve.size()},{"reverse_ratio",p_->cfg.reverse_ratio},{"engine_inertia",p_->cfg.engine_inertia},{"engine_friction",p_->cfg.engine_friction}}},{"wheel_collision",{{"method","native tire envelope without spin in collision; width sweep + rounded curb support; visual tire spin retained"},{"native_hulls",int(std::count_if(p_->cfg.wheels.begin(),p_->cfg.wheels.end(),[](const auto&w){return !w.collision_vertices.empty();}))},{"source",p_->cfg.tire},{"dedicated_native_collision_mesh",false},{"synthetic_fallback","X-axis cylinder when no source mesh provided"}}},{"chassis_collision",{{"boxes",p_->cfg.has_top_collision?2:1},{"source","body.ini CollisionBottom/CollisionTop; bevel on stock bumper overhangs"}}},{"handling",{{"model","native-rate speed-sensitive steer with speed-scaled 1.25-1.40 input response and countersteer; authored anisotropic brush adapter with five-slope lateral response and 40ms dissipative relaxation; native-rate suspension with 5cm spring preload + native rollbar; lateral roll-center and grounded roll damping adapters"},{"suspension_lift_m",suspension_lift_m},{"suspension_spacer_m",-p_->cfg.wheels[0].arch_offset.y},{"ramp_flight_stabilization",true},{"steering_input",p_->steering_input},{"native_tuning",(p_->cfg.provenance.is_object()?p_->cfg.provenance.value("handling",Json::object()):Json::object())},{"grip_scale",p_->grip_scale},{"grounded_yaw_damping",.31},{"grounded_roll_damping",.75},{"lateral_roll_lever_fraction",.55},{"tire_parameters",(p_->cfg.provenance.is_object()?p_->cfg.provenance.value("tire_dynamics",Json::object()):Json::object())},{"mass_inertia_preserved",true}}},{"track_contacts",{{"support_height","authored coplanar plane-equation eligibility; analytic unchanged convex-tire/triangle interior plane support; convex sweep retained at edges, curbs, steep contacts and dynamic obstacles"},{"normal_source","five-sample barycentric authored normal interpolation; 20deg shared-edge and 4.5cm plane continuity bounds; preserve curved curb support"},{"chassis_warmstarting",true},{"geometry_modified",false}}},{"collision_nitro",{{"awards",p_->nitro_awards},{"earned_fraction",p_->nitro_earned},{"tracked_pairs",p_->nitro_touches.size()},{"policy","native prop nitro once per body; wall reward per contact episode with 0.2s separation and 0.75s cooldown; no derby nitro"}}},{"prop_rewards",{{"hits",p_->pose.prop_hits},{"cents",p_->prop_bonus_cents},{"payout",p_->pose.prop_bonus},{"hits_by_type",p_->pose.prop_hits_by_type},{"cents_by_type",p_->pose.prop_bonus_cents_by_type},{"enabled",p_->prop_rewards_enabled}}},{"opponents",p_->bots.size()},{"cpu_ai_lanes",p_->ai_lanes.lanes()},{"cpu_ai_plan_ms",p_->ai_plan_ms},{"physics_worlds",1},{"mass",p_->cfg.mass},{"authored_mass",p_->cfg.authored_mass},{"reference_profile",p_->cfg.profile},{"inertia",{p_->cfg.inertia.x,p_->cfg.inertia.y,p_->cfg.inertia.z}},{"time",p_->pose.time},{"rpm",p_->pose.rpm},{"speed_mps",p_->pose.speed},{"recoveries",p_->recovery_count},{"damage",p_->pose.damage},{"detached_panels",p_->panels->detached()},{"damage_contacts",p_->pose.impacts?p_->pose.impacts->size():0},{"damage_model","OpenGL contact gain, localized native crash targets; native panel mass/hinges and detached rigid bodies"},{"props",p_->props->diagnostics()},{"driver_ejected",p_->pose.driver_ejected},{"driver_velocity_mps",{p_->pose.driver_velocity.x,p_->pose.driver_velocity.y,p_->pose.driver_velocity.z}},{"driver_acceleration_mps2",{p_->pose.driver_acceleration.x,p_->pose.driver_acceleration.y,p_->pose.driver_acceleration.z}},{"driver_max_segment_speed_mps",p_->pose.driver_segment_speed},{"driver_release","adapter: frontal closing >=13.5 m/s, solved delta >=8 m/s; pre-impact COM motion; local +2 up/+4 forward velocity adapter; native release parity unverified"},{"driver_physics","native weighted driver, DriverLoc seat, 11 bodies/10 native joint adapters; authored segment masses; capsule only if native data unavailable"},{"limits","New custom wheel forces on Bullet contacts; native tire/contact/transmission parity not yet established"}};}
}
