#include "alpha_track.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <regex>
#include <unordered_map>
namespace ofo {
namespace {
struct Cursor{
 const Bytes& bytes;size_t at=0;std::string source;
 [[noreturn]] void fail(const char* reason)const{throw std::runtime_error(source+" @ "+std::to_string(at)+": "+reason);}
 const uint8_t* take(size_t n){if(n>bytes.size()-at)fail("truncated");auto p=bytes.data()+at;at+=n;return p;}
 template<class T>T read(){T a;std::memcpy(&a,take(sizeof a),sizeof a);return a;}
 unsigned u(){return read<uint32_t>();}
 unsigned count(unsigned cap){auto n=u();if(n>cap)fail("invalid count");return n;}
 float f(){auto a=read<float>();if(!std::isfinite(a))fail("nonfinite value");return a;}
 Vec v(){return{f(),f(),f()};}
 std::string str(){auto start=at;while(at<bytes.size()&&bytes[at]&&at-start<1024)++at;if(at==bytes.size()||bytes[at])fail("bad string");++at;return std::string(reinterpret_cast<const char*>(bytes.data()+start),at-start-1);}
 void tag(const char* s){if(std::memcmp(take(4),s,4))fail("unexpected chunk");}
};
void bounds(Mesh&m){
 Vec lo{1e20f,1e20f,1e20f},hi{-1e20f,-1e20f,-1e20f};
 for(const auto&v:m.vertices){auto p=point(m.transform,v.position);for(int k=0;k<3;++k){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}}
 m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;
}
void sprite_bounds(Mesh&m){
 Vec lo{1e20f,1e20f,1e20f},hi{-1e20f,-1e20f,-1e20f};
 for(const auto&v:m.vertices){
  auto c=v.position-v.normal;const float r=length(v.normal);
  for(int k=0;k<3;++k){lo[k]=std::min(lo[k],c[k]-r);hi[k]=std::max(hi[k],c[k]+r);}
 }
 m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;
}
void normals(Mesh&m){
 for(size_t i=0;i+2<m.indices.size();i+=3){
  auto&a=m.vertices[m.indices[i]],&b=m.vertices[m.indices[i+1]],&c=m.vertices[m.indices[i+2]];
  auto n=cross(b.position-a.position,c.position-a.position);
  a.normal=a.normal+n;b.normal=b.normal+n;c.normal=c.normal+n;
 }
 for(auto&v:m.vertices)v.normal=unit(v.normal,{0,1,0});
}
const char* surfaces[]={"tarmac","nocollision","tarmac","tarmac paint","gravel hard","gravel hard paint","gravel","gravel paint","sand road","sand road paint","ice","ice paint","snow","snow paint","sand bank","grass","forest","sand","rock","mud","concrete","snow","rock","metal","wood","forest tree","forest bush","rubber","water","reset"};
void load_collision(const Resources&r,const std::string& key,Geometry& g){
 auto raw=r.read(key);Cursor f{raw,0,key};f.tag("aabb");if(f.u()!=0x20020722)f.fail("unknown collision version");
 f.take(8);auto length=f.count(64*1024*1024);if(length%4)f.fail("invalid vertex buffer");
 auto vertices=f.take(length);auto n=f.count(2000000);
 auto begin=g.materials.size();
 std::array<Mesh,30> parts;
 std::array<std::unordered_map<uint32_t,uint32_t>,30> local;
 for(size_t i=0;i<parts.size();++i){Material m;m.name=surfaces[i];g.materials.push_back(m);parts[i].material=begin+i;parts[i].name=std::string("alpha_collision_")+surfaces[i];}
 for(unsigned i=0;i<n;++i){
  f.read<uint8_t>();auto mat=unsigned(f.read<uint8_t>())+1;f.take(2);std::array<uint32_t,3> offsets{f.u(),f.u(),f.u()};
  if(mat==1||mat==29)continue;
  if(mat>=parts.size())mat=2;
  auto& mesh=parts[mat];auto& ids=local[mat];
  for(auto offset:offsets){
   if(offset%4||length<12||offset>length-12)f.fail("invalid collision index");
   auto [it,inserted]=ids.emplace(offset,uint32_t(mesh.vertices.size()));
   if(inserted){Vertex v{};std::memcpy(&v.position.x,vertices+offset,12);if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.position.z))f.fail("invalid collision point");mesh.vertices.push_back(v);}
   mesh.indices.push_back(it->second);
  }
 }
 for(auto&m:parts)if(!m.indices.empty()){bounds(m);g.collision_meshes.push_back(std::move(m));}
 if(g.collision_meshes.empty())f.fail("empty collision");
}
std::pair<float,float> prop_settings(const Resources&r,const std::string&type){
 auto key="alpha_content/global/dynamics/"+type+".bed";if(!r.exists(key))return{0,0};
 auto s=r.text(key);auto read=[&](const char* n,float fallback){std::smatch m;if(!std::regex_search(s,m,std::regex(std::string("\\b")+n+R"(\s*=\s*([-+0-9.]+))")))return fallback;return std::stof(m[1].str());};
 return{read("Mass",0),read("Restitution",.3f)};
}
int prop_sound(const std::string&n){
 if(n.find("wood")!=n.npos||n=="fencepole"||n=="pole")return 5;
 if(n.find("tire")!=n.npos||n.find("cone")!=n.npos)return 6;
 return 4;
}
}
Geometry load_alpha_track(const Resources&r,const std::string& key,bool props_only){
 Geometry g;g.source=key;g.track=true;g.stage_source=fs::path(key).parent_path().parent_path().generic_u8string();
 const auto stage=g.stage_source;g.lighting_source=stage+"/lighting/lightmap1_w2.dds";
 auto raw=r.read(key);Cursor f{raw,0,key};
 if(f.u()!=0x10003)f.fail("unknown track version");
 auto nm=f.count(1024);
 for(unsigned i=0;i<nm;++i){
  f.tag("MATC");Material m;m.name=f.str();auto alpha=f.u();f.take(12);auto shader=f.u();f.take(8+68);
  for(auto&name:m.texture)name=lower(f.str());
  m.alpha=alpha==5?1:0;m.shader=shader==2?1:shader==1?7:0;
  if(shader!=2&&!m.texture[0].empty()){auto tex=fs::path(m.texture[0]).replace_extension(".dds").filename().generic_u8string();m.texture[0]=stage+"/textures/"+tex;}
  if(shader==2&&!m.texture[1].empty()){auto tex=fs::path(m.texture[1]).replace_extension(".dds").filename().generic_u8string();m.texture[1]=stage+"/textures/"+tex;}
  g.materials.push_back(std::move(m));
 }
 Bytes colors;
 auto colorpath=stage+"/lighting/stage1_sky_vtx.rad";
 if(r.exists(colorpath))colors=r.read(colorpath);
 const auto models_count=f.count(20000);
 std::vector<std::vector<Mesh>> models(models_count);
 for(auto&model:models){
  f.tag("BMOD");f.u();f.take(24);const auto count=f.count(4096);
  for(unsigned j=0;j<count;++j){
   f.tag("BATC");auto format=f.u(),material=f.u(),nv=f.count(65536),flags=f.u(),size=f.count(8*1024*1024);
   auto base=f.take(size);f.u();auto mode=f.u(),index_size=f.count(8*1024*1024);auto index=f.take(index_size);f.take(24);
   if(format==1&&mode==0){
    if(material>=g.materials.size()||nv%4||size!=nv*7||index_size)f.fail("invalid sprite batch");
    Mesh mesh;mesh.material=material;mesh.billboard=true;mesh.solid=false;
    for(unsigned j=0;j<nv/4;++j){
     float data[5];uint32_t rect,color;
     auto p=base+j*28;std::memcpy(data,p,20);std::memcpy(&rect,p+20,4);std::memcpy(&color,p+24,4);
     for(auto x:data)if(!std::isfinite(x))f.fail("invalid sprite geometry");
     auto x=rect&15,y=(rect>>4)&15,w=((rect>>8)&15)+1,h=((rect>>12)&15)+1;
     if(x+w>16||y+h>16||data[3]<=0||data[4]<=0)continue;
     if(color<0xff000000&&uint64_t(color&0xffffff)*4+4<=colors.size())std::memcpy(&color,colors.data()+size_t(color&0xffffff)*4,4);
     else if(color<0xff000000)color=0xffaaaaaa;
     const Vec c{data[0],data[1],data[2]};auto first=uint32_t(mesh.vertices.size());
     for(int k=0;k<4;++k){
      bool left=k==0||k==3,bottom=k<2;Vertex v{};
      v.normal={left?-data[3]:data[3],bottom?-data[4]:data[4],0};
      v.position=c+v.normal;v.color=color|0xff000000;
      constexpr float inset=.5f/1024.f;
      v.u=v.lu=(x+(left?0:w))/16.f+(left?inset:-inset);
      v.v=v.lv=(y+(bottom?h:0))/16.f+(bottom?-inset:inset);mesh.vertices.push_back(v);
     }
     for(auto j:{0u,1u,2u,0u,2u,3u})mesh.indices.push_back(first+j);
    }
    if(!mesh.indices.empty())model.push_back(std::move(mesh));
    continue;
   }
   if(format||material>=g.materials.size()||!nv||size%nv||index_size%2||(mode!=4&&mode!=5))f.fail("invalid batch");
   unsigned stride=size/nv, expected=12+(flags&0x10?12:0)+(flags&0x40?4:0)+(flags&0x300?8:0)+(flags&0x200?8:0);
   if(expected>stride||!(flags&2))f.fail("invalid vertex layout");
   Mesh mesh;mesh.material=material;mesh.vertices.reserve(nv);
   for(unsigned k=0;k<nv;++k){
    auto p=base+size_t(k)*stride;
    auto fl=[&](){float x;std::memcpy(&x,p,4);p+=4;if(!std::isfinite(x))f.fail("invalid vertex");return x;};
    Vertex v{};v.position={fl(),fl(),fl()};
    if(flags&0x10)v.normal={fl(),fl(),fl()};
    if(flags&0x40){uint32_t c;std::memcpy(&c,p,4);p+=4;
     if(c<0xff000000&&uint64_t(c&0xffffff)*4+4<=colors.size())std::memcpy(&c,colors.data()+size_t(c&0xffffff)*4,4);
     else if(c<0xff000000)c=0xffaaaaaa;
     v.color=0xff000000|(c&0xffffff);
    }
    auto uv=[&](){float x;std::memcpy(&x,p,4);p+=4;return std::isfinite(x)&&std::abs(x)<1e6f?x:0.f;};
    if(flags&0x300){v.u=uv();v.v=uv();}
    if(flags&0x200){v.lu=uv();v.lv=uv();}
    mesh.vertices.push_back(v);
   }
   auto idx=[&](unsigned i){uint16_t a;std::memcpy(&a,index+size_t(i)*2,2);if(a>=nv)f.fail("invalid triangle index");return uint32_t(a);};
   auto tri=[&](unsigned a,unsigned b,unsigned c){if(a!=b&&b!=c&&c!=a){mesh.indices.push_back(a);mesh.indices.push_back(b);mesh.indices.push_back(c);}};
   if(mode==4){if(index_size%6)f.fail("incomplete triangle");for(unsigned i=0;i<index_size/2;i+=3)tri(idx(i),idx(i+1),idx(i+2));}
   else for(unsigned i=2;i<index_size/2;++i){if(i&1)tri(idx(i-1),idx(i-2),idx(i));else tri(idx(i-2),idx(i-1),idx(i));}
   if(!(flags&0x10))normals(mesh);
   if(!mesh.indices.empty())model.push_back(std::move(mesh));
  }
 }
 auto terrain=f.count(20000);
 for(unsigned i=0;i<terrain;++i){f.tag("TMOD");f.u();f.take(size_t(f.count(2000000))*12);f.take(size_t(f.count(2000000))*12);}
 std::map<std::string,std::pair<float,float>> props;
 auto placements=f.count(20000);
 for(unsigned i=0;i<placements;++i){
  f.tag("MESH");auto name=f.str(),type=lower(f.str());f.u();Mat matrix;for(auto&v:matrix)v=f.f();
  auto lods=f.count(64);const std::vector<Mesh>* model=nullptr;
  for(unsigned j=0;j<lods;++j){auto id=f.u();if(id>=models.size())f.fail("invalid model reference");if(!model&&!models[id].empty())model=&models[id];}
  f.take(8);
  if(!model)continue;
  int prop=-1;
  if(!type.empty()){
   auto it=props.find(type);if(it==props.end())it=props.emplace(type,prop_settings(r,type)).first;
   if(it->second.first>0){
    Vec lo{1e20f,1e20f,1e20f},hi{-1e20f,-1e20f,-1e20f};
    for(const auto&m:*model)for(const auto&v:m.vertices)for(int k=0;k<3;++k){lo[k]=std::min(lo[k],v.position[k]);hi[k]=std::max(hi[k],v.position[k]);}
    PropDefinition def;def.id=i+1;def.mass=it->second.first;def.restitution=std::clamp(it->second.second,0.f,1.f);
    def.center=(hi+lo)*.5f;def.half=(hi-lo)*.5f;
    for(int k=0;k<3;++k)def.half[k]=std::max(def.half[k],.025f);
    auto a=def.half;def.inertia={def.mass*(a.y*a.y+a.z*a.z)/3,def.mass*(a.x*a.x+a.z*a.z)/3,def.mass*(a.x*a.x+a.y*a.y)/3};
    def.collision_sound=prop_sound(type);def.transform=matrix;prop=int(g.props.size());g.props.push_back(def);
    NativePropPlacement native;native.name=name;native.type=type;native.model_name=name;native.native_index=i;native.transform=matrix;native.center=def.center;native.half=def.half;g.native_props.push_back(std::move(native));
   }
  }
  if(props_only&&prop<0)continue;
  for(auto m:*model){
   m.name=name;m.transform=matrix;m.prop_index=prop;m.captured=prop>=0&&!m.billboard;m.solid=prop<0&&!g.materials[m.material].alpha&&!m.billboard;
   if(m.billboard){
    const float sx=std::max(length(direction(matrix,{1,0,0})),length(direction(matrix,{0,0,1})));
    const float sy=length(direction(matrix,{0,1,0}));
    for(auto&v:m.vertices){auto c=point(matrix,v.position-v.normal);v.normal={v.normal.x*sx,v.normal.y*sy,0};v.position=c+v.normal;}
    m.transform=identity();sprite_bounds(m);++g.native_tree_batches;
   }else bounds(m);
   g.triangles+=m.indices.size()/3;g.meshes.push_back(std::move(m));
  }
 }
 if(f.at!=raw.size()||(!props_only&&g.meshes.empty()))f.fail("incomplete scene");
 if(!props_only)load_collision(r,stage+"/geometry/stage1.cdb",g);
 return g;
}
Mat alpha_track_spawn(const Resources&r,const std::string& stage,const Geometry&g){
 auto spline=r.text(stage+"/data/splines.ai");auto begin=spline.find("AIRaceLine");if(begin==std::string::npos)throw std::runtime_error("Missing alpha raceline");
 auto line=spline.substr(begin);std::regex row(R"(\[\s*[12]\s*\]\s*=\s*\{\s*([-+.0-9]+)\s*,\s*([-+.0-9]+)\s*,\s*([-+.0-9]+))");
 std::sregex_iterator i(line.begin(),line.end(),row),end;
 auto next=[&](){if(i==end)throw std::runtime_error("Incomplete alpha raceline");Vec v{std::stof((*i)[1].str()),std::stof((*i)[2].str()),std::stof((*i)[3].str())};++i;return v;};
 auto p=next(),n=next();auto direction=unit(Vec{n.x-p.x,0,n.z-p.z},{0,0,1});
 float top=-1e20f;
 for(const auto&m:g.collision_meshes){
  if(g.materials[m.material].name=="water")continue;
  for(size_t t=0;t+2<m.indices.size();t+=3){
   auto a=m.vertices[m.indices[t]].position,b=m.vertices[m.indices[t+1]].position,c=m.vertices[m.indices[t+2]].position;
   auto v=b-a,w=c-a,d=p-a;float det=v.x*w.z-v.z*w.x;if(std::abs(det)<.00001f)continue;
   float u=(d.x*w.z-d.z*w.x)/det,q=(v.x*d.z-v.z*d.x)/det;
   if(u>=-.0001f&&q>=-.0001f&&u+q<=1.0001f)top=std::max(top,a.y+v.y*u+w.y*q);
  }
 }
 if(top< -1e19f)throw std::runtime_error("No alpha track collision beneath starting point");
 p.y=top+.65f;return multiply(translate(p),rotate_y(std::atan2(direction.x,direction.z)));
}
}
