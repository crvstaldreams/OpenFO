#include "wheel_effects.hpp"
#include "water.hpp"
#include <sstream>
namespace ofo {
namespace {
float ramp(float v,const std::array<float,3>&a){return std::clamp((std::abs(v)-a[0])/std::max(.01f,a[1]-a[0]),0.f,1.f)*a[2];}
template<size_t N>void read_range(const Json&j,const char*k,std::array<float,N>&out){if(!j.contains(k))return;for(size_t i=0;i<N;i++)out[i]=j.at(k).at(std::to_string(i+1)).get<float>();}
bool finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
WheelSurface native_surface(const Json&j){WheelSurface s;s.label=j.value("Name",std::string());s.skid_type=j.value("SkidMarks",0);if(j.contains("RoadEffect1")){auto e=j.at("RoadEffect1");if(e.at("1")==1)s.dirt_type=e.at("2").get<int>();else if(e.at("1")==4)s.dirt_type=0;}s.water=lower(s.label)=="water";return s;}
}
WheelEffectConfig load_wheel_effect_config(const Resources&r,const Geometry&g,const std::string&stage){
 WheelEffectConfig out;auto source=g.source.empty()?stage+"/track_geom.w32":g.source;
 out.particle_texture=r.texture(source,"particles.dds");out.skid_texture=r.texture(source,"skidmark1.dds");
 if(r.exists("particle/dirtsystem_default.ini")){auto j=bed_file(r,"particle/dirtsystem_default.ini").at("DirtParticleSystem");for(int i=1;i<=4;i++){auto&a=j.at(std::to_string(i));auto&p=out.dirt[i];read_range(a,"LifeTime",p.life);read_range(a,"VelAddY",p.up);read_range(a,"EmitRateLon",p.longitudinal);read_range(a,"EmitRateLat",p.lateral);read_range(a,"EmitRateLin",p.linear);p.probability=a.value("Propability",1.f);auto&u=a.at("Units").at("1");read_range(u,"Size",p.size);read_range(u,"Grow",p.grow);}out.loaded=true;}
 if(r.exists("particle/effectparams.txt")){std::istringstream input(r.text("particle/effectparams.txt"));std::string line;while(std::getline(input,line)){std::istringstream row(line);std::string type,name;row>>type>>name;std::array<float,2>*range=nullptr;if(name=="WaterEmitSpeed")range=&out.water_speed;else if(name=="WaterLife")range=&out.water_life;else if(name=="WaterSize")range=&out.water_size;else if(name=="WaterGrow")range=&out.water_grow;else if(name=="WaterSplashVelY")range=&out.water_up;if(range)row>>(*range)[0]>>(*range)[1];}}
 if(r.exists("global/dynamics/surfaces.bed")){auto root=bed_file(r,"global/dynamics/surfaces.bed");auto j=root.at("Surfaces");for(auto it=j.begin();it!=j.end();++it){auto s=native_surface(it.value());auto audio=it.value().value("SurfaceSound",Json(0));if(audio.is_number_integer()||audio.is_number_float())s.sound_id=audio.get<int>();else if(audio.is_object()&&audio.contains("$reference")){auto key=audio["$reference"].get<std::string>();if(root.contains(key)&&root[key].is_number())s.sound_id=root[key].get<int>();}out.surfaces[std::stoi(it.key())]=s;}}
 // MATC exposes render names, not the dynamics surface reference. These explicit
 // name/texture adapters are provisional; unknown prop materials emit nothing.
 for(const auto&m:g.materials){std::string n=lower(m.name),tx=lower(fs::path(m.texture[0]).stem().string());int id=0;auto has=[&](const char*s){return n.find(s)!=std::string::npos||tx.find(s)!=std::string::npos;};
 if(water_material(m))id=28;else if(has("tarmac")||has("asphalt"))id=2;else if(has("snow"))id=12;else if(has("ice"))id=10;else if(has("grass"))id=15;else if(has("sand"))id=17;else if(has("mould")||has("mud"))id=19;
 else if(has("rock")||has("stone"))id=18;
 else if(has("concrete"))id=21;
 else if(has("road")||has("gravel"))id=4;else if(has("terrain")||has("ground")||has("forest"))id=16;
 WheelSurface s;if(out.surfaces.count(id))s=out.surfaces.at(id);else if(id){s.skid_type=id==2?3:id==28?0:1;s.dirt_type=id==2||id==28?0:id==15?3:id==17?4:1;s.water=id==28;}
 if(id==12||id==20)s.dirt_type=1; // family atlas supplies the original snow appearance
 out.materials.push_back(s);
 }
 return out;
}
void WheelEffects::clear(){skids_.clear();particles_.clear();connected_.fill(false);credit_.fill(0);previous_={};started_=false;time_=0;serial_=0;}
void WheelEffects::update(const std::array<WheelContact,4>&contacts,double time,uint64_t generation){
 if(!std::isfinite(time)){clear();return;}
 bool discontinuity=started_&&(time<time_||time-time_>.5||generation!=generation_);
 if(started_&&time==time_&&generation==generation_)return;
 for(size_t i=0;i<4;i++)if(started_&&contacts[i].grounded&&previous_[i].grounded&&length(contacts[i].point-previous_[i].point)>8)discontinuity=true;
 if(discontinuity)clear();
 if(!started_){started_=true;time_=time;generation_=generation;previous_=contacts;return;}
 float dt=float(time-time_);time_=time;generation_=generation;
 for(auto&p:particles_){p.age+=dt;p.position+=p.velocity*dt;p.velocity.y-=dt*.5f;}
 particles_.erase(std::remove_if(particles_.begin(),particles_.end(),[](const Particle&p){return p.age>=p.life;}),particles_.end());
 while(!skids_.empty()&&time-skids_.front().born>30)skids_.pop_front();
 for(size_t i=0;i<4;i++){
 const auto&c=contacts[i];WheelSurface surface;
 if(c.surface_id>=0&&config_.surfaces.count(c.surface_id))surface=config_.surfaces.at(c.surface_id);else if(c.material<config_.materials.size())surface=config_.materials[c.material];
 bool active=c.grounded&&c.load>1&&finite(c.point)&&finite(c.normal)&&std::isfinite(c.speed)&&std::isfinite(c.slip)&&std::isfinite(c.lateral_speed);
 float slip=std::max(std::abs(c.slip),std::abs(c.lateral_speed));bool skid=active&&surface.skid_type>0&&slip>1.5f;
 if(skid&&connected_[i]&&c.material==previous_[i].material){float distance=length(c.point-previous_[i].point);if(distance>.025f&&distance<3){skids_.push_back({previous_[i].point,c.point,unit(c.normal),std::clamp(c.width,.05f,.8f),std::clamp(slip/8,.15f,.8f),time,std::clamp(surface.skid_type,1,4)});if(skids_.size()>max_skids)skids_.pop_front();}}
 // Keep mark endpoint until enough motion accumulates; never bridge over air.
 if(!skid||!connected_[i]||length(c.point-previous_[i].point)>.025f)previous_[i]=c;
 connected_[i]=skid;
 if(!active){credit_[i]=0;previous_[i]=c;continue;}
 int type=std::clamp(surface.dirt_type,0,4);const auto&p=config_.dirt[type?type:1];float amount=std::max(ramp(c.slip,p.longitudinal),ramp(c.lateral_speed,p.lateral));amount=std::max(amount,ramp(c.speed,p.linear));
 // RoadEffect2={2,1} supplies rolling gravel: adapt its native linear
 // threshold (10..60 m/s) to a sparse dust wake.
 if(type>0)amount=std::max(amount,std::clamp((std::abs(c.speed)-10)/50,0.f,1.f)*.4f);
 if(!type&&!surface.skid_type&&!surface.water)amount=0;
 if(surface.water)amount=std::clamp((std::abs(c.speed)-config_.water_speed[0])/std::max(.01f,config_.water_speed[1]-config_.water_speed[0]),0.f,1.f);
 credit_[i]+=dt*24*amount*p.probability;int count=std::min(8,int(credit_[i]));credit_[i]-=count;
 for(int k=0;k<count;k++){float random=float((++serial_*37)%101)/100;Particle q;q.serial=serial_;q.position=c.point+unit(c.normal)*.03f;q.life=p.life[0]+random*(p.life[1]-p.life[0]);q.size=p.size[0]+random*(p.size[1]-p.size[0]);q.grow=p.grow[0]+random*(p.grow[1]-p.grow[0]);q.velocity=finite(c.velocity)?c.velocity*.08f:Vec{};q.velocity+=unit(c.normal)*(p.up[0]+random*(p.up[1]-p.up[0]));
 // Original 512² atlas: dust 0,384 128²; white smoke 256,256
 // 128²; splash 384,384 128². Four 64² units per region.
 int x=type?0:256,y=type?384:256;
 if(surface.water){x=384;y=384;q.life=config_.water_life[0]+random*(config_.water_life[1]-config_.water_life[0]);q.size=config_.water_size[0]+random*(config_.water_size[1]-config_.water_size[0]);q.grow=config_.water_grow[0]+random*(config_.water_grow[1]-config_.water_grow[0]);q.velocity+=unit(c.normal)*(config_.water_up[0]+random*(config_.water_up[1]-config_.water_up[0]));}
 q.x=x+int(serial_%2)*64;q.y=y+int((serial_/2)%2)*64;particles_.push_back(q);if(particles_.size()>max_particles)particles_.pop_front();}
 }
}
WheelEffectVertices WheelEffects::vertices(Vec right,Vec up)const{
 WheelEffectVertices out;vertices(right,up,out);return out;
}
void WheelEffects::vertices(Vec right,Vec up,WheelEffectVertices&out,int detail_stride)const{
 out.skids.clear();out.particles.clear();detail_stride=std::clamp(detail_stride,1,4);right=unit(right,{1,0,0});up=unit(up);const size_t skid_limit=(skids_.size()+detail_stride-1)/detail_stride;
 // Grow geometrically while a trail is filling; reserving its exact length each
 // frame would still allocate every frame until the 30-second history is full.
 auto reserve=[](std::vector<Vertex>&v,size_t needed,size_t limit){if(needed>v.capacity())v.reserve(std::min(limit,std::max(needed,v.capacity()*2)));};
 reserve(out.skids,skid_limit*6,max_skids*6);reserve(out.particles,((particles_.size()+detail_stride-1)/detail_stride)*6,max_particles*6);
 auto quad=[](std::vector<Vertex>&v,const std::array<Vec,4>&positions,uint32_t color,float u0,float v0,float u1,float v1,Vec normal){Vertex q[4];for(int i=0;i<4;i++){q[i].position=positions[i];q[i].normal=normal;q[i].color=color;q[i].u=i==0||i==3?u0:u1;q[i].v=i<2?v0:v1;}for(int i:{0,1,2,0,2,3})v.push_back(q[i]);};
 for(auto it=skids_.end()-skid_limit;it!=skids_.end();++it){const auto&s=*it;Vec side=unit(cross(s.n,s.b-s.a),{1,0,0})*(s.width*.5f),offset=s.n*.012f;float alpha=s.alpha*float(std::clamp((30-(time_-s.born))/5,0.,1.));quad(out.skids,{s.a-side+offset,s.a+side+offset,s.b+side+offset,s.b-side+offset},(uint32_t(alpha*255)<<24)|0xffffff,((s.type-1)*64+.5f)/256,.5f/64,(s.type*64-.5f)/256,63.5f/64,s.n);}
 for(const auto&p:particles_){if((p.serial-1)%detail_stride)continue;float radius=std::max(.01f,p.size+p.age*p.grow),alpha=.5f*(1-p.age/p.life);Vec r=right*radius,u=up*radius;quad(out.particles,{p.position-r+u,p.position+r+u,p.position+r-u,p.position-r-u},(uint32_t(alpha*255)<<24)|0xffffff,(p.x+.5f)/512,(p.y+.5f)/512,(p.x+64-.5f)/512,(p.y+64-.5f)/512,{0,1,0});}
}
}
