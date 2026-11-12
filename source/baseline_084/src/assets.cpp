#include "assets.hpp"
#include "alpha_car.hpp"
#include "alpha_track.hpp"
#include "file_io.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cctype>
#include <regex>
#include <limits>
#include <set>
namespace ofo {
std::string lower(std::string s){for(char&c:s)c=char(std::tolower((unsigned char)c));return s;}
std::string clean_path(std::string s){for(char&c:s)if(c=='\\')c='/';s=lower(s);if(s.rfind("data/",0)==0)s.erase(0,5);fs::path p=fs::u8path(s);if(p.is_absolute()||s.find(':')!=s.npos)throw std::runtime_error("Absolute resource path");for(const auto&a:p)if(a=="..")throw std::runtime_error("Resource path traversal");auto key=p.lexically_normal().generic_u8string();while(key.size()>1&&key.back()=='/')key.pop_back();return key;}
Resources::Resources(fs::path root):root_(fs::exists(root/"data")?root/"data":root){if(!fs::is_directory(root_))throw std::runtime_error("Missing data directory: "+root_.u8string());for(auto&i:fs::recursive_directory_iterator(root_,fs::directory_options::skip_permission_denied)){if(i.is_symlink()||!i.is_regular_file())continue;auto rel=i.path().lexically_relative(root_).generic_u8string();if(rel.find("/.")!=rel.npos||rel[0]=='.')continue;auto key=clean_path(rel);if(index_.count(key))throw std::runtime_error("Ambiguous case-insensitive resource: "+key);index_[key]=i.path();}}
void Resources::mount(fs::path root,std::string prefix){if(!fs::is_directory(root))return;for(auto&i:fs::recursive_directory_iterator(root)){if(i.is_symlink()||!i.is_regular_file())continue;auto key=clean_path(prefix+"/"+i.path().lexically_relative(root).generic_u8string());if(index_.count(key))throw std::runtime_error("Companion resource collision: "+key);index_[key]=i.path();}}
// Optional packaged HD replacements keep native canonical resource names.
// Scene/script paths remain unchanged. The native balks atlas definition is an explicit UI exception.
size_t Resources::overlay_textures(fs::path root){
 if(!fs::is_directory(root))return 0;
 std::map<std::string,fs::path> replacements;
 for(const auto&item:fs::recursive_directory_iterator(root,fs::directory_options::skip_permission_denied)){
  if(item.is_symlink()||!item.is_regular_file())continue;
  auto key=clean_path(item.path().lexically_relative(root).generic_u8string());
  const bool night_sky=key.rfind("tracks/shared/skybox/openfo_night/",0)==0;
  const bool native_balk=key=="menu/balks.bed"||key=="menu/balks.dds";
  auto ext=lower(item.path().extension().string());if(ext!=".dds"&&ext!=".tga"&&!native_balk)continue;
  if(!index_.count(key)&&!native_balk&&!night_sky)continue;
  if(!replacements.emplace(key,item.path()).second)throw std::runtime_error("Ambiguous HD texture: "+key);
 }
 for(const auto&entry:replacements)index_[entry.first]=entry.second;
 return replacements.size();
}
bool Resources::exists(std::string s)const{return index_.count(clean_path(s));}
Bytes Resources::read(std::string s)const{auto key=clean_path(s);auto it=index_.find(key);if(it==index_.end())throw std::runtime_error("Missing resource: "+key);return read_file(it->second); }
std::string Resources::text(std::string s)const{auto b=read(s);return std::string(b.begin(),b.end());}std::vector<std::string>Resources::paths()const{std::vector<std::string>r;for(auto&x:index_)r.push_back(x.first);return r;}
std::string Resources::texture(std::string geometry,std::string ref)const{
 ref=clean_path(ref);
 if(ref.empty())return {};
 auto source=fs::u8path(clean_path(geometry));
 auto path=fs::u8path(ref);
 const auto leaf=path.filename();
 auto find=[&](fs::path candidate)->std::string{
  if(exists(candidate.generic_u8string()))return candidate.generic_u8string();
  for(auto ext:{".dds",".tga"}){
   candidate.replace_extension(ext);
   if(exists(candidate.generic_u8string()))return candidate.generic_u8string();
  }
  return {};
 };
 const bool rooted=path.has_parent_path();
 if(rooted)if(auto exact=find(path);!exact.empty())return exact;
 auto folder=source.parent_path();
 if(auto local=find(folder/path);!local.empty())return local;
 for(int i=0;i<5&&!folder.empty();++i,folder=folder.parent_path()){
  if(auto local=find(folder/"textures"/leaf);!local.empty())return local;
 }
 if(!rooted)if(auto exact=find(path);!exact.empty())return exact;
 const auto key=source.generic_u8string();
 if(key.rfind("alpha_content/",0)==0){
  for(auto scope:{"alpha_content/cars/shared","alpha_content/tracks/shared"})
   if(auto shared=find(fs::path(scope)/leaf);!shared.empty())return shared;
  return {};
 }
 for(auto scope:{"cars/shared","tracks/shared"})
  if(auto shared=find(fs::path(scope)/leaf);!shared.empty())return shared;
 return {};
}

namespace {
struct Reader {const Bytes&b;std::string path;size_t p=0;void need(size_t n){if(n>b.size()-std::min(p,b.size()))throw std::runtime_error(path+": truncated geometry at "+std::to_string(p));}template<class T>T get(){need(sizeof(T));T v;std::memcpy(&v,b.data()+p,sizeof v);p+=sizeof v;return v;}uint32_t u(){return get<uint32_t>();}float f(){auto x=get<float>();if(!std::isfinite(x))throw std::runtime_error(path+": nonfinite vertex");return x;}void skip(size_t n){need(n);p+=n;}uint32_t count(uint32_t max=1000000){auto n=u();if(n>max)throw std::runtime_error(path+": excessive record count at "+std::to_string(p));return n;}std::string str(){size_t q=p;while(p<b.size()&&b[p]&&p-q<4096)++p;if(p==b.size()||p-q==4096)throw std::runtime_error(path+": unterminated string");std::string s((char*)b.data()+q,p-q);++p;return s;}Vec vec(){return{f(),f(),f()};}Mat matrix(){Mat m;for(auto&v:m)v=f();return m;}};
struct Stream{uint32_t type=0,count=0,stride=0,flags=0;size_t offset=0;};
struct Surf{uint32_t material=0,nv=0,flags=0,np=0,mode=0,ni=0,vs=0,is=0,vo=0,io=0,ns=0;};
struct BedParser {
 std::vector<std::string> t;size_t p=0;
 explicit BedParser(const std::string&s){for(size_t i=0;i<s.size();){unsigned char c=s[i];if(std::isspace(c)||c==';'){++i;continue;}if(c=='#'||(c=='-'&&i+1<s.size()&&s[i+1]=='-')){while(i<s.size()&&s[i]!='\n')++i;continue;}if(c=='"'||c=='\''){char quote=s[i++];std::string v="\"";while(i<s.size()&&s[i]!=quote){if(s[i]=='\\'&&i+1<s.size())++i;v+=s[i++];}if(i==s.size())throw std::runtime_error("Unterminated BED string");++i;t.push_back(v);continue;}if(std::isalnum(c)||c=='_'||c=='.'||c=='-'||c=='+'){size_t q=i++;while(i<s.size()&&(std::isalnum((unsigned char)s[i])||s[i]=='_'||s[i]=='.'||s[i]=='-'||s[i]=='+'))++i;t.push_back(s.substr(q,i-q));}else t.push_back(s.substr(i++,1));}}
 std::string take(){if(p==t.size())throw std::runtime_error("Unexpected BED end");return t[p++];}bool at(const char*s){return p<t.size()&&t[p]==s;}
 Json value(){auto v=take();if(v=="{"){Json o=Json::object();unsigned index=1;while(!at("}")){std::string key;if(at("[")){take();key=take();if(!key.empty()&&key[0]=='"')key.erase(0,1);if(take()!="]"||take()!="=")throw std::runtime_error("Bad BED key");}else if(p+1<t.size()&&t[p+1]=="="){key=take();take();}else key=std::to_string(index++);o[key]=value();if(at(","))take();}take();return o;}if(!v.empty()&&v[0]=='"')return v.substr(1);if(v=="true")return true;if(v=="false")return false;char*end=nullptr;double d=std::strtod(v.c_str(),&end);if(end!=v.c_str()&&!*end){if(!std::isfinite(d))throw std::runtime_error("Nonfinite BED number");return d;}if(at("(")){take();std::string expr=v+"(";int depth=1;while(depth){auto a=take();if(a=="(")++depth;if(a==")")--depth;expr+=a;}return Json{{"$expression",expr}};}return Json{{"$reference",v}};}
 Json root(){Json out=Json::object();while(p<t.size()){auto name=take();if(at("=")){take();out[name]=value();}else while(p<t.size()&&t[p]!="="&&t[p]!="{")++p;if(at(","))take();}return out;}
};
Json settings(const Resources&r,std::string file,std::string key){auto j=bed_file(r,file);if(!j.contains(key)||!j[key].contains("Settings"))throw std::runtime_error("Missing settings: "+key+" in "+file);return j[key]["Settings"];}
float number(const Json&j,const char*k,float fallback){if(!j.contains(k))return fallback;if(!j[k].is_number())throw std::runtime_error(std::string("Unresolved physical field: ")+k);return j[k].get<float>();}
}
Json parse_bed(const std::string&s){return BedParser(s).root();}Json bed_file(const Resources&r,const std::string&p){return parse_bed(r.text(p));}Vec vector3(const Json&j){return{j.at("1").get<float>(),j.at("2").get<float>(),j.at("3").get<float>()};}
static Geometry load_geometry_impl(const Resources&r,const std::string&path,const std::string&stage,bool props_only){auto key=clean_path(path);if(alpha_car_resource(key))return load_alpha_car_geometry(r,key);if(alpha_track_path(clean_path(path)))return load_alpha_track(r,clean_path(path),props_only);auto bytes=r.read(path);Reader f{bytes,path};Geometry out;out.source=clean_path(path);out.track=lower(fs::path(path).extension().string())==".w32";auto version=f.u();if(version!=0x10004&&version!=0x10005)throw std::runtime_error(path+": unsupported geometry version "+std::to_string(version));auto nm=f.count(10000);for(uint32_t i=0;i<nm;i++){if(f.u()!=0x4354414d)throw std::runtime_error(path+": missing MATC");Material m;m.name=f.str();m.alpha=f.u();f.skip(8);m.shader=f.u();f.skip(32+68);for(auto&tx:m.texture)tx=f.str();out.materials.push_back(m);}
 uint32_t stage_mask=0;
 if(out.track&&!stage.empty()){
  out.stage_source=clean_path(stage);
  if(stage_geometry(out.stage_source)!=out.source)throw std::runtime_error(path+": stage belongs to another location");
  auto variant=fs::path(out.stage_source).filename().string();
  if(variant.size()!=1||variant[0]<'a'||variant[0]>'c')throw std::runtime_error(path+": unsupported native stage variant "+variant);
  // FO1 compact placement flags use bits 13/14/15 for variants a/b/c.
  // These are authored inclusion masks: shared objects have all three bits.
  // Corroborated by FlatOutW32BGMTool commandline.h
  // CMD_W32_FBXSkipHiddenPropsA/B/C and w32fbxexport.h compact mesh filter.
  stage_mask=0x2000u<<(variant[0]-'a');
 }
 std::vector<Stream> streams;for(auto n=f.count(100000);n--;){Stream s;s.type=f.u();if(f.u()!=0)throw std::runtime_error(path+": unsupported compressed stream");s.count=f.count(30000000);if(s.type==1||s.type==3){s.stride=f.count(1024);if(s.type==1)s.flags=f.u();s.offset=f.p;f.skip(size_t(s.count)*s.stride);}else if(s.type==2){s.stride=2;s.offset=f.p;f.skip(size_t(s.count)*2);}else throw std::runtime_error(path+": unknown stream type");streams.push_back(s);}
 std::vector<Surf> surfaces;for(auto n=f.count(100000);n--;){Surf s;f.u();s.material=f.u();s.nv=f.count(3000000);s.flags=f.u();s.np=f.count(3000000);s.mode=f.u();s.ni=f.count(10000000);f.skip(24);s.ns=f.count(2);if(s.ns<1)throw std::runtime_error(path+": no vertex stream");s.vs=f.u();s.vo=f.u();if(s.ns==2){s.is=f.u();s.io=f.u();}if(s.material>=out.materials.size()||s.vs>=streams.size()||(s.ns==2&&s.is>=streams.size()))throw std::runtime_error(path+": surface reference out of range");surfaces.push_back(s);}
 Bytes vertex_colors;if(out.track){auto base=clean_path(stage.empty()?fs::path(out.source).parent_path().parent_path().generic_u8string()+"/a":stage);out.lighting_source=base+"/lighting/vertexcolors_w2.w32";if(r.exists(out.lighting_source)){vertex_colors=r.read(out.lighting_source);if(vertex_colors.size()%4)throw std::runtime_error("Invalid stage vertexcolor table: "+out.lighting_source);}else out.lighting_source.clear();}
 std::vector<uint32_t> tree_palette;auto emit=[&](size_t id,std::string name,Mat transform,Vec tree_scale=Vec{1,1,1},uint32_t color_offset=0xffffffffu){if(id>=surfaces.size())throw std::runtime_error(path+": invalid surface");auto&s=surfaces[id];auto&v=streams[s.vs];
 if(v.type==3&&s.mode==0){
  if(v.stride!=28||s.nv!=uint64_t(s.np)*4||uint64_t(s.vo)+uint64_t(s.np)*v.stride>uint64_t(v.count)*v.stride)throw std::runtime_error(path+": invalid native tree sprite span");
  Mesh m;m.name=name;m.source_surface=int(id);m.material=s.material;m.billboard=true;m.solid=false;
  Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
  for(uint32_t j=0;j<s.np;j++){
   Reader a{bytes,path,v.offset+s.vo+size_t(j)*v.stride};auto p=a.vec();p={p.x*tree_scale.x,p.y*tree_scale.y,p.z*tree_scale.z};auto center=point(transform,p);float w=a.f()*std::max(std::abs(tree_scale.x),std::abs(tree_scale.z)),h=a.f()*std::abs(tree_scale.y);auto rect=a.u(),color=a.u();
   if(w<=0||h<=0||w>1000||h>1000)throw std::runtime_error(path+": invalid native tree sprite size");
   if(color_offset!=0xffffffffu){if(size_t(color_offset)+j>=tree_palette.size())throw std::runtime_error(path+": tree palette range");color=tree_palette[size_t(color_offset)+j];}
   if(color<0xff000000u){auto index=size_t(color&0xffffffu);if(!vertex_colors.empty()){if(index>=vertex_colors.size()/4)throw std::runtime_error(path+": tree sprite lighting range");std::memcpy(&color,vertex_colors.data()+index*4,4);}else color=0xffffffffu;}color|=0xff000000u;
   // Some winter template placeholders intentionally have a zero UV span.
   float u0=float(rect&255)/16.f,v0=float((rect>>8)&255)/16.f,u1=float((rect>>16)&255)/16.f,v1=float(rect>>24)/16.f;
   if(u0<0||v0<0||u1>1||v1>1||u1<u0||v1<v0)throw std::runtime_error(path+": tree sprite atlas rectangle");
   if(u0==u1||v0==v1)continue;
   constexpr float inset=.5f/1024.f;
   u0+=inset;u1-=inset;v0+=inset;v1-=inset;
   uint32_t first=uint32_t(m.vertices.size());for(int k=0;k<4;k++){Vertex q;float dx=(k==0||k==3)?-w:w,dy=k<2?-h:h;q.position=center+Vec{dx,dy,0};q.normal={dx,dy,0};q.color=color;q.u=q.lu=(k==0||k==3)?u0:u1;q.v=q.lv=k<2?v1:v0;m.vertices.push_back(q);}
   for(auto k:{0u,1u,2u,0u,2u,3u})m.indices.push_back(first+k);
   float radius=std::hypot(w,h);for(int k=0;k<3;k++){lo[k]=std::min(lo[k],center[k]-radius);hi[k]=std::max(hi[k],center[k]+radius);}
  }
  if(m.vertices.empty()){m.center=point(transform,{});m.radius=0;}
  else{m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;}
  out.triangles+=m.indices.size()/3;out.meshes.push_back(std::move(m));return;
 }
 if(v.type!=1||v.stride<12||uint64_t(s.vo)+uint64_t(s.nv)*v.stride>uint64_t(v.count)*v.stride)throw std::runtime_error(path+": vertex span out of range");Mesh m;m.source_surface=int(id);m.material=s.material;m.name=name;m.transform=transform;m.vertices.reserve(s.nv);Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};for(uint32_t i=0;i<s.nv;i++){Reader a{bytes,path,v.offset+s.vo+size_t(i)*v.stride};Vertex q;q.position=a.vec();size_t used=12;if(v.flags&0x10){q.normal=a.vec();used+=12;}else q.normal={0,1,0};if(v.flags&0x40){q.color=a.u();if(out.track&&q.color<0xff000000u){auto index=size_t(q.color&0xffffff);if(!vertex_colors.empty()){if(index>=vertex_colors.size()/4)throw std::runtime_error(path+": vertexcolor reference out of range");std::memcpy(&q.color,vertex_colors.data()+index*4,4);q.color|=0xff000000u;}else q.color=0xffffffffu;}used+=4;}unsigned uv=(v.flags>>8)&15;if(uv&&used+8<=v.stride){q.u=a.f();q.v=a.f();used+=8;}if(uv>1&&used+8<=v.stride){q.lu=a.f();q.lv=a.f();}else{q.lu=q.u;q.lv=q.v;}auto world=point(transform,q.position);for(int k=0;k<3;k++){lo[k]=std::min(lo[k],world[k]);hi[k]=std::max(hi[k],world[k]);}m.vertices.push_back(q);}m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;
 // Native indices are absolute within the shared vertex stream. Rebase every
 // index by this surface's first vertex; deciding per index using nv corrupts
 // spans whose absolute and local ranges overlap (and still passes bounds).
 std::vector<uint32_t> indices;if(s.ns==2){auto&ib=streams[s.is];if(s.vo%v.stride)throw std::runtime_error(path+": unaligned vertex span");if(ib.type!=2||uint64_t(s.io)+uint64_t(s.ni)*2>uint64_t(ib.count)*2)throw std::runtime_error(path+": index span out of range");Reader a{bytes,path,ib.offset+s.io};const auto base=s.vo/v.stride;for(uint32_t i=0;i<s.ni;i++){uint32_t index=a.get<uint16_t>();if(index<base||uint64_t(index)-base>=s.nv)throw std::runtime_error(path+": vertex index out of range");indices.push_back(index-base);}}else for(uint32_t i=0;i<s.nv;i++)indices.push_back(i);
 if(s.mode==4){for(size_t i=0;i+2<indices.size();i+=3)for(int k=0;k<3;k++)m.indices.push_back(indices[i+k]);}else if(s.mode==5){for(size_t i=2;i<indices.size();i++){auto a=indices[i-2],b=indices[i-1],c=indices[i];if(a==b||a==c||b==c)continue;if(i&1)std::swap(a,b);m.indices.insert(m.indices.end(),{a,b,c});}}else throw std::runtime_error(path+": unsupported primitive mode");out.triangles+=m.indices.size()/3;out.meshes.push_back(std::move(m));};
 if(out.track){
  auto n=f.count(100000);out.native_static_batches=n;
  for(uint32_t i=0;i<n;i++){f.u();auto surface=f.u();f.u();f.u();if(!props_only)emit(surface,"static_"+std::to_string(i),identity());}
  // FO1 W32 0x10004/5: palette, billboard LODs, full tree surfaces,
  // collision transform, models, dummies, then compact placements.
  // Trunk/branch vertices are world-space. Reusable leaf sprite records
  // are local and need the tree matrix, scale and per-instance color palette.
  auto colors=f.count(30000000);
  if(props_only)f.skip(size_t(colors)*4);
  else{tree_palette.reserve(colors);for(uint32_t k=0;k<colors;k++)tree_palette.push_back(f.u());}
  auto lods=f.count(1000000);f.skip(size_t(lods)*28);
  n=f.count(100000);out.native_tree_instances=n;
  for(uint32_t i=0;i<n;i++){
   f.skip(16);auto tree_transform=f.matrix();auto scale=f.vec();std::array<uint32_t,3>ids;for(auto&id:ids)id=f.u();auto palette=f.u();f.u();f.u();
   if(props_only)continue;
   for(int k=0;k<3;k++){auto id=ids[k];if(id==0xffffffffu)continue;if(id>=surfaces.size())throw std::runtime_error(path+": invalid tree surface");emit(id,"tree_"+std::to_string(i)+"_"+std::to_string(k),k==2?tree_transform:identity(),scale,k==2?palette:0xffffffffu);out.meshes.back().solid=k==0;++out.native_tree_batches;}
  }
  f.matrix();std::vector<std::vector<uint32_t>>models;std::vector<NativePropPlacement> model_info;
  n=f.count(100000);for(uint32_t i=0;i<n;i++){if(f.u()!=0x444f4d42)throw std::runtime_error(path+": missing track BMOD");NativePropPlacement model;model.model_flags=f.u();model.model_name=f.str();model.center=f.vec();model.half=f.vec();f.f();std::vector<uint32_t>ids;for(auto count=f.count(100000);count--;){auto id=f.u();if(id>=surfaces.size())throw std::runtime_error(path+": invalid model surface");ids.push_back(id);}model.surfaces=ids;model_info.push_back(std::move(model));models.push_back(std::move(ids));}
  n=f.count(100000);for(uint32_t i=0;i<n;i++){if(f.u()!=0x434a424f)throw std::runtime_error(path+": missing track OBJC");auto name=f.str();f.str();f.u();out.markers[lower(name)]=f.matrix();}
  f.count(100000);n=f.count(100000);out.native_compact_instances=n;
  for(uint32_t i=0;i<n;i++){if(f.u()!=0x4853454d)throw std::runtime_error(path+": missing track MESH");auto name=f.str();auto type=f.str();auto placement_flags=f.u();auto group_index=f.u();auto transform=f.matrix();auto count=f.count(1000);uint32_t first=0;for(uint32_t k=0;k<count;k++){auto id=f.u();if(id>=models.size())throw std::runtime_error(path+": invalid compact LOD");if(!k)first=id;}if(count){if(stage_mask&&!(placement_flags&stage_mask)){++out.filtered_compact_instances;continue;}auto placement=model_info[first];placement.placement_flags=placement_flags;placement.group_index=group_index;placement.name=name;placement.type=type;placement.transform=transform;placement.native_index=i;out.native_props.push_back(std::move(placement));for(auto id:models[first]){emit(id,name,transform);++out.native_compact_batches;}}}
  if(f.p!=bytes.size())throw std::runtime_error(path+": unexpected W32 tail");
 }
 else{std::vector<std::vector<uint32_t>>models;auto n=f.count(100000);for(uint32_t i=0;i<n;i++){if(f.u()!=0x444f4d42)throw std::runtime_error(path+": missing BMOD");f.u();f.str();f.skip(28);std::vector<uint32_t>a;for(auto count=f.count(100000);count--;)a.push_back(f.u());models.push_back(a);}n=f.count(100000);for(uint32_t i=0;i<n;i++){if(f.u()!=0x4853454d)throw std::runtime_error(path+": missing MESH");auto name=f.str();f.str();f.u();f.u();auto matrix=f.matrix();auto count=f.count(1000);std::vector<uint32_t>ids;while(count--)ids.push_back(f.u());out.markers[lower(name)]=matrix;if(lower(name).find("shadow")!=name.npos)continue;if(!ids.empty()){if(ids[0]>=models.size())throw std::runtime_error(path+": model reference");for(auto id:models[ids[0]])emit(id,name,matrix);}}n=f.count(100000);for(uint32_t i=0;i<n;i++){if(f.u()!=0x434a424f)throw std::runtime_error(path+": missing OBJC");auto name=f.str();f.str();f.u();out.markers[lower(name)]=f.matrix();}}
 return out;}
