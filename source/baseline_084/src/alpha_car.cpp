#include "alpha_car.hpp"
#include "vehicle_dynamics.hpp"
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ofo {
namespace {
constexpr std::array<const char*,4> corners{{"tire_fl","tire_fr","tire_rl","tire_rr"}};
struct AlphaAxleFit {float left_out,right_out,forward,compressed_drop;};
struct AlphaWheelFit {float ride_drop;AlphaAxleFit front,rear;};
// Metres relative to the CARC centers. Full-bump bounds were checked against
// the original exterior triangles over the authored steering range.
constexpr std::array<AlphaWheelFit,4> arch_fit{{
 {.260f,{.026f,.023f,.045f,.224f},{0,0,0,.220f}},             // BMW
 {.176f,{.004f,.004f,-.012f,.146f},{.009f,.009f,-.008f,.105f}}, // Camaro
 {.207f,{.052f,.018f,.024f,.174f},{0,0,0,.074f}},              // Beetle
 {.203f,{-.006f,-.010f,-.016f,.176f},{.019f,.017f,-.005f,.150f}} // BMW Alpha 4
}};
enum class View { Body, Wheels, Showroom, Crash };
struct Cursor {
 const Bytes& bytes;
 std::string path;
 size_t offset=0;
 [[noreturn]] void fail(const char* msg)const{throw std::runtime_error(path+" at "+std::to_string(offset)+": "+msg);}
 void need(size_t n)const{if(n>bytes.size()-offset)fail("truncated CARC");}
 void skip(size_t n){need(n);offset+=n;}
 template<class T>T read(){need(sizeof(T));T v;std::memcpy(&v,bytes.data()+offset,sizeof v);offset+=sizeof v;return v;}
 uint32_t u32(){return read<uint32_t>();}
 uint32_t count(uint32_t max){auto n=u32();if(n>max)fail("CARC record count too large");return n;}
 float f32(){auto n=read<float>();if(!std::isfinite(n))fail("nonfinite CARC coordinate");return n;}
 Vec vec(){return {f32(),f32(),f32()};}
 Mat mat(){Mat m;for(auto&v:m)v=f32();return m;}
 void tag(const char* expected){need(4);if(std::memcmp(bytes.data()+offset,expected,4)!=0)fail(expected);offset+=4;}
 std::string str(){
  size_t first=offset;
  while(offset<bytes.size()&&bytes[offset]&&offset-first<512)++offset;
  if(offset==bytes.size()||bytes[offset])fail("unterminated CARC name");
  return std::string(reinterpret_cast<const char*>(bytes.data()+first),offset++-first);
 }
};
struct Bounds {
 Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
 void add(Vec p){for(int k=0;k<3;++k){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}}
 bool valid()const{return lo.x<hi.x&&lo.y<hi.y&&lo.z<hi.z;}
};
void bounds(Mesh& mesh){
 Bounds b;for(const auto&v:mesh.vertices)b.add(point(mesh.transform,v.position));
 mesh.center=(b.lo+b.hi)*.5f;mesh.radius=length(b.hi-b.lo)*.5f;
}
std::string car_texture(const Resources&r,const std::string&dir,const std::string& raw){
 if(raw.empty())return {};
 auto name=fs::path(clean_path(raw)).filename();name.replace_extension(".dds");
 auto leaf=name.generic_u8string();
 for(auto key:{dir+"/"+leaf,"alpha_content/cars/shared/"+leaf})if(r.exists(key))return key;
 return dir+"/"+leaf;
}
struct Model {std::vector<Mesh> batches;};
std::vector<Mesh> read_model(Cursor& f,size_t material_count,int& surface_id){
 f.tag("BMOD");f.skip(28);std::vector<Mesh> result;
 auto n=f.count(128);result.reserve(n);
 for(uint32_t j=0;j<n;++j){
  f.tag("BATC");f.u32();Mesh m;m.source_surface=surface_id++;
  m.material=f.u32();if(m.material>=material_count)f.fail("CARC material index out of bounds");
  auto verts=f.count(65535),flags=f.u32(),data_size=f.count(65535*64);
  if(verts==0||data_size%verts||!(flags&2)||flags&~(2u|16u|64u|256u|512u))f.fail("invalid CARC vertex stream");
  const auto stride=data_size/verts;
  const uint32_t used=12+((flags&16)?12:0)+((flags&64)?4:0)+((flags&(256|512))?8:0)+((flags&512)?8:0);
  if(used!=stride)f.fail("unsupported CARC vertex format");
  m.vertices.reserve(verts);
  for(uint32_t k=0;k<verts;++k){
   Vertex v;v.position=f.vec();
   v.normal=(flags&16)?unit(f.vec()):Vec{0,1,0};
   if(flags&64)v.color=f.u32();
   if(flags&(256|512)){v.u=f.f32();v.v=f.f32();}
   if(flags&512){v.lu=f.f32();v.lv=f.f32();}
   m.vertices.push_back(v);
  }
  f.u32();auto primitive=f.u32(),index_bytes=f.count(12*1024*1024);
  if(index_bytes%2||(primitive!=4&&primitive!=5))f.fail("unsupported CARC primitive");
  std::vector<uint16_t> indices;indices.reserve(index_bytes/2);
  for(uint32_t k=0;k<index_bytes/2;++k){
   auto i=f.read<uint16_t>();if(i>=verts)f.fail("CARC vertex index out of bounds");indices.push_back(i);
  }
  if(primitive==4){if(indices.size()%3)f.fail("malformed CARC triangle list");m.indices.assign(indices.begin(),indices.end());}
  else for(size_t k=2;k<indices.size();++k){
   auto a=indices[k-2],b=indices[k-1],c=indices[k];
   if(a==b||b==c||a==c)continue;
   if(k&1)std::swap(a,b);
   m.indices.insert(m.indices.end(),{a,b,c});
  }
  f.skip(24); // stored BMOD batch bounds; recalc after binding the pivot
  result.push_back(std::move(m));
 }
 return result;
}
float number(const Json&j,const std::string&key){
 auto x=j.at(key).get<float>();if(!std::isfinite(x))throw std::runtime_error("invalid alpha car setting: "+key);return x;
}
float positive(const Json&j,const std::string& key){
 auto x=number(j,key);if(x<=0)throw std::runtime_error("nonpositive alpha car setting: "+key);return x;
}
float tuple(const Json& j,const std::string&key,int index){return number(j.at(key),std::to_string(index));}
}

std::string alpha_car_directory(int id){
 if(!alpha_car_id(id))throw std::runtime_error("alpha car ID must be 17..20");
 return "alpha_content/cars/car_"+std::to_string(id-16);
}
Geometry load_alpha_car_geometry(const Resources&r,const std::string& path){
 auto dir=fs::path(path).parent_path().generic_u8string();
 auto filename=fs::path(path).filename().string();
 auto mode=filename=="model.wheels"?View::Wheels:filename=="model.showcar"?View::Showroom:filename=="model.crash"?View::Crash:View::Body;
 auto source=dir+"/model.car";auto bytes=r.read(source);Cursor f{bytes,source};f.tag("CARC");
 if(f.u32()!=0x10001)f.fail("unsupported CARC version");
 Geometry g;g.source=path;
 for(uint32_t i=0,n=f.count(1024);i<n;++i){
  f.tag("MATC");Material m;m.name=lower(f.str());
  auto alpha=f.u32();f.skip(12);auto shader=f.u32();f.skip(76);
  for(auto&tex:m.texture)tex=car_texture(r,dir,f.str());
  // Prototype's shaders 3/4 are body/glass. The OpenFO renderer uses its
  // compatible material modes; do not pass early shader IDs through unchanged.
  m.shader=shader==3?5:shader==4?6:3;
  m.alpha=alpha==5?1u:0u;
  // Keep MATC texture slots. dirt.dds is an authored secondary texture only where referenced.
  g.materials.push_back(std::move(m));
 }
 std::vector<Model> models;
 int surface=0;
 for(uint32_t i=0,n=f.count(4096);i<n;++i)models.push_back({read_model(f,g.materials.size(),surface)});
 for(uint32_t i=0,n=f.count(4096);i<n;++i){
  f.tag("MESH");auto name=lower(f.str());f.str();f.u32();auto frame=f.mat();
  auto count=f.count(128);uint32_t primary=0;
  for(uint32_t j=0;j<count;++j){auto id=f.u32();if(id>=models.size())f.fail("CARC model index out of bounds");if(j==0)primary=id;}
  f.skip(8);if(!count)continue;
  const bool tire=std::find(corners.begin(),corners.end(),name)!=corners.end();
  if(tire)g.markers.emplace("placeholder_"+name,frame);
  const bool crash=name.size()>6&&name.compare(name.size()-6,6,"_crash")==0;
  if((mode==View::Crash)!=crash||name.find("_shadow")!=std::string::npos)continue;
  if((mode==View::Body&&tire)||(mode==View::Wheels&&!tire))continue;
  for(const auto&batch:models[primary].batches){
   if(batch.indices.empty())continue;
   auto m=batch;m.name=name;
   if(mode==View::Wheels){
    // Tire spin operates about the CARC MESH pivot. Keep the authored
    // orientation but do not bake its position into the local hull.
    frame[12]=frame[13]=frame[14]=0;
   }
   m.transform=frame;bounds(m);g.triangles+=m.indices.size()/3;
   g.meshes.push_back(std::move(m));
  }
 }
 for(uint32_t i=0,n=f.count(4096);i<n;++i){
  f.tag("OBJC");auto name=lower(f.str());f.str();f.u32();auto transform=f.mat();
  g.markers.emplace(std::move(name),transform);
 }
 if(f.offset!=bytes.size()||(mode!=View::Crash&&g.meshes.empty()))f.fail("unexpected CARC tail or empty mesh selection");
 if(mode==View::Showroom){
  const auto id=16+std::stoi(dir.substr(dir.find_last_of('_')+1));
  const auto cfg=load_alpha_vehicle(r,id);
  for(auto&mesh:g.meshes){
   auto it=std::find_if(cfg.wheels.begin(),cfg.wheels.end(),[&](const auto&w){return w.mesh_name==mesh.name;});
   if(it==cfg.wheels.end()){
    const int hub=wheel_hub_corner(mesh.name);
    if(hub>=0){
     mesh.transform=multiply(translate(cfg.wheels[hub].arch_offset),mesh.transform);
     bounds(mesh);
    }
    continue;
   }
   const auto mount=suspension_mount(*it);
   mesh.transform[12]=mesh.transform[13]=mesh.transform[14]=0;
   for(auto&vertex:mesh.vertices){
    const auto p=point(mesh.transform,vertex.position),n=direction(mesh.transform,vertex.normal);
    const auto s=it->mesh_scale;
    vertex.position={p.x*s.x,p.y*s.y,p.z*s.z};
    vertex.normal=unit({n.x/s.x,n.y/s.y,n.z/s.z});
   }
   mesh.transform=translate(mount);
   bounds(mesh);
  }
 }
 return g;
}

VehicleConfig load_alpha_vehicle(const Resources&r,int id){
 const auto dir=alpha_car_directory(id);
 const auto body=bed_file(r,dir+"/car.ini"),engine=bed_file(r,dir+"/engine.ini");
 const auto tire=bed_file(r,dir+"/tires.ini").at("LocalDynamics");
 const char* names[]={"BMW","CAMARO","BEETLE","BMW (ALPHA 4)"};
 VehicleConfig c;c.id=id;c.name=names[id-17];c.body=dir+"/model.car";c.tire=dir+"/model.wheels";
 c.mass=c.authored_mass=positive(body,"Mass");c.inertia=vector3(body.at("MomentOfInertia"))*c.mass;
 c.center_mass=vector3(body.at("CenterOfMass"));c.drag=vector3(body.at("AeroDrag"));
 c.damping={tuple(body,"Damping",1),tuple(body,"Damping",2)};
 c.aero_location=vector3(body.at("AeroDragLoc"));c.downforce_location=vector3(body.at("CenterOfDownForce"));
 c.downforce=number(body,"DownForceConst");c.front_drive=number(body,"FrontTraction");
 c.brake_torque=positive(body,"BrakeTorque");c.handbrake=positive(body,"HandBrakeTorque");
 c.brake_front=tuple(body,"BrakeBalance",2);
 c.steering_angle=(positive(body,"TireTurnAngleIn")+positive(body,"TireTurnAngleOut"))*.5f;
 c.steering_limit=positive(body,"SteeringLimit");c.steering_reduction=number(body,"SteeringReduction");
 c.idle_rpm=positive(engine,"IdleRpm");c.engine_torque=positive(engine,"TorqueMax");
 c.engine_inertia=positive(engine,"InertiaEngine");c.shaft_inertia=positive(engine,"InertiaDriveShaft");
 c.engine_friction=number(engine,"EngineFriction");c.final_ratio=positive(engine,"EndRatio");
 c.clutch_release=number(engine,"ClutchReleaseTime");c.clutch_engage=number(engine,"ClutchEngageTime");
 c.gear_shift=number(engine,"GearShiftTime");c.reverse_ratio=tuple(engine,"GearRRatio",2);
 c.nitro_g=c.nitro_capacity=0; // Alpha acceleration setting is not an authored nitro system.
 const auto gears=int(positive(engine,"NumGears"));
 if(gears<1||gears>8||c.reverse_ratio>=0)throw std::runtime_error("invalid alpha gearbox");
 for(int i=1;i<=gears;++i)c.ratios.push_back(tuple(engine,"Gear"+std::to_string(i)+"Ratio",2));
 float horsepower=0;
 for(int i=0;i<64;++i){
  char key[16];std::snprintf(key,sizeof key,"Torque%02d",i);if(!engine.contains(key))break;
  float rpm=tuple(engine,key,1),ratio=tuple(engine,key,2);
  if(ratio<0||(!c.torque_curve.empty()&&rpm<=c.torque_curve.back().first))throw std::runtime_error("invalid alpha torque curve");
  c.torque_curve.emplace_back(rpm,ratio);horsepower=std::max(horsepower,rpm*ratio*c.engine_torque/7121.f);
 }
 if(c.torque_curve.size()<2)throw std::runtime_error("missing alpha torque curve");
 c.max_rpm=c.torque_curve.back().first;
 c.mu=1;c.tire_dynamics.authored=true;
 c.tire_dynamics.longitudinal=tuple(tire,"ZFriction",1);
 c.tire_dynamics.lateral=tuple(tire,"XFriction",1);
 c.tire_dynamics.rolling_resistance=number(tire,"RollingResistance");
 const auto shell=load_alpha_car_geometry(r,c.body),tires=load_alpha_car_geometry(r,c.tire);
 Bounds all,roof;
 for(const auto&m:shell.meshes)if(shell.materials[m.material].shader==5||shell.materials[m.material].shader==6)
  for(const auto&v:m.vertices)all.add(point(m.transform,v.position));
 if(!all.valid())throw std::runtime_error("empty alpha chassis");
 const float split=all.lo.y+(all.hi.y-all.lo.y)*.53f;
 for(const auto&m:shell.meshes)if(shell.materials[m.material].shader==5||shell.materials[m.material].shader==6)
  for(const auto&v:m.vertices){auto p=point(m.transform,v.position);if(p.y>split+.06f)roof.add(p);}
 // CARC places the shell at its construction ground plane (Y=0), and
 // includes narrow underbody accessories below it. Extending a full-width
 // box down to the exhaust tip grounds the chassis before the tires load.
 c.lo={all.lo.x,std::max(0.f,all.lo.y),all.lo.z};c.hi={all.hi.x,split,all.hi.z};
 if(roof.valid()){c.top_lo={roof.lo.x,split,roof.lo.z};c.top_hi=roof.hi;c.has_top_collision=true;}
 Json rollbar=Json::array();
 const auto& fit=arch_fit[size_t(id-17)];
 for(size_t i=0;i<corners.size();++i){
  auto&w=c.wheels[i];w.mesh_name=corners[i];w.mount=point(shell.markers.at("placeholder_"+w.mesh_name),{});
  auto axle=std::string(i<2?"Front":"Rear");
  w.width=positive(body,"TireWidth");w.radius=positive(body,"TireRadius");w.mass=positive(body,"TireMass");
  w.inertia_factor=positive(body,axle+"TireMomentOfInertia");
  w.rest=tuple(body,axle+"SuspensionRestLength",2);w.min=number(body,axle+"SuspensionMinLength");w.max=positive(body,axle+"SuspensionMaxLength");
  w.k=tuple(body,axle+"SuspensionConst",2);w.bump=tuple(body,axle+"BumpDamp",2);w.rebound=tuple(body,axle+"ReboundDamp",2);
  if(!(w.min>=0&&w.min<w.rest&&w.rest<w.max&&w.k>0&&w.bump>=0&&w.rebound>=0))
   throw std::runtime_error("invalid alpha suspension");
  const auto& arch=i<2?fit.front:fit.rear;
  w.arch_offset={i%2==0?-arch.left_out:arch.right_out,-fit.ride_drop,arch.forward};
  // Stop compression before the tread can enter the arch. Keep the nominal
  // spring, dampers, rolling dimensions and the available extension.
  w.min=std::max(w.min,w.rest-(fit.ride_drop-arch.compressed_drop));
  if(i%2==0)rollbar.push_back(tuple(body,axle+"RollBarStiffness",2)/w.k);
  Bounds b;float radial=0;
  for(const auto&m:tires.meshes)if(m.name==w.mesh_name)for(const auto&v:m.vertices){
   auto q=point(m.transform,v.position);w.collision_vertices.push_back(q);b.add(q);radial=std::max(radial,std::hypot(q.y,q.z));
  }
  if(w.collision_vertices.size()<8||radial<.01f||b.hi.x-b.lo.x<.01f)throw std::runtime_error("invalid alpha tire: "+w.mesh_name);
  w.mesh_scale={w.width/(b.hi.x-b.lo.x),w.radius/radial,w.radius/radial};
  for(auto&q:w.collision_vertices)q={q.x*w.mesh_scale.x,q.y*w.mesh_scale.y,q.z*w.mesh_scale.z};
 }
 c.provenance={{"alpha_content",true},{"body",dir+"/car.ini"},{"engine",dir+"/engine.ini"},{"tire",dir+"/tires.ini"},
  {"model",c.body},{"horsepower",horsepower},{"mass_source","authored prototype CAR.INI"},
  {"collision_source","CARC body and per-corner tire meshes; body uses two fitted boxes, tire hulls preserve authored nominal radius/width"},
  {"handling",{{"steering_rates",Json::array({tuple(body,"SteeringSpeeds",1),tuple(body,"SteeringSpeeds",2)})},{"rollbar",rollbar}}}};
 return c;
}
}
