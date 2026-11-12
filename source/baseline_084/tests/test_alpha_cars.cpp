#include "assets.hpp"
#include "vehicle_dynamics.hpp"
#include "vehicle_damage.hpp"
#include "vehicle_texture_resolver.hpp"
#include "simulation.hpp"
#include <btBulletCollisionCommon.h>
#include <iostream>
#include <stdexcept>
using namespace ofo;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
btVector3 bullet(Vec v){return {v.x,v.y,v.z};}
struct ArchFixture {
 btTriangleMesh faces;
 btDefaultCollisionConfiguration configuration;
 btCollisionDispatcher dispatcher{&configuration};
 btDbvtBroadphase broadphase;
 btCollisionWorld world{&dispatcher,&broadphase,&configuration};
 std::unique_ptr<btBvhTriangleMeshShape> arch_shape;
 btCollisionObject arch;
 explicit ArchFixture(const Geometry& body){
  for(const auto& mesh:body.meshes){
   if(body.materials[mesh.material].shader!=5)continue;
   const auto& name=mesh.name;
   // Axles, hubs, and underbody mechanical parts are not the arch shell.
   if(name.rfind("wheelhub",0)==0||name.rfind("susp",0)==0||
      name.rfind("exhaust",0)==0||name=="engine"||name=="driveshaft")continue;
   for(size_t i=0;i<mesh.indices.size();i+=3){
    auto v=[&](int n){return bullet(point(mesh.transform,mesh.vertices[mesh.indices[i+n]].position));};
    faces.addTriangle(v(0),v(1),v(2));
   }
  }
  arch_shape=std::make_unique<btBvhTriangleMeshShape>(&faces,true);
  arch_shape->setMargin(.001f);
  arch.setCollisionShape(arch_shape.get());
  arch.setWorldTransform(btTransform::getIdentity());
 }
 bool clears(const WheelConfig& wheel,const Mat& transform){
  btConvexHullShape tire;
  for(auto v:wheel.collision_vertices)tire.addPoint(bullet(v),false);
  tire.recalcLocalAabb();
  tire.setMargin(.001f);
  btCollisionObject probe;
  probe.setCollisionShape(&tire);
  btTransform pose;
  pose.setFromOpenGLMatrix(transform.data());
  probe.setWorldTransform(pose);
  struct Contact : btCollisionWorld::ContactResultCallback {
   bool penetrates=false;
   btScalar addSingleResult(btManifoldPoint& point,const btCollisionObjectWrapper*,int,int,
                            const btCollisionObjectWrapper*,int,int) override{
    penetrates=penetrates||point.getDistance()<-.005f;
    return 0;
   }
  } contact;
  world.contactPairTest(&probe,&arch,contact);
  return !contact.penetrates;
 }
};

}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("usage: test_alpha_cars native-data alpha-content-dir");
 Resources r(argv[1]);r.mount(argv[2],"alpha_content");
 Material paint;paint.name="body";paint.texture[0]="body.tga";
 require(resolve_vehicle_diffuse(r,"cars/car_3/body.bgm",paint,2)=="cars/car_3/skin2.dds","retail livery alias");
 require(resolve_vehicle_diffuse(r,"menu/menucar_3.bgm",paint,102)=="cars/car_3/skinai2.dds","showroom AI livery alias");
 require(resolve_vehicle_diffuse(r,"cars/car_3/body.bgm",paint,99)=="cars/car_3/skin1.dds","missing retail livery fallback");
 Material window;window.texture[0]="cars/shared/windows.dds";
 require(resolve_vehicle_diffuse(r,"cars/car_3/body.bgm",window,2)==window.texture[0],"glass keeps authored texture");
 Geometry road;road.materials.push_back({"tarmac"});Mesh asphalt;
 for(Vec p: {Vec{-15,0,-20},Vec{15,0,-20},Vec{15,0,160},Vec{-15,0,160}}){Vertex v;v.position=p;asphalt.vertices.push_back(v);}
 asphalt.indices={0,2,1,0,3,2};road.meshes.push_back(std::move(asphalt));
 const char* names[]={"BMW","CAMARO","BEETLE","BMW (ALPHA 4)"};
 for(int id=17;id<=20;++id){
  auto c=load_vehicle(r,id);auto b=load_geometry(r,c.body);auto w=load_geometry(r,c.tire);
  auto show=load_geometry(r,(fs::path(c.body).parent_path()/"model.showcar").generic_u8string());
  require(c.name==names[id-17]&&c.mass>=600&&c.mass<=1600&&c.ratios.size()>=4,"original alpha vehicle configuration");
  require(b.triangles>2500&&w.meshes.size()==4&&show.triangles==b.triangles+w.triangles,"body, four local wheels, full showroom");
  require(b.markers.count("steering_dummy_l")&&b.markers.count("steering_dummy_r"),"original driver placement markers");
  require(c.lo.y<c.hi.y&&c.has_top_collision&&c.top_lo.y<c.top_hi.y,"body-derived collision envelopes");
  ArchFixture arches(b);
  for(size_t corner=0;corner<c.wheels.size();++corner){
   const auto& wheel=c.wheels[corner];
   const auto display=std::find_if(show.meshes.begin(),show.meshes.end(),[&](const Mesh& m){return m.name==wheel.mesh_name;});
   require(display!=show.meshes.end(),"Showroom contains each tire");
   if(!arches.clears(wheel,display->transform))
    throw std::runtime_error(c.name+" "+wheel.mesh_name+" tire intersects the showroom fender");
   const auto center=point(display->transform,{});
   require(length(center-suspension_mount(wheel))<1e-5f,"Showroom and race use the same fitted wheel center");
   const float steer=corner<2?c.steering_angle*3.14159265f/180.f:0.f;
   for(float travel:{wheel.min,(wheel.min+wheel.rest)*.5f,wheel.rest,wheel.max}){
    for(int turn=-4;turn<=4;++turn){
     const auto moved=center+Vec{0,wheel.rest-travel,0};
     const auto steered=multiply(translate(moved),rotate_y(steer*float(turn)/4.f));
     for(float spin:{0.f,.065f,.13f}){
      if(!arches.clears(wheel,multiply(steered,rotate_x(spin))))
       throw std::runtime_error(c.name+" "+wheel.mesh_name+" tire crosses the arch during suspension/steering travel ("+
                                std::to_string(travel)+", "+std::to_string(turn)+")");
     }
    }
   }
  }
  for(const auto& wheel:c.wheels){
   require(wheel.collision_vertices.size()>50&&wheel.mount.y>.1f,"native wheel shape and mount");
   require(length(point(b.markers.at("placeholder_"+wheel.mesh_name),{})-wheel.mount)<1e-5f,"wheel bind transform matches marker");
  }
  for(const auto& m:b.meshes){
   const auto& mat=b.materials.at(m.material);
   require(!m.indices.empty()&&m.name.find("_crash")==std::string::npos&&m.name.find("_shadow")==std::string::npos,"only live LOD0 geometry");
   require(!mat.texture[0].empty()&&!r.texture(b.source,mat.texture[0]).empty(),"all rendered body meshes bind original textures");
   for(auto i:m.indices)require(i<m.vertices.size(),"CARC triangle in bounds");
  }
  const auto prototype_path=fs::u8path(c.body).parent_path().generic_u8string();
  auto fallback=resolve_vehicle_diffuse(r,c.body,paint,5);
  require(fallback.rfind(prototype_path+"/",0)==0&&r.exists(fallback),"prototype paint stays in its own car directory");
  const auto crash=load_vehicle_crash(r,b);
  require(crash.records>=13&&crash.moved>1000,"alpha CARC supplies authored deformation targets");
  for(const auto& target:crash.surfaces)
   require(target.prototype_profile&&target.positions.size()==b.meshes[target.mesh].vertices.size(),"alpha crash topology and impact profile");
  const auto bent=vehicle_damage_geometry(b,crash,1,{});
  require(std::any_of(crash.surfaces.begin(),crash.surfaces.end(),[&](const CrashTarget& target){
   const auto& clean=b.meshes[target.mesh];
   const auto& dent=bent.meshes[target.mesh];
   if(dent.indices!=clean.indices)return false;
   for(size_t j=0;j<clean.vertices.size();++j)
    if(length(clean.vertices[j].position-dent.vertices[j].position)>.01f)return true;
   return false;
  }),"alpha targets move the existing meshes without changing topology");
  auto panels=load_vehicle_panels(r,b);
  require(panels.size()>=4,"native alpha damage panel definitions");
  const auto hood=std::find_if(panels.begin(),panels.end(),[](const VehiclePanelDefinition& part){return part.name=="hood";});
  require(hood!=panels.end()&&hood->hinge&&hood->marker=="hood_joint"&&hood->break_impulse==std::array<float,4>{30000,25000,21000,30000}[size_t(id-17)],
   "alpha hood uses its CARC anchor and original release threshold");
  const float expected_angle=(id==19?15.f:5.f)*3.14159265f/180;
  require(std::abs(hood->upper_limit-hood->lower_limit-expected_angle)<.001f,"alpha hood uses its native angular range");
  apply_handling_difficulty(c,r,1);require(c.provenance["handling"]["difficulty_id"]==1,"alpha steering configuration");
  Simulation drive(road,c,translate({0,1,0}),panels);
  for(int i=0;i<150;++i)drive.step({});
  int loaded=0;for(const auto& contact:drive.state().wheel_contacts)loaded+=contact.grounded&&contact.load>100;
  require(loaded==4,"alpha suspension settles on all four tires");
  Input input;input.throttle=1;
  for(int i=0;i<400;++i)drive.step(input);
  auto state=drive.state();
  require(state.car[14]>10&&state.velocity.z>5&&!state.driver_ejected,"alpha vehicle drives under its own power");
  std::cout<<c.id<<" "<<c.name<<" mass="<<c.mass<<" body="<<b.triangles<<" wheels="<<w.triangles<<" panels="<<panels.size()<<" speed="<<state.velocity.z<<"\n";
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