Geometry load_geometry(const Resources& r,const std::string& path,const std::string& stage){
 return load_geometry_impl(r,path,stage,false);
}
Geometry load_track_prop_geometry(const Resources& r,const std::string& path){
 return load_geometry_impl(r,path,{},true);
}
Json geometry_summary(const Geometry&g){size_t sprites=0;for(auto&m:g.meshes)if(m.billboard)sprites+=m.vertices.size()/4;return {{"source",g.source},{"triangles",g.triangles},{"meshes",g.meshes.size()},{"static_batches",g.native_static_batches},{"tree_instances",g.native_tree_instances},{"tree_batches",g.native_tree_batches},{"tree_sprites",sprites},{"compact_instances",g.native_compact_instances},{"compact_batches",g.native_compact_batches},{"vertex_lighting",g.lighting_source}};}
VehicleConfig load_vehicle(const Resources&r,int id,const fs::path&profiles){if(alpha_car_id(id))return load_alpha_vehicle(r,id);if(id<1||id>16)throw std::runtime_error("Original car ID must be 1..16");VehicleConfig c;c.id=id;auto n=std::to_string(id),base="equipmnt/car"+n+"/";auto root=bed_file(r,base+"body_car"+n+".bed");auto body=root.at("Car.Body.Car"+n).at("Settings");auto engine=settings(r,"equipmnt/custom/engine_"+n+".bed","Car.Engine.Car"+n+"_1");auto gear=settings(r,"equipmnt/custom/gearbox_"+n+".bed","Car.Gearbox.Car"+n+"_1");auto suspension=settings(r,"equipmnt/custom/suspension_"+n+".bed","Car.Suspension.Car"+n+"_1");c.name=root.at("Car.Body.Car"+n).at("Name");c.body="cars/car_"+n+"/body.bgm";c.tire="cars/shared/tire_1.bgm";if(!r.exists(c.tire))c.tire="cars/shared/tire_2.bgm";c.authored_mass=number(body,"Mass",0)+number(engine,"Mass",0);c.mass=c.authored_mass;c.center_mass=vector3(body.at("CenterOfMass"));c.inertia=vector3(body.at("MomentOfInertia"))*c.mass;c.drag=vector3(body.at("AeroDrag"));c.front_drive=number(body,"FrontTraction",0);c.brake_torque=number(body,"BrakeTorque",8500);c.handbrake=number(body,"HandBrakeTorque",5000);c.downforce=number(body,"DownForceConst",2);c.steering_angle=(number(body,"TireTurnAngleIn",35)+number(body,"TireTurnAngleOut",35))*.5f;c.steering_limit=number(body,"SteeringLimit",45);c.steering_reduction=number(body,"SteeringReduction",.4f);c.engine_torque=number(engine,"TorqueMax",125);c.idle_rpm=number(engine,"IdleRpm",950);c.nitro_g=number(engine,"NitroAcceleration",1.2);c.nitro_capacity=number(engine,"NitroStorage",5);c.final_ratio=number(gear,"EndRatio",3.8);for(int i=1;i<=int(number(gear,"NumGears",4));i++)c.ratios.push_back(gear.at("Gear"+std::to_string(i)+"Ratio").at("2").get<float>());for(int i=0;i<int(number(engine,"TorqueCurveParams",20));i++){char key[32];std::snprintf(key,sizeof key,"Torque%02d",i);if(engine.contains(key))c.torque_curve.push_back({engine[key]["1"],engine[key]["2"]});}c.max_rpm=c.torque_curve.empty()?7000:c.torque_curve.back().first;
 c.engine_inertia=number(engine,"InertiaEngine",.12f);c.shaft_inertia=number(engine,"InertiaDriveShaft",.06f);c.engine_friction=number(engine,"EngineFriction",.055f);c.reverse_ratio=gear.at("GearRRatio").at("2");c.clutch_torque=number(gear,"ClutchTorque",2*c.engine_torque);c.clutch_release=number(gear,"ClutchReleaseTime",.1f);c.gear_shift=number(gear,"GearShiftTime",.08f);c.clutch_engage=number(gear,"ClutchEngageTime",.15f);
 c.aero_location=body.contains("AeroDragLoc")?vector3(body.at("AeroDragLoc")):c.center_mass;c.downforce_location=body.contains("CenterOfDownForce")?vector3(body.at("CenterOfDownForce")):c.center_mass;auto damp=body.value("Damping",Json::object());c.damping={number(damp,"1",.01f),number(damp,"2",.05f)};
 if(body.contains("BrakeBalance"))c.brake_front=number(body.at("BrakeBalance"),"2",.55f);
 if(c.ratios.empty()||c.reverse_ratio>=0||c.engine_inertia<=0||c.shaft_inertia<0||c.clutch_torque<=0||c.torque_curve.size()<2)throw std::runtime_error("Invalid native drivetrain");
 for(size_t i=0;i<c.torque_curve.size();++i)if(!std::isfinite(c.torque_curve[i].second)||c.torque_curve[i].second<0||(i&&c.torque_curve[i].first<=c.torque_curve[i-1].first))throw std::runtime_error("Invalid native torque curve");
 auto tire_root=bed_file(r,base+"tires_car"+n+".bed");auto tire_config=tire_root.at("Car.Tires.Car"+n+"Tire1").at("Settings");auto tire_expr=tire_config.at("$expression").get<std::string>();int tire_id=tire_expr=="GetDefaults(TIRE2)"?2:tire_expr=="GetDefaults(TIRE1)"?1:0;if(!tire_id)throw std::runtime_error("Unsupported native tire defaults: "+tire_expr);auto tire=settings(r,"equipmnt/default/tires_default.bed","Car.Tires.DefaultTire"+std::to_string(tire_id));c.tire="cars/shared/tire_"+std::to_string(tire_id)+".bgm";auto body_ini=bed_file(r,"cars/car_"+n+"/body.ini");c.lo=vector3(body_ini.at("CollisionBottomMin"));c.hi=vector3(body_ini.at("CollisionBottomMax"));c.top_lo=vector3(body_ini.at("CollisionTopMin"));c.top_hi=vector3(body_ini.at("CollisionTopMax"));c.has_top_collision=true;auto tire_geometry=load_geometry(r,c.tire);auto g=load_geometry(r,c.body);std::vector<std::pair<std::string,Vec>> mounts;for(auto&kv:g.markers)if(kv.first.rfind("placeholder_tire_",0)==0)mounts.push_back({kv.first,point(kv.second,{})});if(mounts.size()<4)throw std::runtime_error("Four authored tire markers missing");std::sort(mounts.begin(),mounts.end(),[](auto&a,auto&b){return a.second.z>b.second.z;});for(int axis=0;axis<2;axis++){if(mounts[axis*2].second.x>mounts[axis*2+1].second.x)std::swap(mounts[axis*2],mounts[axis*2+1]);auto s=suspension.at(axis?"Rear":"Front");for(int k=0;k<2;k++){auto&w=c.wheels[axis*2+k];w.mount=mounts[axis*2+k].second;w.mesh_name=mounts[axis*2+k].first.substr(12);w.width=number(tire,"Width",.185f);w.radius=number(tire,"Radius",.34);w.mass=number(tire,"Mass",50);w.inertia_factor=number(tire,"MomentOfInertia",1);w.rest=number(s,"RestLength",.32);w.min=number(s,"MinLength",.05);w.max=number(s,"MaxLength",.55);w.k=number(body,"Mass",0)*.25f*9.81f/number(s,"DefaultCompression",.11);float critical=2*std::sqrt(w.k*number(body,"Mass",0)*.25f);w.bump=critical*number(s,"BumpDamp",.35);w.rebound=critical*number(s,"ReboundDamp",.4);}}
 auto tire_local=bed_file(r,"cars/car_"+n+"/tires.ini").at("LocalDynamics");auto tire_equipment=tire.at("LocalDynamics");auto&td=c.tire_dynamics;td.authored=true;c.mu=1;td.longitudinal=number(tire_local.at("ZFriction"),"1",1)*number(tire_equipment.at("ZFriction"),"1",1);td.lateral=number(tire_local.at("XFriction"),"1",1)*number(tire_equipment.at("XFriction"),"1",1);td.optimal_slip_ratio=number(tire_equipment,"OptimalSlipRatio",.1f);td.optimal_slip_angle=number(tire_equipment,"OptimalSlipAngle",28);td.optimal_slip_load=number(tire_equipment,"OptimalSlipLoad",1.5f);td.optimal_load_factor=number(tire_equipment,"OptimalLoadFactor",1.2f);td.rolling_resistance=number(tire_local,"RollingResistance",1)*number(tire_equipment,"RollingResistance",1);
 // Tire collision is a convex adapter of the original per-corner mesh, never its shadow.
 for(auto&w:c.wheels){
  Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};float radius=0;
  for(auto&m:tire_geometry.meshes)if(lower(m.name)==w.mesh_name)for(auto&v:m.vertices){auto q=point(m.transform,v.position);w.collision_vertices.push_back(q);radius=std::max(radius,std::hypot(q.y,q.z));for(int k=0;k<3;k++){lo[k]=std::min(lo[k],q[k]);hi[k]=std::max(hi[k],q[k]);}}
  if(w.collision_vertices.size()<8||radius<.01f||hi.x-lo.x<.01f)throw std::runtime_error("Invalid original tire geometry: "+w.mesh_name);
  w.mesh_scale={w.width/(hi.x-lo.x),w.radius/radius,w.radius/radius};
  for(auto&q:w.collision_vertices){q.x*=w.mesh_scale.x;q.y*=w.mesh_scale.y;q.z*=w.mesh_scale.z;}
 }
 c.provenance={{"body",base+"body_car"+n+".bed"},{"engine","equipmnt/custom/engine_"+n+".bed"},{"mass_source","authored physical Settings; menu ignored"},{"spring_source","body mass/4 *9.81 / DefaultCompression"},{"gearbox","equipmnt/custom/gearbox_"+n+".bed"},{"engine_audio","sound/engine_"+n+"_0.bed"},{"torque_curve_points",c.torque_curve.size()},{"drivetrain_adapter","native curve/count, inertia/friction, reverse ratio and clutch timings; custom rotor/clutch and automatic shift controller"}};
 if(!std::isfinite(td.longitudinal)||td.longitudinal<=0||!std::isfinite(td.lateral)||td.lateral<=0||td.optimal_slip_ratio<=0||td.optimal_slip_angle<=0)throw std::runtime_error("Invalid native tire dynamics");
 c.provenance["tire_dynamics"]={{"car_local",tire_local},{"equipment_local",tire_equipment},{"source", "cars/car_"+n+"/tires.ini and equipmnt/default/tires_default.bed"},{"adapter","anisotropic peak friction and brush slip response; negative friction components/load law and aligning moment semantics unverified"}};
 auto global_tires=bed_file(r,"global/dynamics/tiredynamics.bed").at("GlobalDynamics");auto surface_definitions=bed_file(r,"global/dynamics/surfaces.bed").at("Surfaces");
 for(auto it=surface_definitions.begin();it!=surface_definitions.end();++it){auto surface=it.value();for(const auto&d:global_tires)if(d.value("Name",std::string())==surface.value("Dynamics",std::string())){float friction=number(surface,"Friction",1);c.tire_surfaces[std::stoi(it.key())]={friction*number(d.at("ZFriction"),"1",1),friction*number(d.at("XFriction"),"1",1),number(d,"RollingResistance",0)*td.rolling_resistance};break;}}
 c.provenance["tire_dynamics"]["global_dynamics"]=global_tires;c.provenance["tire_dynamics"]["surfaces"]=surface_definitions;c.provenance["tire_dynamics"]["surface_selection"]="material/texture name adapter; unknown material uses Tarmac id 2; original per-triangle dynamics IDs not decoded";
 auto rates=body.value("SteeringSpeeds",Json::object());c.provenance["handling"]={{"steering_rates",Json::array({number(rates,"1",1.05f),number(rates,"2",1.45f)})},{"rollbar",Json::array({number(suspension.at("Front"),"RollBarStiffness",.1f),number(suspension.at("Rear"),"RollBarStiffness",.1f)})},{"source","native SteeringSpeeds (maximum input per second), SteeringReduction at SteeringLimit, RollBarStiffness; rollbar force adapter"}};
 if(!profiles.empty()){auto p=profiles/("car"+n+".json");if(fs::exists(p)){std::ifstream f(p);Json j;f>>j;if(j.at("name")!=c.name)throw std::runtime_error("Reference profile identity mismatch");c.mass=j.at("mass_native");c.inertia=vector3(j.at("inertia_vector"));c.profile=j.at("source");c.provenance["effective_mass_source"]=c.profile;c.provenance["configuration_warning"]="Observed reference configuration override; stock/tuning equivalence not established. Authored mass remains separately reported.";}}
 if(c.mass<100||c.mass>10000||c.inertia.x<=0||c.inertia.y<=0||c.inertia.z<=0)throw std::runtime_error("Invalid physical configuration");return c;}
