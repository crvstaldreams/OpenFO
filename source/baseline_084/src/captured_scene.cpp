#include "captured_scene.hpp"
#include <set>
#include <regex>
#include <unordered_map>

namespace ofo {
namespace {
Vec captured_vector(const Json& a) {
 if(a.size()!=3)throw std::runtime_error("Captured physics vector size");
 Vec v{a.at(0),a.at(1),a.at(2)};
 for(int k=0;k<3;k++)if(!std::isfinite(v[k]))throw std::runtime_error("Captured physics vector nonfinite");
 return v;
}
struct PhysicsType {float mass=0,restitution=.1f;int sound=0;std::string inertia="INERTIA_SOLID_BOX";PropJointDefinition joint;PropBonus bonus;std::optional<LogContact> log_contact;};
std::map<std::string,PhysicsType> native_types(const Resources& r) {
 std::map<std::string,PhysicsType> out;
 if(!r.exists("global/dynamics/dynamic_objects.bed"))return out;
 auto text=r.text("global/dynamics/dynamic_objects.bed");
 // Read native data tables without executing script functions.
 text=std::regex_replace(text,std::regex("(--|#)[^\r\n]*"),"");
 // Balance complete native type tables, including nested Hinge/Joint records
 // and mixed-case stunt names. Flat-table regexes silently omit these bodies.
 std::map<std::string,int>sound_ids;std::regex sound_constant(R"((COLLISIONSOUND_[A-Z_]+)\s*=\s*([0-9]+))");for(std::sregex_iterator it(text.begin(),text.end(),sound_constant),end;it!=end;++it)sound_ids[(*it)[1]]=std::stoi((*it)[2]);
 std::map<std::string,PropBonus> bonuses;
 std::regex bonus_constant(R"((BONUSTYPE_[A-Z_]+)\s*=\s*([0-9]+))");
 for(std::sregex_iterator it(text.begin(),text.end(),bonus_constant),end;it!=end;++it)
  bonuses[(*it)[1]].type=std::stoi((*it)[2]);
 // The symbolic BED keys are retained by the data parser. Convert the money
 // table to exact cents before summing, then truncate only at the payout.
 auto load_bonus_table=[&](const char* name,bool price){
  std::smatch match;
  std::regex pattern(std::string(R"((?:^|
)[ 	]*)")+name+R"([ 	]*=[ 	]*\{[^}]*\})");
  if(!std::regex_search(text,match,pattern))return;
  auto table=parse_bed(match.str()).at(name);
  for(auto it=table.begin();it!=table.end();++it){
   auto bonus=bonuses.find(it.key());
   if(bonus==bonuses.end()||!it.value().is_number())continue;
   double value=it.value().get<double>();
   if(price&&value>=0&&value<=100000)bonus->second.cents=int(std::lround(value*100));
   if(!price&&value>=0&&value<=1)bonus->second.nitro=float(value);
  }
 };
 load_bonus_table("BonusTypePrice",true);
 load_bonus_table("BonusTypeMayhem",false);
 std::regex table(R"((?:^|
)[ 	]*([A-Za-z_][A-Za-z0-9_]*)[ 	]*=[ 	]*\{)");
 for(std::sregex_iterator it(text.begin(),text.end(),table),end;it!=end;++it) {
  size_t begin=size_t(it->position()),open=begin+size_t(it->length())-1,stop=open+1;int depth=1;char quote=0;
  for(;stop<text.size()&&depth;stop++){char c=text[stop];if(quote){if(c==quote&&(stop==0||text[stop-1]!='\\'))quote=0;}else if(c=='"'||c=='\'')quote=c;else if(c=='{')++depth;else if(c=='}')--depth;}
  if(depth)throw std::runtime_error("Unbalanced native dynamic type table");
  auto type=it->operator[](1).str();auto j=parse_bed(text.substr(begin,stop-begin));const auto& a=j.at(type);
  if(!a.contains("Mass")||!a["Mass"].is_number()||!a.contains("Restitution")||!a["Restitution"].is_number())continue;
  PhysicsType p;p.mass=a["Mass"];p.restitution=a["Restitution"];
  const char* joint_name=a.contains("Hinge")?"Hinge":a.contains("Joint")?"Joint":nullptr;
  if(joint_name){const auto&j=a.at(joint_name);p.joint.kind=std::string(joint_name)=="Hinge"?PropJointDefinition::Hinge:PropJointDefinition::Pole;auto limits=[&](const char*key){std::array<float,2>v{};if(j.contains(key)){v[0]=j.at(key).at("1").get<float>()*.0174532925f;v[1]=j.at(key).at("2").get<float>()*.0174532925f;}return v;};p.joint.x_limits=limits(p.joint.kind==PropJointDefinition::Hinge?"AngleLimits":"XAxisLimits");p.joint.z_limits=limits("ZAxisLimits");p.joint.max_force=j.value("MaxForce",0.f);p.joint.velocity=j.value("Velocity",0.f);p.joint.break_force=j.value("BreakForce",0.f);p.joint.break_torque=j.value("BreakTorque",0.f);}

  if(a.contains("Inertia")&&a["Inertia"].is_object())p.inertia=a["Inertia"].value("$reference",p.inertia);
  const auto key=lower(type);
  if((key=="wood_log_small"||key=="wood_log_medium")&&p.inertia=="INERTIA_SOLID_CYLINDER"){
   p.log_contact=LogContact{a.value("WakeupVelocity",.25f),a.value("ReactivationVelocity",.25f)};
  }
  if(a.contains("BonusType")){
   const auto&b=a.at("BonusType");
   if(b.is_object()){auto found=bonuses.find(b.value("$reference",std::string()));if(found!=bonuses.end())p.bonus=found->second;}
   else if(b.is_number()){for(const auto&entry:bonuses)if(entry.second.type==b.get<double>()){p.bonus=entry.second;break;}}
  }
  if(a.contains("CollisionSound")){const auto&sound=a["CollisionSound"];if(sound.is_number_integer())p.sound=sound.get<int>();else if(sound.is_object())p.sound=sound_ids[sound.value("$reference",std::string())];}out[lower(type)]=p;
 }
 return out;
}
Vec model_inertia(float mass,Vec h,const std::string& kind) {
 if(kind=="INERTIA_SOLID_CYLINDER") {
  float radius=(h.x+h.z)*.5f,axis=mass*radius*radius*.5f,side=mass*(3*radius*radius+4*h.y*h.y)/12;
  return {side,axis,side};
 }
 // Native box inertia also serves as a declared approximation for unsupported
 // hollow/spherical types outside the exact captured instances.
 return {mass*(h.y*h.y+h.z*h.z)/3,mass*(h.x*h.x+h.z*h.z)/3,mass*(h.x*h.x+h.y*h.y)/3};
}
bool same_transform(const Mat& a,const Mat& b) {
 for(int k=0;k<16;k++)if(std::abs(a[k]-b[k])>=.01f)return false;
 return true;
}
}
Json append_captured_scene(Geometry& g,const Resources& r) {
 Json info={{"instances",0},{"unique_meshes",0},{"vertices",0},{"removed_static_meshes",0},
  {"physics","native track placements and type mass/restitution; model-bound inertia; exact capture inertia where available"},
  {"replacement_policy","attach bodies to matching native compact meshes; preserve static geometry and trees"}};
 Json manifest={{"complete",true},{"objects",Json::array()}};
 if(r.exists("capture_assets/manifest.json"))manifest=Json::parse(r.text("capture_assets/manifest.json"));
 if(!manifest.value("complete",false))throw std::runtime_error("Incomplete captured scene");
 // The capture is a library of physics types, not a placement list for other tracks.
 // Surface/model indices are local to each W32 and must never be reused elsewhere.
 auto definitions=native_types(r);
 std::map<std::string,const Json*> types;
 std::map<std::string,std::vector<const Json*>> exact;
 for(const auto& o:manifest.at("objects")) {
  if(o.value("export_status","")!="linked")continue;
  auto type=lower(o.value("type_name",std::string()));
  if(!type.empty())types.emplace(type,&o);
  if(clean_path(o.at("source_track"))==g.source)exact[lower(o.value("instance_name",std::string()))].push_back(&o);
 }
 std::unordered_map<std::string,std::vector<size_t>> meshes_by_name;
 for(size_t i=0;i<g.meshes.size();++i){
  const auto& mesh=g.meshes[i];
  if(mesh.prop_index<0&&!mesh.billboard&&mesh.source_surface>=0)meshes_by_name[mesh.name].push_back(i);
 }
 std::set<std::string> models,unmatched_types;
 size_t instances=0,vertices=0,exact_count=0,type_count=0,unmatched=0,approximate_inertia=0;
 for(const auto& p:g.native_props) {
  const Json* capture=nullptr;
  bool exact_match=false;
  auto found=exact.find(lower(p.name));
  if(found!=exact.end())for(auto o:found->second) {
   Mat transform;const auto& a=o->at("world_matrix");
   if(a.size()!=16)throw std::runtime_error("Captured matrix size");
   for(int k=0;k<16;k++)transform[k]=a[k];
   auto surfaces=o->at("surface_indices").get<std::vector<uint32_t>>();
   if(surfaces==p.surfaces&&same_transform(transform,p.transform)){capture=o;exact_match=true;break;}
  }
  // Flag 4 is present on the captured dynamic BMODs. Require both the native
  // dynamic flag and a known native type before enabling an uncaptured body.
  if(!capture&&(p.model_flags&4u)) {
   auto type=types.find(lower(p.type));
   if(type!=types.end())capture=type->second;
  }
  auto definition=definitions.find(lower(p.type));
  bool native_definition=(p.model_flags&4u)&&definition!=definitions.end();
  if(!capture&&!native_definition){if(p.model_flags&4u){++unmatched;unmatched_types.insert(p.type);}continue;}
  std::vector<size_t> meshes;
  const auto candidates=meshes_by_name.find(p.name);
  if(candidates==meshes_by_name.end())continue;
  for(size_t i:candidates->second) {
   const auto& m=g.meshes[i];
   if(m.prop_index>=0||!same_transform(m.transform,p.transform))continue;
   if(std::find(p.surfaces.begin(),p.surfaces.end(),uint32_t(m.source_surface))!=p.surfaces.end())meshes.push_back(i);
  }
  if(meshes.empty())continue;
  PropDefinition body;
  // A track-local index gives distinct identities even when instance names repeat.
  body.id=(uint64_t(1)<<62)|(uint64_t(p.native_index)+1);
  if(native_definition){body.mass=definition->second.mass;body.restitution=definition->second.restitution;body.collision_sound=definition->second.sound;body.joint=definition->second.joint;body.log_contact=definition->second.log_contact;}
  else {body.mass=capture->at("mass_native");body.restitution=capture->at("restitution_native");}
  if(definition!=definitions.end())body.bonus=definition->second.bonus;
  body.transform=p.transform;body.center=p.center;body.half=p.half;
  if(exact_match) {
   body.id=capture->at("id");body.center=captured_vector(capture->at("collision_center_local"));
   body.half=captured_vector(capture->at("collision_half_extents"));
   body.inertia=captured_vector(capture->at("inertia_diagonal_native"));++exact_count;
  } else {
   auto kind=native_definition?definition->second.inertia:"INERTIA_SOLID_BOX";
   body.inertia=model_inertia(body.mass,body.half,kind);++type_count;
   if(kind!="INERTIA_SOLID_BOX"&&kind!="INERTIA_SOLID_CYLINDER")++approximate_inertia;
  }
  if(!(body.mass>0)||body.half.x<=0||body.half.y<=0||body.half.z<=0)throw std::runtime_error("Invalid native prop bounds or mass");
  int prop_index=int(g.props.size());g.props.push_back(body);
  for(auto i:meshes){auto& m=g.meshes[i];m.prop_index=prop_index;m.captured=true;vertices+=m.vertices.size();}
  models.insert(p.model_name);++instances;
 }
 info["instances"]=instances;info["unique_meshes"]=models.size();info["vertices"]=vertices;
 info["exact_captured_instances"]=exact_count;info["native_type_instances"]=type_count;
 info["unmatched_dynamic_instances"]=unmatched;info["unmatched_dynamic_types"]=unmatched_types;
 info["stage_source"]=g.stage_source;info["filtered_compact_instances"]=g.filtered_compact_instances;
 info["native_bonus_instances"]=std::count_if(g.props.begin(),g.props.end(),[](const auto&b){return b.bonus.type>0&&(b.bonus.cents>0||b.bonus.nitro>0);});
 info["placement_source"]=g.source;info["approximate_inertia_instances"]=approximate_inertia;info["native_type_definitions"]=definitions.size();
 return info;
}
}
