#include "track_editor.hpp"
#include "editor_gizmo.hpp"
#include "captured_scene.hpp"
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <cstring>
namespace ofo { namespace {
constexpr size_t placement_cap=80000,route_cap=8192;
void require(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
bool bounded(float x){return std::isfinite(x)&&std::abs(x)<=1000000.f;}
void check_vec(Vec p){require(bounded(p.x)&&bounded(p.y)&&bounded(p.z),"Track position outside finite world bounds");}
Json jvec(Vec p){return Json::array({p.x,p.y,p.z});}
Vec read_vec(const Json&j){require(j.is_array()&&j.size()==3,"Track vector must contain three numbers");Vec p;for(int k=0;k<3;k++){require(j[k].is_number(),"Track vector coordinate must be numeric");p[k]=j[k].get<float>();}check_vec(p);return p;}
size_t index(const Json&j){require(j.is_number_unsigned()||(j.is_number_integer()&&j.get<int64_t>()>=0),"Track index must be nonnegative integer");return j.get<size_t>();}
Mat inverse_affine(const Mat&m){
 float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
 float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);require(std::isfinite(det)&&std::abs(det)>1e-10f,"Track source transform is singular");
 Mat r=identity();r[0]=(e*i-f*h)/det;r[4]=(c*h-b*i)/det;r[8]=(b*f-c*e)/det;r[1]=(f*g-d*i)/det;r[5]=(a*i-c*g)/det;r[9]=(c*d-a*f)/det;r[2]=(d*h-e*g)/det;r[6]=(b*g-a*h)/det;r[10]=(a*e-b*d)/det;Vec t=direction(r,{m[12],m[13],m[14]})*-1; r[12]=t.x;r[13]=t.y;r[14]=t.z;return r;
}
void recalc_bounds(Mesh&m){Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};for(auto&v:m.vertices){auto p=point(m.transform,v.position);for(int k=0;k<3;k++){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}}if(m.vertices.empty()){m.center={m.transform[12],m.transform[13],m.transform[14]};m.radius=0;}else{m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;}}
std::vector<std::vector<size_t>> prop_meshes(const Geometry& g){
 std::vector<std::vector<size_t>> groups(g.props.size());
 for(size_t i=0;i<g.meshes.size();++i){
  const auto& m=g.meshes[i];
  if(m.captured&&m.prop_index>=0&&size_t(m.prop_index)<groups.size())groups[m.prop_index].push_back(i);
 }
 return groups;
}
void check_source_geometry(const std::string& path){
 require(path.size()<=512&&clean_path(path)==path&&
         (path.rfind("tracks/",0)==0||path.rfind("alpha_content/tracks/",0)==0)&&
         path.find("/geometry/track_geom.w32")==path.size()-24,"Invalid foreign geometry path");
}
std::string key(const Geometry& g,size_t pi,const std::vector<size_t>& meshes){
 std::string s;
 auto add=[&](const auto& v){s.append(reinterpret_cast<const char*>(&v),sizeof(v));};
 const auto& p=g.props[pi];
 add(p.mass);add(p.restitution);add(p.inertia);add(p.center);add(p.half);
 add(p.collision_sound);add(p.joint.kind);add(p.joint.pivot);add(p.joint.axis);
 add(p.joint.x_limits);add(p.joint.z_limits);add(p.joint.max_force);add(p.joint.velocity);
 add(p.joint.break_force);add(p.joint.break_torque);add(p.bonus.type);add(p.bonus.cents);add(p.bonus.nitro);
 add(bool(p.log_contact));
 if(p.log_contact){add(p.log_contact->wakeup_velocity);add(p.log_contact->reactivation_velocity);}
 for(auto i:meshes){
  const auto& m=g.meshes[i];
  add(m.material);add(m.vertices.size());add(m.indices.size());
  for(const auto& v:m.vertices){add(v.position);add(v.normal);add(v.color);add(v.u);add(v.v);add(v.lu);add(v.lv);}
  for(auto index:m.indices)add(index);
 }
 return s;
}
bool has_scale(const TrackPlacement& p){return p.scale.x!=1.f||p.scale.y!=1.f||p.scale.z!=1.f;}
Vec scaled_vec(Vec v,Vec scale){return {v.x*scale.x,v.y*scale.y,v.z*scale.z};}
void scale_body(PropDefinition& body,Vec scale){
 const auto prior=body.half;
 body.center=scaled_vec(body.center,scale);
 body.half=scaled_vec(body.half,scale);
 for(int axis=0;axis<3;axis++){
  const int a=(axis+1)%3,b=(axis+2)%3;
  body.inertia[axis]*=(body.half[a]*body.half[a]+body.half[b]*body.half[b])/
                      (prior[a]*prior[a]+prior[b]*prior[b]);
  require(body.half[axis]>0&&body.half[axis]<=1000&&std::isfinite(body.inertia[axis])&&body.inertia[axis]>0,
          "Scaled dynamic object exceeds collision limits");
 }
 body.joint.pivot=scaled_vec(body.joint.pivot,scale);
 body.joint.axis=unit(scaled_vec(body.joint.axis,scale));
}
void bake_scale(Mesh& mesh,const Mat& local){
 const auto inv=inverse_affine(local);
 for(auto& v:mesh.vertices){
  v.position=point(local,v.position);
  const Vec n=v.normal;
  v.normal=unit({inv[0]*n.x+inv[1]*n.y+inv[2]*n.z,
                 inv[4]*n.x+inv[5]*n.y+inv[6]*n.z,
                 inv[8]*n.x+inv[9]*n.y+inv[10]*n.z});
 }
}
void write_file(const fs::path&p,const char*data,size_t n){fs::create_directories(p.parent_path());auto tmp=p;tmp+=".tmp";{std::ofstream f(tmp,std::ios::binary|std::ios::trunc);require(bool(f),"Cannot open track export");f.write(data,std::streamsize(n));f.close();require(bool(f),"Cannot write track export");}std::error_code ec;fs::rename(tmp,p,ec);if(ec){fs::remove(p,ec);ec.clear();fs::rename(tmp,p,ec);}require(!ec,"Cannot replace track export");}
void write_json(const fs::path&p,const Json&j){auto s=j.dump(2);write_file(p,s.data(),s.size());}
}
void validate_track_document(const TrackDocument& d){
 require(d.version>=1&&d.version<=4,"Unsupported track edit version");
 require(!d.stage.empty()&&d.stage.size()<=512,"Track stage is empty or too long");
 require(d.name.size()<=160&&d.course.size()<=128,"Track metadata too long");
 require(d.placements.size()<=placement_cap&&d.route.size()<=route_cap,"Track edit count limit exceeded");
 require(d.route.empty()?d.start_index==0:d.start_index<d.route.size(),"Track start index out of range");
 std::set<size_t> native_indices;
 std::set<uint64_t> native_ids;
 size_t clones=0;
 for(const auto& p:d.placements){
  if(!p.source_geometry.empty()){
   require(d.version>=4&&p.source_id&& !p.replace_native,"Invalid foreign template reference");
   check_source_geometry(p.source_geometry);
  }
  if(p.replace_native){
   require(native_indices.insert(p.source_prop).second,"Duplicate native override");
   if(p.source_id)require(native_ids.insert(p.source_id).second,"Duplicate native override identity");
  }else require(++clones<=4096,"Track clone count exceeded");
  require(p.source_prop<=1000000,"Track template index out of range");
  check_vec(p.position);
  require(bounded(p.yaw),"Track yaw must be finite and bounded");
  for(int k=0;k<3;k++)
   require(std::isfinite(p.scale[k])&&p.scale[k]>=track_min_scale&&p.scale[k]<=track_max_scale,
           "Track scale must be between 0.05 and 20");
  require(d.version>=4||!has_scale(p),"Scale requires track document version 4");
  for(float x:p.rotation)require(std::isfinite(x),"Nonfinite object rotation");
  require(std::abs(p.rotation[3])+std::abs(p.rotation[7])+std::abs(p.rotation[11])+
          std::abs(p.rotation[12])+std::abs(p.rotation[13])+std::abs(p.rotation[14])+
          std::abs(p.rotation[15]-1)<.001f,"Rotation must be rigid");
  Vec a=direction(p.rotation,{1,0,0}),b=direction(p.rotation,{0,1,0}),c=direction(p.rotation,{0,0,1});
  require(std::abs(dot(a,a)-1)<.002f&&std::abs(dot(b,b)-1)<.002f&&std::abs(dot(c,c)-1)<.002f&&
          std::abs(dot(a,b))<.002f&&std::abs(dot(a,c))<.002f&&std::abs(dot(b,c))<.002f&&
          dot(cross(a,b),c)>.998f,"Invalid rigid object rotation");
 }
 for(auto p:d.route)check_vec(p);
}
Json track_document_json(const TrackDocument& d){
 validate_track_document(d);
 Json j={{"version",d.version},{"stage",d.stage},{"placements",Json::array()},{"route",Json::array()},
         {"closed",d.closed},{"start_index",d.start_index}};
 if(d.version>=4){
  if(!d.name.empty())j["name"]=d.name;
  if(!d.course.empty())j["course"]=d.course;
 }
 for(const auto& p:d.placements){
  Json item={{"source_prop",p.source_geometry.empty()?p.source_prop:0},{"source_id",p.source_id},
             {"rotation",p.rotation},{"replace_native",p.replace_native},{"position",jvec(p.position)},{"yaw",p.yaw}};
  if(!p.source_geometry.empty())item["source_geometry"]=p.source_geometry;
  if(d.version>=4&&has_scale(p))item["scale"]=jvec(p.scale);
  j["placements"].push_back(std::move(item));
 }
 for(auto p:d.route)j["route"].push_back(jvec(p));
 return j;
}
TrackDocument parse_track_document(const Json& j){
 require(j.is_object(),"Track document must be object");
 TrackDocument d;
 require(j.at("version").is_number_integer(),"Track version must be integer");
 d.version=j.at("version").get<int>();
 d.stage=j.at("stage").get<std::string>();
 if(d.version>=4){
  d.name=j.value("name",std::string{});
  d.course=j.value("course",std::string{});
 }
 const auto& ps=j.at("placements");
 const auto& rs=j.at("route");
 require(ps.is_array()&&rs.is_array(),"Track placements and route must be arrays");
 require(ps.size()<=placement_cap&&rs.size()<=route_cap,"Track edit count limit exceeded");
 for(const auto& p:ps){
  TrackPlacement a;
  a.source_prop=index(p.at("source_prop"));
  if(p.contains("source_id"))a.source_id=index(p.at("source_id"));
  a.position=read_vec(p.at("position"));
  require(p.at("yaw").is_number(),"Track yaw must be numeric");
  a.yaw=p.at("yaw").get<float>();
  if(d.version>=3){
   if(p.contains("rotation"))a.rotation=p.at("rotation").get<Mat>();
   a.replace_native=p.value("replace_native",false);
  }
  if(d.version>=4){
   a.source_geometry=p.value("source_geometry",std::string{});
   if(p.contains("scale"))a.scale=read_vec(p.at("scale"));
  }
  d.placements.push_back(std::move(a));
 }
 for(const auto& p:rs)d.route.push_back(read_vec(p));
 d.closed=j.at("closed").get<bool>();
 d.start_index=index(j.at("start_index"));
 validate_track_document(d);
 return d;
}
void bind_track_document(TrackDocument& d,Geometry& g,const Resources& r){
 validate_track_document(d);
 std::map<uint64_t,size_t> ids;
 for(size_t i=0;i<g.props.size();i++)
  if(!track_template_only(g,i))require(ids.emplace(g.props[i].id,i).second,"Duplicate native prop identity");
 std::vector<uint64_t> legacy;
 if(d.version==1&&r.exists("capture_assets/manifest.json")){
  auto m=Json::parse(r.text("capture_assets/manifest.json"));
  for(const auto& o:m.at("objects"))
   if(o.value("export_status","")=="linked"&&clean_path(o.at("source_track"))==g.source)legacy.push_back(o.at("id"));
 }
 TrackAssetLibrary library(r);
 const size_t original_props=g.props.size(),original_meshes=g.meshes.size(),original_materials=g.materials.size();
 auto next=d;
 try{
  for(auto& p:next.placements){
   if(next.version==1){
    require(p.source_prop<legacy.size(),"Legacy editor template unavailable on this track");
    p.source_id=legacy[p.source_prop];
   }
   if(!p.source_geometry.empty()){
    p.source_prop=library.materialize(g,{0,p.source_id,{},p.source_geometry});
   }else if(p.source_id){
    auto it=ids.find(p.source_id);
    require(it!=ids.end(),"Saved native template identity unavailable");
    p.source_prop=it->second;
   }else{
    require(p.source_prop<g.props.size(),"Saved editor template index unavailable");
    const auto origin=g.editor_template_props.find(p.source_prop);
    if(origin==g.editor_template_props.end())p.source_id=g.props[p.source_prop].id;
    else{
     p.source_id=origin->second.source_id;
     p.source_geometry=origin->second.source_geometry;
    }
   }
  }
  // Preserve legacy v3 serialization unless v4 data is actually required.
  if(next.version<3)next.version=3;
  validate_track_document(next);
 }catch(...){
  g.props.resize(original_props);
  g.meshes.resize(original_meshes);
  g.materials.resize(original_materials);
  g.editor_template_props.erase(g.editor_template_props.lower_bound(original_props),g.editor_template_props.end());
  throw;
 }
 d=std::move(next);
}
std::vector<TrackTemplate> track_templates(const Geometry& g){
 const auto groups=prop_meshes(g);
 std::set<std::string> seen;
 std::vector<TrackTemplate> out;
 for(size_t p=0;p<g.props.size();p++){
  if(groups[p].empty()||track_template_only(g,p))continue;
  if(seen.insert(key(g,p,groups[p])).second)
   out.push_back({p,g.props[p].id,g.meshes[groups[p].front()].name,{}});
 }
 return out;
}
const Geometry& TrackAssetLibrary::load_source(const std::string& path){
 check_source_geometry(path);
 if(cached_source_&&cached_source_->source==path)return *cached_source_;
 cached_source_.reset();
 auto source=load_track_prop_geometry(resources_,path);
 append_captured_scene(source,resources_);
 source.meshes.erase(std::remove_if(source.meshes.begin(),source.meshes.end(),[](const Mesh& mesh){
  return mesh.prop_index<0||mesh.billboard||!mesh.captured;
 }),source.meshes.end());
 cached_source_=std::move(source);
 return *cached_source_;
}
std::vector<TrackTemplate> TrackAssetLibrary::templates(const Geometry& local){
 if(!catalog_ready_){
  std::vector<TrackTemplate> catalog;
  for(const auto& path:track_catalog(resources_)){
   // Track catalog also sees files from user installations. An unavailable or
   // invalid donor should not prevent editing an otherwise valid scene.
   try{
    auto entries=track_templates(load_source(path));
    const auto location=fs::path(path).parent_path().parent_path().generic_u8string();
    for(auto& entry:entries){
     entry.source_geometry=path;
     entry.name+=" ["+location+"]";
     catalog.push_back(std::move(entry));
    }
   }catch(const std::runtime_error&){}
  }
  catalog_=std::move(catalog);
  catalog_ready_=true;
 }
 auto entries=track_templates(local);
 std::set<uint64_t> native;
 for(size_t i=0;i<local.props.size();i++)if(!track_template_only(local,i))native.insert(local.props[i].id);
 for(const auto& entry:catalog_){
  if(entry.source_geometry!=local.source||!native.count(entry.native_id))entries.push_back(entry);
 }
 return entries;
}
size_t TrackAssetLibrary::materialize(Geometry& local,const TrackTemplate& entry){
 if(entry.source_geometry.empty()){
  require(entry.source_prop<local.props.size()&&!track_template_only(local,entry.source_prop),"Unknown local track template");
  return entry.source_prop;
 }
 for(const auto& pair:local.editor_template_props)
  if(pair.second.source_geometry==entry.source_geometry&&pair.second.source_id==entry.native_id)return pair.first;
 const auto& source=load_source(entry.source_geometry);
 auto prop=std::find_if(source.props.begin(),source.props.end(),[&](const PropDefinition& p){return p.id==entry.native_id;});
 require(prop!=source.props.end(),"Saved foreign template identity unavailable");
 const auto index=size_t(prop-source.props.begin());
 const auto target=local.props.size();
 const auto groups=prop_meshes(source);
 require(!groups[index].empty(),"Foreign template has no dynamic mesh");
 std::map<size_t,size_t> material_indices;
 std::vector<Material> materials;
 std::vector<Mesh> meshes;
 for(size_t m:groups[index]){
  auto mesh=source.meshes[m];
  auto [it,added]=material_indices.emplace(mesh.material,local.materials.size()+materials.size());
  if(added){
   auto material=source.materials.at(mesh.material);
   for(auto& texture:material.texture)texture=resources_.texture(source.source,texture);
   auto old=std::find_if(local.materials.begin(),local.materials.end(),[&](const Material& value){
    return value.shader==material.shader&&value.alpha==material.alpha&&value.name==material.name&&
     std::equal(std::begin(value.texture),std::end(value.texture),std::begin(material.texture));
   });
   if(old==local.materials.end())materials.push_back(std::move(material));
   else it->second=size_t(old-local.materials.begin());
  }
  mesh.material=it->second;
  mesh.prop_index=int(target);
  meshes.push_back(std::move(mesh));
 }
 local.materials.reserve(local.materials.size()+materials.size());
 local.meshes.reserve(local.meshes.size()+meshes.size());
 local.props.reserve(target+1);
 local.editor_template_props.emplace(target,TrackAssetId{entry.source_geometry,entry.native_id});
 auto body=*prop;
 // Preview-only index, never emitted as a native object.
 body.id=(uint64_t(3)<<62)|target;
 local.props.push_back(std::move(body));
 for(auto& material:materials)local.materials.push_back(std::move(material));
 for(auto& mesh:meshes)local.meshes.push_back(std::move(mesh));
 return target;
}
size_t apply_track_document(Geometry& g,const TrackDocument& d){
 validate_track_document(d);
 require(g.source==stage_geometry(d.stage),"Track edit stage does not match geometry");
 const auto native_count=g.editor_template_props.empty()?g.props.size():g.editor_template_props.begin()->first;
 const auto groups=prop_meshes(g);
 size_t vertex_budget=0;
 std::vector<PropDefinition> props;
 std::vector<Mesh> meshes;
 std::vector<std::pair<size_t,PropDefinition>> override_props;
 std::vector<std::pair<size_t,Mesh>> override_meshes;
 std::set<uint64_t> ids;
 for(size_t i=0;i<native_count;i++)ids.insert(g.props[i].id);
 uint64_t next=uint64_t(1)<<63;
 for(size_t n=0;n<d.placements.size();n++){
  const auto& p=d.placements[n];
  require(p.source_prop<g.props.size()&&!groups[p.source_prop].empty(),"Track source prop missing");
  const auto foreign=g.editor_template_props.find(p.source_prop);
  if(foreign!=g.editor_template_props.end()){
   require(!p.replace_native&&p.source_geometry==foreign->second.source_geometry&&p.source_id==foreign->second.source_id,
           "Foreign track template was not bound");
  }else require(p.source_geometry.empty(),"Foreign track template was not bound");
  const auto& source=g.props[p.source_prop];
  auto body=source;
  auto unscaled=p;
  unscaled.scale={1,1,1};
  body.transform=placement_matrix(source,unscaled);
  auto inverse_source=inverse_affine(source.transform);
  const auto delta=multiply(body.transform,inverse_source);
  const bool scaled=has_scale(p);
  if(scaled)scale_body(body,p.scale);
  const auto target=p.replace_native?p.source_prop:native_count+props.size();
  if(p.replace_native)override_props.emplace_back(target,body);
  else{
   while(ids.count(next))next++;
   body.id=next++;
   ids.insert(body.id);
   props.push_back(body);
  }
  for(size_t i:groups[p.source_prop]){
   const auto& original=g.meshes[i];
   if(!p.replace_native){
    require(original.vertices.size()<=6000000-vertex_budget,"Track clone vertex budget exceeded");
    vertex_budget+=original.vertices.size();
   }
   auto mesh=original;
   mesh.prop_index=int(target);
   if(scaled){
    auto scale=identity();
    scale[0]=p.scale.x;scale[5]=p.scale.y;scale[10]=p.scale.z;
    bake_scale(mesh,multiply(scale,multiply(inverse_source,mesh.transform)));
    mesh.transform=body.transform;
   }else mesh.transform=multiply(delta,mesh.transform);
   recalc_bounds(mesh);
   if(p.replace_native)override_meshes.emplace_back(i,std::move(mesh));
   else{
    mesh.name="edited_"+std::to_string(n)+"_"+mesh.name;
    meshes.push_back(std::move(mesh));
   }
  }
 }
 g.props.reserve(native_count+props.size());
 g.meshes.reserve(g.meshes.size()+meshes.size());
 for(auto& p:override_props)g.props[p.first]=std::move(p.second);
 for(auto& m:override_meshes)g.meshes[m.first]=std::move(m.second);
 g.meshes.erase(std::remove_if(g.meshes.begin(),g.meshes.end(),[&](const Mesh& m){
  return m.prop_index>=0&&size_t(m.prop_index)>=native_count;
 }),g.meshes.end());
 g.props.resize(native_count);
 g.editor_template_props.clear();
 for(auto& p:props)g.props.push_back(std::move(p));
 for(auto& m:meshes){
  g.triangles+=m.indices.size()/3;
  g.meshes.push_back(std::move(m));
 }
 return props.size();
}
fs::path track_edit_path(const fs::path&root,const std::string&stage){require(!stage.empty()&&stage.size()<=512,"Track stage is empty or too long");std::string name;for(unsigned char c:stage)name+=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')?char(c):'_';return root/"track_edits"/(name+".json");}
std::optional<TrackDocument> load_track_document(const fs::path&root,const std::string&stage){auto p=track_edit_path(root,stage);if(!fs::exists(p))return std::nullopt;require(fs::file_size(p)<=8*1024*1024,"Track document exceeds file limit");std::ifstream f(p);require(bool(f),"Cannot read track document");Json j;f>>j;auto d=parse_track_document(j);require(d.stage==stage,"Track sidecar stage mismatch");return d;}
void save_track_document(const fs::path&root,const TrackDocument&d){write_json(track_edit_path(root,d.stage),track_document_json(d));}
Json track_route_json(const TrackDocument&d){validate_track_document(d);Json j={{"points",Json::array()},{"gates",Json::array()},{"available",false},{"closed",d.closed},{"source","track editor"}};if(d.route.size()<2)return j;for(size_t n=0;n<d.route.size();n++){size_t i=(d.start_index+n)%d.route.size();Vec p=d.route[i];j["points"].push_back(jvec(p));Vec before=d.route[(i+d.route.size()-1)%d.route.size()],after=d.route[(i+1)%d.route.size()];Vec tangent=unit(after-before,{0,0,1});Vec side=unit(cross({0,1,0},tangent),{1,0,0})*8;j["gates"].push_back({{"position",jvec(p)},{"left",jvec(p-side)},{"right",jvec(p+side)}});}j["available"]=d.closed&&d.route.size()>=3;return j;}
TrackMinimap make_track_minimap(const TrackDocument&d,int size){validate_track_document(d);require(size==256||size==512,"Track minimap size must be 256 or 512");TrackMinimap out;out.size=size;float minx=1e30f,minz=1e30f,maxx=-1e30f,maxz=-1e30f;auto include=[&](Vec p){minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);minz=std::min(minz,p.z);maxz=std::max(maxz,p.z);};for(auto&p:d.route)include(p);for(auto&p:d.placements)include(p.position);if(minx>maxx){minx=minz=-50;maxx=maxz=50;}float extent=std::max(10.f,std::max(maxx-minx,maxz-minz))*1.2f,cx=(minx+maxx)*.5f,cz=(minz+maxz)*.5f;out.world_top_left={cx-extent*.5f,0,cz-extent*.5f};out.world_bottom_right={cx+extent*.5f,0,cz+extent*.5f};
 size_t stride=(size*3+3)&~size_t(3),bytes=54+stride*size;out.bmp.assign(bytes,0);auto put=[&](size_t at,uint32_t v,int n){for(int k=0;k<n;k++)out.bmp[at+k]=uint8_t(v>>(k*8));};out.bmp[0]='B';out.bmp[1]='M';put(2,uint32_t(bytes),4);put(10,54,4);put(14,40,4);put(18,size,4);put(22,size,4);put(26,1,2);put(28,24,2);put(34,uint32_t(stride*size),4);
 auto pixel=[&](int x,int y,uint32_t color){if(x<0||x>=size||y<0||y>=size)return;size_t at=54+(size-1-y)*stride+x*3;out.bmp[at]=uint8_t(color);out.bmp[at+1]=uint8_t(color>>8);out.bmp[at+2]=uint8_t(color>>16);};for(int y=0;y<size;y++)for(int x=0;x<size;x++)pixel(x,y,0x182526);auto xy=[&](Vec p){return std::pair<int,int>{int(std::lround((p.x-out.world_top_left.x)/extent*(size-1))),int(std::lround((p.z-out.world_top_left.z)/extent*(size-1)))};};auto disk=[&](int x,int y,int radius,uint32_t color){for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++)if(dx*dx+dy*dy<=radius*radius)pixel(x+dx,y+dy,color);};auto line=[&](Vec p,Vec q){auto a=xy(p),b=xy(q);int steps=std::max(std::abs(a.first-b.first),std::abs(a.second-b.second));for(int s=0;s<=steps;s++){float t=steps?float(s)/steps:0;disk(int(std::lround(a.first+(b.first-a.first)*t)),int(std::lround(a.second+(b.second-a.second)*t)),size/128,0xe6e7df);}};for(size_t i=1;i<d.route.size();i++)line(d.route[i-1],d.route[i]);if(d.closed&&d.route.size()>2)line(d.route.back(),d.route.front());for(auto&p:d.placements){auto a=xy(p.position);disk(a.first,a.second,3,0xf5a742);}if(!d.route.empty()){auto a=xy(d.route[d.start_index]);disk(a.first,a.second,size/64,0x42e879);}out.tga.assign(18+size_t(size)*size*4,0);out.tga[2]=2;out.tga[12]=uint8_t(size);out.tga[13]=uint8_t(size>>8);out.tga[14]=uint8_t(size);out.tga[15]=uint8_t(size>>8);out.tga[16]=32;out.tga[17]=8;for(int y=0;y<size;y++)for(int x=0;x<size;x++){size_t src=54+y*stride+x*3,dst=18+(size_t(y)*size+x)*4;for(int k=0;k<3;k++)out.tga[dst+k]=out.bmp[src+k];out.tga[dst+3]=255;}out.metadata={{"MapSize",{{"1",size},{"2",size}}},{"WorldTopLeft",{{"1",out.world_top_left.x},{"2",out.world_top_left.z}}},{"WorldBottomRight",{{"1",out.world_bottom_right.x},{"2",out.world_bottom_right.z}}},{"ScreenPos",{{"1",16},{"2",16}}},{"ScreenSize",{{"1",128},{"2",128}}},{"format","BMP24"},{"axis","image x = world x; image y = world z"}};return out;}
void save_track_minimap(const fs::path&root,const TrackDocument&d,int size){auto m=make_track_minimap(d,size);auto base=track_edit_path(root,d.stage);base.replace_extension();auto bmp=fs::path(base.string()+"_minimap.bmp");write_file(bmp,reinterpret_cast<const char*>(m.bmp.data()),m.bmp.size());auto tga=fs::path(base.string()+"_minimap.tga");write_file(tga,reinterpret_cast<const char*>(m.tga.data()),m.tga.size());m.metadata["image"]=bmp.filename().string();m.metadata["MapTexture"]=tga.filename().string();write_json(fs::path(base.string()+"_minimap.json"),m.metadata);}
}