Mat load_spawn(const Resources&r,const std::string&stage,int index){auto j=bed_file(r,stage+"/data/startpoints.bed").at("Startpoints").at(std::to_string(index));auto o=j.at("Orientation");auto m=identity();auto x=vector3(o.at("x")),y=vector3(o.at("y")),z=vector3(o.at("z")),p=vector3(j.at("Position"));for(int k=0;k<3;k++){m[k]=x[k];m[4+k]=y[k];m[8+k]=z[k];m[12+k]=p[k];}return m;}
std::vector<std::string>track_catalog(const Resources&r){std::vector<std::string>t;for(auto&p:r.paths())if(p.size()>14&&p.find("/menu/")==p.npos&&p.find("/geometry/track_geom.w32")!=p.npos)t.push_back(p);return t;}
std::string stage_geometry(const std::string&s){if(alpha_track_path(clean_path(s)))return clean_path(s)+"/geometry/track_geom.w32";return fs::path(clean_path(s)).parent_path().generic_u8string()+"/geometry/track_geom.w32";}
std::vector<std::string> first_build_stages(const Resources&r){std::vector<std::string>out;for(auto s:{"tracks/forest/forest1/a","tracks/forest/forest2/a"})if(r.exists(stage_geometry(s))&&r.exists(std::string(s)+"/data/startpoints.bed"))out.push_back(s);return out;}
}
