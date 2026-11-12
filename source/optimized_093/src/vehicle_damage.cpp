#include "vehicle_damage.hpp"
#include "alpha_car.hpp"
#include <cstring>
#include <set>
#include <stdexcept>
namespace ofo { namespace {
struct Reader {
 const Bytes& b; size_t p=0;
 void need(size_t n)const {if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated crash.dat");}
 uint32_t u(uint32_t max){need(4);uint32_t v=uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;p+=4;if(v>max)throw std::runtime_error("crash.dat count limit");return v;}
 std::string name(){size_t start=p;while(p<b.size()&&b[p]&&p-start<256)++p;if(p==b.size()||p-start==256)throw std::runtime_error("Invalid crash.dat name");return std::string(b.begin()+start,b.begin()+p++);}
 float f(size_t at)const{if(at>b.size()||b.size()-at<4)throw std::runtime_error("Truncated crash.dat float");uint32_t bits=uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;float v;std::memcpy(&v,&bits,4);if(!std::isfinite(v)||std::abs(v)>1e8f)throw std::runtime_error("Invalid crash.dat float");return v;}
 Vec vec(size_t at)const{return{f(at),f(at+4),f(at+8)};}
};
bool same(Vec a,Vec b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
float safe(float v,float fallback=0){return std::isfinite(v)?v:fallback;}
float value(const Json&j,const char*k,int i,float fallback){auto a=j.find(k);if(a==j.end())return fallback;if(a->is_number())return safe(a->get<float>(),fallback);if(a->is_array()&&size_t(i)<a->size()&&(*a)[i].is_number())return safe((*a)[i].get<float>(),fallback);if(a->is_object()){auto b=a->find(std::to_string(i+1));if(b!=a->end()&&b->is_number())return safe(b->get<float>(),fallback);}return fallback;}
float box_distance(Vec p,Vec lo,Vec hi){float d=0;for(int k=0;k<3;k++){float q=std::max({lo[k]-p[k],0.f,p[k]-hi[k]});d+=q*q;}return std::sqrt(d);}
void bounds(Mesh&m){if(m.vertices.empty())return;Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};for(auto&v:m.vertices){auto p=point(m.transform,v.position);for(int k=0;k<3;k++){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}}m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;}
std::string folder(const Geometry&g){return fs::u8path(g.source).parent_path().generic_u8string();}
CrashProfile crash_profile(const Json& panel,bool prototype){
 CrashProfile out;
 out.onset=value(panel,"CrashMomentum",0,prototype?1000.f:1.f);
 out.full=std::max(out.onset+1,value(panel,"CrashMomentum",1,prototype?40000.f:25.f));
 out.nominal=std::max(.1f,value(panel,"CrashNominal",0,prototype?2100.f:3.5f));
 out.radius_min=std::clamp(value(panel,"CrashVolume",0,.5f),.05f,4.f);
 out.radius_max=std::clamp(value(panel,"CrashVolume",1,.6f),out.radius_min,4.f);
 out.radius_at=std::max(1.f,value(panel,"CrashVolume",2,prototype?out.full:25.f));
 return out;
}
float impact_radius(const CrashProfile& p,float strength){
 return p.radius_min+(p.radius_max-p.radius_min)*std::clamp(strength/p.radius_at,0.f,1.f);
}
CrashTargets load_alpha_crash(const Resources& r,const Geometry& g){
 const auto dir=folder(g);
 auto crash=load_alpha_car_geometry(r,dir+"/model.crash");
 const auto panels=r.exists(dir+"/panels.ini")?bed_file(r,dir+"/panels.ini"):Json::object();
 CrashTargets out;
 std::map<std::string,std::vector<size_t>> by_name;
 for(size_t i=0;i<g.meshes.size();++i)by_name[g.meshes[i].name].push_back(i);
 std::set<std::string> records;
 std::set<size_t> bound;
 for(const auto& target:crash.meshes){
  const auto name=target.name.substr(0,target.name.size()-6);
  const auto originals=by_name.find(name);
  if(originals==by_name.end())continue;
  for(size_t index:originals->second){
   const auto& clean=g.meshes[index];
   if(bound.count(index)||clean.material!=target.material||clean.indices!=target.indices||clean.vertices.size()!=target.vertices.size())continue;
   if(!std::equal(clean.vertices.begin(),clean.vertices.end(),target.vertices.begin(),
    [](const Vertex& a,const Vertex& b){return a.u==b.u&&a.v==b.v;}))continue;
   CrashTarget shape;
   shape.mesh=index;
   shape.prototype_profile=crash_profile(panels.value(name,panels.value("body",Json::object())),true);
   shape.positions.reserve(target.vertices.size());
   shape.normals.reserve(target.vertices.size());
   // CARC instances are rigid transforms. Some damaged frames have their own
   // translation, so match in chassis space and store in the clean bind frame.
   const Vec x=direction(clean.transform,{1,0,0}),y=direction(clean.transform,{0,1,0}),z=direction(clean.transform,{0,0,1});
   auto local=[&](Vec v){return Vec{dot(x,v),dot(y,v),dot(z,v)};};
   const auto origin=point(clean.transform,{});
   for(size_t j=0;j<target.vertices.size();++j){
    const auto& v=target.vertices[j];
    const auto p=local(point(target.transform,v.position)-origin);
    shape.moved+=length(p-clean.vertices[j].position)>.00001f;
    shape.positions.push_back(p);
    shape.normals.push_back(unit(local(direction(target.transform,v.normal)),clean.vertices[j].normal));
   }
   out.vertices+=shape.positions.size();
   out.moved+=shape.moved;
   out.surfaces.push_back(std::move(shape));
   records.insert(name);
   bound.insert(index);
   break;
  }
 }
 out.records=records.size();
 return out;
}

}
CrashTargets parse_vehicle_crash(const Bytes&bytes,const Geometry&g){
 Reader r{bytes};CrashTargets out;auto records=r.u(4096);std::set<std::string>seen;
 for(uint32_t record=0;record<records;record++){
  auto authored=r.name();if(authored.size()<7||authored.substr(authored.size()-6)!="_crash")throw std::runtime_error("Invalid crash.dat suffix");auto name=authored.substr(0,authored.size()-6);if(!seen.insert(name).second)throw std::runtime_error("Duplicate crash.dat record");
  std::vector<size_t> meshes;for(size_t i=0;i<g.meshes.size();i++)if(g.meshes[i].name==name)meshes.push_back(i);
  if(meshes.empty())throw std::runtime_error("Missing crash.dat source instance: "+name);
  auto count=r.u(4096);if(!count)continue;if(count!=meshes.size())throw std::runtime_error("Crash surface count mismatch: "+name);
  ++out.records;
  for(size_t i=0;i<count;i++){
   auto n=r.u(2000000),size=r.u(128000000);auto&m=g.meshes[meshes[i]];
   if(!n||n>2000000-out.vertices||m.vertices.size()!=n||size%n||(size/n!=32&&size/n!=36))throw std::runtime_error("Crash vertex declaration mismatch");
   auto stride=size/n;r.need(size);size_t bind=r.p;r.p+=size;r.need(size_t(n)*48);CrashTarget t;t.mesh=meshes[i];t.positions.reserve(n);t.normals.reserve(n);
   for(auto index:m.indices)if(index>=n)throw std::runtime_error("Crash source index out of range");
   for(size_t v=0;v<n;v++){
    auto p=r.vec(r.p+v*48),target=r.vec(r.p+v*48+12),normal=r.vec(r.p+v*48+24),target_normal=r.vec(r.p+v*48+36);
    // Crash bind normals are exported with single-precision rounding differences
    // from the BGM normals (observed below 1e-6); vertex identity stays exact.
    if(!same(p,r.vec(bind+v*stride))||!same(p,m.vertices[v].position)||length(normal-r.vec(bind+v*stride+12))>1e-5f||length(normal-m.vertices[v].normal)>1e-5f)throw std::runtime_error("Crash bind position/normal mismatch: "+name);

    // Both supported declarations end with their original UV pair. Preserve all
    // other vertex data by updating positions/normals in the decoded Mesh only.
    if(r.f(bind+v*stride+stride-8)!=m.vertices[v].u||r.f(bind+v*stride+stride-4)!=m.vertices[v].v)throw std::runtime_error("Crash bind UV mismatch");
    t.moved+=!same(p,target);t.positions.push_back(target);t.normals.push_back(target_normal);
   }
   r.p+=size_t(n)*48;out.vertices+=n;out.moved+=t.moved;out.surfaces.push_back(std::move(t));
  }
 }
 if(r.p!=bytes.size())throw std::runtime_error("Trailing crash.dat data");return out;
}
CrashTargets load_vehicle_crash(const Resources&r,const Geometry&g){if(alpha_car_resource(g.source))return load_alpha_crash(r,g);auto path=folder(g)+"/crash.dat";return r.exists(path)?parse_vehicle_crash(r.read(path),g):CrashTargets{};}
int vehicle_window_pane(const std::string&raw){auto n=lower(raw);if(n.find("window")==n.npos&&n.find("glass")==n.npos)return -1;const char*names[]={"front","rear","left","right"};for(int i=0;i<4;i++)if(n.find(names[i])!=n.npos)return i;return -1;}
std::vector<VehiclePanelDefinition> vehicle_panel_definitions(const Geometry&g,const Json&panels){
 std::vector<VehiclePanelDefinition> out;if(!panels.is_object())return out;
 auto owner=[&](std::string n){std::set<std::string>seen;while(panels.contains(n)&&seen.insert(n).second){auto p=panels[n].value("Parent",std::string());if(p.empty()||!panels.contains(p))break;n=p;}return n;};
 std::map<std::string,std::vector<size_t>>groups;std::set<std::string> counted;
 for(size_t i=0;i<g.meshes.size();i++){auto&name=g.meshes[i].name;if(name!="body"&&panels.contains(name))groups[owner(name)].push_back(i);}
 for(auto&entry:groups){VehiclePanelDefinition d;d.name=entry.first;d.meshes=entry.second;d.lo={1e30f,1e30f,1e30f};d.hi={-1e30f,-1e30f,-1e30f};std::set<std::string>names;
  for(auto i:d.meshes){auto&m=g.meshes[i];if(names.insert(m.name).second)d.mass+=std::max(.1f,value(panels[m.name],"Mass",0,10));for(auto&v:m.vertices){auto p=point(m.transform,v.position);for(int k=0;k<3;k++){d.lo[k]=std::min(d.lo[k],p[k]);d.hi[k]=std::max(d.hi[k],p[k]);}}}
  if(d.lo.x>d.hi.x)continue;d.center=(d.lo+d.hi)*.5f;d.pivot=d.center;bool door=d.name=="door_l"||d.name=="door_r";d.axis=door?Vec{0,1,0}:Vec{1,0,0};
  for(auto suffix:{"_joint","_attach_1","_attach"}){auto it=g.markers.find(lower(d.name+suffix));if(it!=g.markers.end()){d.marker=it->first;d.pivot=point(it->second,{});int column=door?4:0;d.axis=unit({it->second[column],it->second[column+1],it->second[column+2]},d.axis);break;}}
  d.hinge=!d.marker.empty()&&(door||d.name=="hood"||d.name=="trunk");auto&p=panels[d.name];float angle=std::clamp(value(p,"MaxAngle",0,door?75:45),0.f,180.f)*3.14159265f/180;
  if(d.hinge){auto motion=cross(d.axis,d.center-d.pivot);float outward=door?motion.x*(d.center.x<0?-1.f:1.f):motion.y;d.lower_limit=outward<0?-angle:0;d.upper_limit=outward<0?0:angle;}
  if(!d.hinge&&p.contains("Joints")&&p["Joints"].is_object()){
   for(const auto&joint:p["Joints"]){
    if(!joint.is_object())continue;
    const auto marker=g.markers.find(lower(joint.value("Position",std::string())));
    if(marker==g.markers.end()||!joint.contains("MinAngle")||!joint.contains("MaxAngle"))continue;
    constexpr float radians=3.14159265f/180.f;
    d.joint_min=vector3(joint.at("MinAngle"))*radians;
    d.joint_max=vector3(joint.at("MaxAngle"))*radians;
    if(d.joint_min.x>d.joint_max.x||d.joint_min.y>d.joint_max.y||d.joint_min.z>d.joint_max.z)continue;
    if(joint.contains("RestAngle"))d.joint_rest=vector3(joint.at("RestAngle"))*radians;
    if(joint.contains("Stiffness"))d.joint_stiffness=vector3(joint.at("Stiffness"));
    for(int axis=0;axis<3;++axis)
     d.joint_rest[axis]=std::clamp(d.joint_rest[axis],d.joint_min[axis],d.joint_max[axis]);
    d.marker=marker->first;d.joint_frame=marker->second;d.pivot=point(d.joint_frame,{});
    d.hinge=d.has_joint_limits=true;
    break;
   }
  }
  d.prototype_momentum=alpha_car_resource(g.source);
  d.break_impulse=std::max(1.f,value(p,"ReleaseMomentum",0,200));
  const float unlock_fallback=d.prototype_momentum?value(p,"UnlockMomentumRange",0,value(p,"ReleaseMomentumRange",0,d.break_impulse)):50.f;
  d.unlock_impulse=std::max(1.f,value(p,"UnlockMomentum",0,unlock_fallback));
  d.crash=crash_profile(p,d.prototype_momentum);
  out.push_back(std::move(d));
 }return out;
}
std::vector<VehiclePanelDefinition> load_vehicle_panels(const Resources&r,const Geometry&g){auto path=folder(g)+"/panels.ini";return r.exists(path)?vehicle_panel_definitions(g,bed_file(r,path)):std::vector<VehiclePanelDefinition>{};}
void tick_vehicle_panel(VehiclePanelDamage&s,const VehiclePanelDefinition&d,float dt){dt=std::clamp(safe(dt),0.f,1.f);s.cooldown=std::max(0.f,s.cooldown-dt);if(s.unlocked)s.unlock_age+=dt;if(!s.detached&&(!d.hinge||s.unlock_age>=.08f||d.unlock_impulse>=d.break_impulse)&&s.attachment_momentum>=d.break_impulse)s.detached=true;}
void hit_vehicle_panel(VehiclePanelDamage&s,const VehiclePanelDefinition&d,const VehicleDamageContact&c,float mass,bool direct,float durability){
 if(s.cooldown>0||!(c.impulse>0)||!(c.closing_speed>0))return;
 float radius=impact_radius(d.crash,d.prototype_momentum?c.impulse:c.closing_speed),distance=box_distance(c.position,d.lo,d.hi);if(!direct&&distance>=radius)return;
 float falloff=direct?1:std::pow(1-distance/radius,2),delta=std::min(c.closing_speed,c.impulse/std::max(direct?d.mass:mass,1.f));
 const float onset=d.prototype_momentum?d.crash.onset:std::max(2.5f,d.crash.onset*.65f);
 const float full=d.prototype_momentum?std::max(onset+1,d.crash.full):std::max(onset+2,d.crash.full*.65f);
 const float shock=d.prototype_momentum?c.impulse:std::max(delta,c.closing_speed*.3f);
 float amount=std::clamp((shock-onset)/(full-onset),0.f,1.f)*falloff*std::clamp((d.prototype_momentum?2100.f:3.5f)/d.crash.nominal,.3f,1.4f)*(.5f/std::clamp(durability,.05f,20.f));
 if(!(amount>0))return;s.damage=std::min(1.f,s.damage+amount);s.cooldown=.12f;if(s.detached)return;s.attachment_momentum+=(direct||d.prototype_momentum?c.impulse:c.impulse*d.mass/std::max(mass,1.f))*falloff;if(d.hinge&&s.attachment_momentum>=d.unlock_impulse)s.unlocked=true;
}
Mat vehicle_panel_hinge(const VehiclePanelDefinition&d,float angle){angle=std::clamp(safe(angle),d.lower_limit,d.upper_limit);auto a=unit(d.axis);float c=std::cos(angle),s=std::sin(angle),t=1-c;Mat r={t*a.x*a.x+c,t*a.x*a.y+s*a.z,t*a.x*a.z-s*a.y,0,t*a.x*a.y-s*a.z,t*a.y*a.y+c,t*a.y*a.z+s*a.x,0,t*a.x*a.z+s*a.y,t*a.y*a.z-s*a.x,t*a.z*a.z+c,0,0,0,0,1};return multiply(translate(d.pivot),multiply(r,translate(d.pivot*-1)));}
Geometry vehicle_damage_geometry(const Geometry&g,const CrashTargets&targets,float damage,const std::vector<VehicleDamageContact>&contacts,const Resources*r,const std::array<float,4>&glass,bool removed){
 Geometry out=g;float amount=std::clamp(safe(damage),0.f,1.f),blend=std::min(1.f,amount*1.7f);
 for(auto&t:targets.surfaces){if(t.mesh>=out.meshes.size())continue;auto&m=out.meshes[t.mesh];if(t.positions.size()!=m.vertices.size()||t.normals.size()!=m.vertices.size())continue;
  for(size_t v=0;v<m.vertices.size();v++){
   auto& q=m.vertices[v];
   float factor=contacts.empty()?blend:0.f;
   const auto root=point(m.transform,q.position);
   for(const auto& contact:contacts){
    if(!(contact.radius>0)||!std::isfinite(contact.radius)||!std::isfinite(contact.depth))continue;
    float radius=contact.radius,amplitude=blend;
    if(t.prototype_profile){
     const auto& p=*t.prototype_profile;
     radius=impact_radius(p,contact.impulse);
     amplitude=std::clamp((contact.impulse-p.onset)/(p.full-p.onset)*2100.f/p.nominal,0.f,1.f);
    }
    const auto delta=root-contact.position;
    const float dist=dot(delta,delta)/(radius*radius);
    if(dist<1)factor=std::max(factor,amplitude*(1-dist)*(1-dist));
   }
   if(factor<=0)continue;
   q.position=q.position+(t.positions[v]-q.position)*factor;
   q.normal=unit(q.normal+(t.normals[v]-q.normal)*factor,q.normal);
  }bounds(m);
 }
 for(auto&mat:out.materials){int pane=vehicle_window_pane(mat.name);if(pane>=0&&glass[size_t(pane)]>.02f&&r&&r->exists("cars/shared/windows_damaged.dds"))mat.texture[0]="cars/shared/windows_damaged.dds";
  else if(amount>=.25f&&lower(mat.name)=="body"&&r){auto stem=fs::u8path(mat.texture[0]).stem().generic_u8string();if(stem.rfind("skin",0)!=0||stem.find("_damaged")!=stem.npos)stem="skin1";auto path=folder(g)+"/"+stem+"_damaged.dds";if(r->exists(path))mat.texture[0]=path;}
 }
 if(removed)for(auto&m:out.meshes)if(m.material<out.materials.size()&&vehicle_window_pane(out.materials[m.material].name)==0)m.indices.clear();return out;
}
std::array<float,4> vehicle_glass_damage(const Geometry&g,const std::vector<VehicleDamageContact>&contacts,float mass){
 std::array<float,4>result{};std::array<Vec,4>lo,hi;std::array<bool,4>present{};
 for(auto&m:g.meshes){if(m.material>=g.materials.size())continue;int pane=vehicle_window_pane(g.materials[m.material].name);if(pane<0)continue;for(auto&v:m.vertices){auto p=point(m.transform,v.position);if(!present[pane]){lo[pane]=hi[pane]=p;present[pane]=true;}else for(int k=0;k<3;k++){lo[pane][k]=std::min(lo[pane][k],p[k]);hi[pane][k]=std::max(hi[pane][k],p[k]);}}}
 for(auto&c:contacts){float delta=std::min(c.closing_speed>0?c.closing_speed:100.f,c.impulse/std::max(mass,1.f));for(int pane=0;pane<4;pane++){if(!present[pane])continue;float distance=box_distance(c.position,lo[pane],hi[pane]);bool oriented=pane==0?c.normal.z<-.35f:pane==1?c.normal.z>.35f:pane==2?c.normal.x>.35f:c.normal.x<-.35f;bool transmitted=pane==0&&c.normal.z<-.55f&&delta>5&&c.position.z>=lo[pane].z;if(!oriented||(!transmitted&&distance>.65f))continue;float severity=std::clamp((delta-3.5f)/12,0.f,1.f);if(transmitted)severity=std::max(severity,std::clamp((delta-5)/18,0.f,1.f));result[pane]=std::min(1.f,result[pane]+severity*(transmitted?.65f:std::max(.2f,1-distance/.8f)));}}
 return result;
}
}
