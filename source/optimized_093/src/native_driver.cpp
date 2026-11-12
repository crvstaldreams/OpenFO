#include "native_driver.hpp"
#include "alpha_car.hpp"
#include "driver_seat_fit.hpp"
#include "menu_steering.hpp"
#include <cstring>
#include <stdexcept>
#include <regex>
namespace ofo {namespace {
struct Reader {const Bytes&b;size_t p=0;void need(size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated native driver data");}template<class T>T get(){need(sizeof(T));T x;std::memcpy(&x,b.data()+p,sizeof x);p+=sizeof x;return x;}uint32_t u(){return get<uint32_t>();}uint32_t count(uint32_t max=100000){auto n=u();if(n>max)throw std::runtime_error("Excessive native driver count");return n;}float f(){float x=get<float>();if(!std::isfinite(x))throw std::runtime_error("Nonfinite native driver data");return x;}Vec vec(){return{f(),f(),f()};}Mat mat(){Mat m;for(auto&x:m)x=f();return m;}void skip(size_t n){need(n);p+=n;}std::string str(){size_t q=p;while(p<b.size()&&b[p]&&p-q<4096)++p;if(p==b.size()||p-q==4096)throw std::runtime_error("Invalid native driver string");std::string s((char*)b.data()+q,p-q);++p;return s;}};
Mat quaternion(std::array<float,4>q){float n=0;for(auto x:q)n+=x*x;if(n<1e-12f)throw std::runtime_error("Invalid driver quaternion");for(auto&x:q)x/=std::sqrt(n);auto[x,y,z,w]=q;auto m=identity();m[0]=1-2*(y*y+z*z);m[1]=2*(x*y+z*w);m[2]=2*(x*z-y*w);m[4]=2*(x*y-z*w);m[5]=1-2*(x*x+z*z);m[6]=2*(y*z+x*w);m[8]=2*(x*z+y*w);m[9]=2*(y*z-x*w);m[10]=1-2*(x*x+y*y);return m;}
std::array<float,4> quat(const Json&j){return{j.at("1"),j.at("2"),j.at("3"),j.at("4")};}
Mat align(Vec a,Vec b){a=unit(a);b=unit(b);auto axis=cross(a,b);float w=1+dot(a,b);if(w<1e-6f){axis=unit(cross(a,std::abs(a.x)<.9f?Vec{1,0,0}:Vec{0,1,0}));w=0;}return quaternion({axis.x,axis.y,axis.z,w});}
Mat limb(const Mat&m,Vec a,Vec b,Vec c,Vec d){return multiply(translate(c),multiply(align(b-a,d-c),multiply(translate(a*-1),m)));}
Mat axes(Vec x,Vec y,Vec z){
 return {x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,0,0,0,1};
}
Mat hand_frame(Vec along,Vec palm){
 return axes(along,unit(cross(palm,along)),palm);
}
Mat rotation_between(const Mat& from,const Mat& to){
 Mat inverse=identity();
 for(int x=0;x<3;++x)for(int y=0;y<3;++y)inverse[4*x+y]=from[4*y+x];
 return multiply(to,inverse);
}
struct GripTarget {Vec contact,wrist,along,palm;};
GripTarget wheel_grip(const DriverPlacement& p,int hand,float angle){
 Vec local=p.grip[hand];
 const float x=local.x,y=local.y;
 local.x=x*std::cos(angle)-y*std::sin(angle);
 local.y=x*std::sin(angle)+y*std::cos(angle);
 const Vec contact=point(p.steering,local);
 const Vec outward=unit(direction(p.steering,{local.x,local.y,0}));
 const Vec forward=unit(direction(p.steering,{0,0,1}));
 return {contact,contact+outward*.052f-forward*.031f,outward*-1.f,forward};
}
void pose_grip(const NativeDriver& d,const DriverPlacement& p,float angle,DriverPose& out){
 out.hands_locked=true;
 for(int hand=0;hand<2;++hand){
  const auto& bind=d.hands[hand];
  if(!bind.valid){out.hands_locked=false;continue;}
  const size_t upper=hand?9:7,lower=upper+1;
  const Vec shoulder=point(out.skin[upper],d.segments[upper].joint);
  const Vec elbow=point(out.skin[lower],d.segments[lower].joint);
  const Vec wrist=point(out.skin[lower],bind.wrist);
  const auto target=wheel_grip(p,hand,angle);
  const auto delta=target.wrist-shoulder;
  const float distance=length(delta),a=length(elbow-shoulder),b=length(wrist-elbow);
  if(std::min({distance,a,b})<1e-5f){out.hands_locked=false;continue;}
  const auto direction_to_wrist=delta/distance;
  const float reach=std::clamp(distance,std::abs(a-b)+1e-5f,a+b-1e-5f);
  Vec pole{hand?.20f:-.20f,-1,0};
  pole=pole-direction_to_wrist*dot(pole,direction_to_wrist);
  pole=unit(pole,{0,0,-1});
  const float along=(a*a-b*b+reach*reach)/(2*reach);
  const Vec next=shoulder+direction_to_wrist*along+pole*std::sqrt(std::max(0.f,a*a-along*along));
  const Vec end=shoulder+direction_to_wrist*reach;
  out.skin[upper]=limb(out.skin[upper],shoulder,elbow,shoulder,next);
  out.skin[lower]=limb(out.skin[lower],elbow,wrist,next,end);
  const auto orient=rotation_between(hand_frame(bind.along,bind.palm),hand_frame(target.along,target.palm));
  out.hand_skin[hand]=multiply(translate(end),multiply(orient,translate(bind.wrist*-1.f)));
  out.posed_hands[hand]=true;
  out.hand_target[hand]=target.contact;
  out.wrist_target[hand]=end;
  out.hands_locked=out.hands_locked&&std::abs(reach-distance)<.001f;
 }
}
void skin_hand(const DriverHandBind& bind,const Mat& hand,const Vertex& vertex,Vec& position,Vec& normal){
 const auto local=vertex.position-bind.wrist;
 const float reach=dot(local,bind.along);
 float blend=std::clamp((reach+.016f)/.04f,0.f,1.f);
 if(blend<=0)return;
 blend=blend*blend*(3-2*blend);
 const auto knuckle=bind.wrist+bind.along*.045f+bind.palm*.011f;
 const auto axis=unit(cross(bind.along,bind.palm));
 // The native hand is already partly bent. Add flex past the knuckles so
 // the fingertips follow the far side of the rim and the thumb stays opposed.
 const float curl=.78f*std::clamp((reach-.047f)/.067f,0.f,1.f);
 const float cosine=std::cos(curl),sine=std::sin(curl);
 auto rotate=[&](Vec v){
  return v*cosine+cross(axis,v)*sine+axis*(dot(axis,v)*(1-cosine));
 };
 position=position*(1-blend)+point(hand,knuckle+rotate(vertex.position-knuckle))*blend;
 normal=normal*(1-blend)+direction(hand,rotate(vertex.normal))*blend;
}
void bounds(Mesh&m){Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};for(auto&v:m.vertices)for(int k=0;k<3;k++){lo[k]=std::min(lo[k],v.position[k]);hi[k]=std::max(hi[k],v.position[k]);}m.center=(lo+hi)*.5f;m.radius=length(hi-lo)*.5f;}
constexpr const char* johnny_mesh="alpha_content/driver/johnny.ofad";
constexpr const char* johnny_atlas="alpha_content/driver/johnny_b.dds";
void replace_with_johnny(const Resources&r,NativeDriver&d){
 const auto bytes=r.read(johnny_mesh);Reader f{bytes};
 if(f.u()!=0x4441464fu||f.u()!=1)throw std::runtime_error("Unsupported Alpha driver mesh");
 auto nv=f.count(100000),ni=f.count(600000);
 if(!nv||!ni||ni%3||bytes.size()!=16+size_t(nv)*40+size_t(ni)*2)
  throw std::runtime_error("Invalid Alpha driver mesh length");
 Geometry g;g.source=johnny_mesh;
 Material material;material.name="Johnny_B";material.shader=0xfffffffeu;material.texture[0]=johnny_atlas;g.materials.push_back(material);
 Mesh m;m.name="Johnny_body";m.vertices.reserve(nv);m.indices.reserve(ni);
 std::vector<DriverSkinWeight> weights;weights.reserve(nv);size_t blended=0;
 for(uint32_t i=0;i<nv;++i){
  Vertex v;v.position=f.vec();v.normal=unit(f.vec());v.u=v.lu=f.f();v.v=v.lv=f.f();
  DriverSkinWeight w;int sum=0,active=0;
  for(auto&b:w.bone){b=f.get<uint8_t>();if(b>=driver_bone_count)throw std::runtime_error("Invalid Alpha driver joint");}
  for(auto&q:w.weight){q=f.get<uint8_t>();sum+=q;active+=q!=0;}
  if(sum!=255)throw std::runtime_error("Invalid Alpha driver weights");
  blended+=active>1;m.vertices.push_back(v);weights.push_back(w);
 }
 for(uint32_t i=0;i<ni;++i){auto v=f.get<uint16_t>();if(v>=nv)throw std::runtime_error("Invalid Alpha driver index");m.indices.push_back(v);}
 bounds(m);g.triangles=ni/3;g.meshes.push_back(std::move(m));d.geometry=std::move(g);
 d.weights.clear();d.weights.push_back(std::move(weights));d.blended_vertices=blended;
}
struct Stream {uint32_t type,count,stride,flags;size_t offset;};
}
NativeDriver load_native_driver(const Resources&r,bool female){return load_native_driver(r,female?DriverModel::Female:DriverModel::Male);}
NativeDriver load_native_driver(const Resources&r,DriverModel model){NativeDriver d;std::string gender=model==DriverModel::Female?"female":"male",path="drivers/ragdoll/"+gender+".bgm";d.geometry.source=path;auto bytes=r.read(path);Reader f{bytes};if(f.u()!=0x10002)throw std::runtime_error("Unsupported native driver BGM version");for(auto n=f.count();n--;){if(f.u()!=0x4354414d)throw std::runtime_error("Missing driver MATC");Material m;m.name=f.str();m.alpha=f.u();f.skip(8);m.shader=f.u();f.skip(100);for(auto&t:m.texture)t=f.str();d.geometry.materials.push_back(m);}
 std::vector<Stream>streams;for(auto n=f.count();n--;){Stream s{};s.type=f.u();if(f.u()!=0)throw std::runtime_error("Compressed driver stream");s.count=f.count(1000000);s.stride=s.type==2?2:f.count(1024);if(s.type==1)s.flags=f.u();if(s.type!=1&&s.type!=2)throw std::runtime_error("Invalid driver stream");s.offset=f.p;f.skip(size_t(s.count)*s.stride);streams.push_back(s);}
 for(auto n=f.count();n--;){f.u();Mesh m;m.name="native_driver";m.material=f.u();auto nv=f.count(),flags=f.u(),np=f.count(),mode=f.u(),ni=f.count();(void)flags;(void)np;f.skip(24);if(f.u()!=2)throw std::runtime_error("Driver requires indexed stream");auto vs=f.u(),vo=f.u(),is=f.u(),io=f.u();if(vs>=streams.size()||is>=streams.size()||m.material>=d.geometry.materials.size())throw std::runtime_error("Invalid driver surface");auto&v=streams[vs];auto&i=streams[is];if(v.type!=1||v.stride!=36||v.flags!=0x152||i.type!=2||vo%v.stride||uint64_t(vo)+uint64_t(nv)*36>uint64_t(v.count)*v.stride||uint64_t(io)+uint64_t(ni)*2>uint64_t(i.count)*2)throw std::runtime_error("Unsupported driver skin stream");std::vector<DriverSkinWeight>w;for(uint32_t j=0;j<nv;j++){Reader a{bytes,v.offset+vo+j*36};Vertex q;q.position=a.vec();q.normal=a.vec();auto packed=a.u();DriverSkinWeight skin{{uint8_t(packed),uint8_t(packed>>16)},{uint8_t(packed>>8),uint8_t(packed>>24)}};if(skin.bone[0]>=11||skin.bone[1]>=11||int(skin.weight[0])+skin.weight[1]!=255)throw std::runtime_error("Invalid native two-bone weight");d.blended_vertices+=(skin.weight[0]!=0&&skin.weight[1]!=0);q.u=q.lu=a.f();q.v=q.lv=a.f();m.vertices.push_back(q);w.push_back(skin);}Reader a{bytes,i.offset+io};std::vector<uint32_t>indices;for(uint32_t j=0;j<ni;j++){auto id=a.get<uint16_t>();if(id<vo/36||uint32_t(id)-vo/36>=nv)throw std::runtime_error("Invalid driver index");indices.push_back(id-vo/36);}if(mode==4)m.indices=indices;else if(mode==5){for(size_t j=2;j<indices.size();j++){auto x=indices[j-2],y=indices[j-1],z=indices[j];if(x==y||x==z||y==z)continue;if(j&1)std::swap(x,y);m.indices.insert(m.indices.end(),{x,y,z});}}else throw std::runtime_error("Unsupported driver primitive");bounds(m);d.geometry.triangles+=m.indices.size()/3;d.geometry.meshes.push_back(std::move(m));d.weights.push_back(std::move(w));}
 // BGM model/root records are intentionally not applied: BSA includes the root
 // pelvis offset and inverse bind. Applying BGM's placement again doubles it.
 auto animation=r.read("drivers/interior/"+gender+"_hands.bsa");Reader a{animation};if(a.u()!=0x10002||a.u()!=11)throw std::runtime_error("Unsupported native BSA");d.start=a.u();d.duration=a.u();d.rate=a.u();for(size_t i=0;i<11;i++){auto n=a.count();auto&t=d.tracks[i];t.parent=int32_t(a.u());if(!n||t.parent< -1||t.parent>=int(i))throw std::runtime_error("Invalid native BSA hierarchy");t.inverse_bind=a.mat();uint32_t previous=0;for(uint32_t j=0;j<n;j++){DriverKey k;k.time=a.u();k.position=a.vec();for(auto&q:k.rotation)q=a.f();quaternion(k.rotation);if(j&&k.time<previous)throw std::runtime_error("Unsorted native BSA keys");previous=k.time;t.keys.push_back(k);}}if(a.p!=animation.size())throw std::runtime_error("Unexpected native BSA tail");
 auto definition=bed_file(r,"drivers/ragdoll/"+gender+"_bones.ini");d.native_bones=definition.at("Bones");d.native_eject_poses=definition.at("EjectPoseSegments");d.native_behavior=r.text("drivers/"+gender+".ini");const char*names[]={"lower_torso","upper_leg_l","lower_leg_l","upper_leg_r","lower_leg_r","upper_torso","head","upper_arm_l","lower_arm_l","upper_arm_r","lower_arm_r"};for(size_t i=0;i<11;i++){auto&s=d.segments[i];s.name=names[i];s.parent=d.tracks[i].parent;auto j=definition.at("Segments").at(s.name);s.center=vector3(j.at("Position"));s.dimension=vector3(j.at("Dimension"));s.orientation=quaternion(quat(j.at("Orientation")));s.joint=vector3(d.native_bones.at(std::to_string(i+1)).at("Position"));std::smatch hit;if(std::regex_search(d.native_behavior,hit,std::regex("Segments\\."+s.name+"\\.Mass\\s*=\\s*([0-9.]+)")))s.mass=std::stof(hit[1]); }
 const bool johnny=model==DriverModel::JohnnyAlpha&&r.exists(johnny_mesh)&&r.exists(johnny_atlas);
 if(johnny)replace_with_johnny(r,d);
 for(int h=0;h<2;++h){
  const auto& arm=d.segments[h?10:8];
  const auto along=unit(arm.center-arm.joint);
  const auto palm=unit(Vec{0,0,1}-along*along.z);
  // Distances from the lower-arm bind joint to the modeled sleeve cuff.
  const float length=johnny?.300f:model==DriverModel::Female?.271f:.287f;
  d.hands[h]={arm.joint+along*length,along,palm,true};
 }
 return d;
}
DriverPlacement load_driver_placement(const Resources&r,int id,const Geometry&body,const NativeDriver* driver){
 if(id<1||id>20)throw std::runtime_error("Invalid driver car ID");
 DriverPlacement p;
 const bool alpha=alpha_car_id(id);
 const auto number=std::to_string(id);
 const auto folder=alpha?alpha_car_directory(id):"cars/car_"+number;
 const auto seat_file=alpha?folder+"/car.ini":"equipmnt/car"+number+"/body_car"+number+".bed";
 bool authored=false;
 try{
  const auto seat_data=bed_file(r,seat_file);
  p.seat=vector3(alpha?seat_data.at("DriverLoc"):
                    seat_data.at("Car.Body.Car"+number).at("Settings").at("DriverLoc"));
  authored=seat_finite(p.seat);
  if(authored)p.source=seat_file+":DriverLoc";
 }catch(const std::exception&){
  // Some prototype and replacement cabins lack a usable DriverLoc.
 }
 if(auto mount=steering_mount(body)){
  p.steering=mount->frame;
  p.rim_radius=mount->radius;
  p.rim_z=mount->rim_z;
  p.has_steering=true;
 }
 p=fit_driver_placement(body,p,authored);
 const auto override_path=folder+"/driver_seat.json";
 if(r.exists(override_path)){
  const auto coordinates=Json::parse(r.text(override_path)).at("seat");
  if(!coordinates.is_array()||coordinates.size()!=3)
   throw std::runtime_error(override_path+": seat must be [x,y,z]");
  p.seat={coordinates[0].get<float>(),coordinates[1].get<float>(),coordinates[2].get<float>()};
  if(!seat_finite(p.seat))throw std::runtime_error(override_path+": nonfinite seat coordinate");
  p.source=override_path;
 }
 if(p.has_steering){
  // 10 and 2 o'clock on the centerline of the replacement rim tube.
  const float radius=p.rim_radius*.944f;
  for(int h=0;h<2;++h)p.grip[h]={(h?1.f:-1.f)*radius*.8660254f,radius*.5f,p.rim_z};
 }
 if(driver&&p.has_steering&&p.source!=override_path){
  const auto wheel=point(p.steering,{0,0,p.rim_z});
  p.seat.x=wheel.x;
  auto seated=sample_driver_pose(*driver,p,.5f,0,false);
  const auto neutral_head=point(seated.parts[6],{});
  const float height=std::clamp(wheel.y+.265f-neutral_head.y,-.20f,.055f);
  p.seat.y+=height;
  // Fit the seat using real shoulder and wrist locations across the steering
  // sweep. Reserve room for elbow flex rather than maxing out the arm reach.
  float advance=0;
  for(int h=0;h<2;++h){
   const size_t upper=h?9:7,lower=upper+1;
   const auto shoulder=point(seated.skin[upper],driver->segments[upper].joint)+Vec{0,height,0};
   const auto elbow=point(seated.skin[lower],driver->segments[lower].joint)+Vec{0,height,0};
   const auto wrist=point(seated.skin[lower],driver->hands[h].wrist)+Vec{0,height,0};
   const float reach=length(elbow-shoulder)+length(wrist-elbow)-.045f;
   for(float angle:{-.72f,0.f,.72f}){
    const auto delta=wheel_grip(p,h,angle).wrist-shoulder;
    const float axial=std::sqrt(std::max(.01f,reach*reach-delta.x*delta.x-delta.y*delta.y));
    advance=std::max(advance,delta.z-axial);
   }
  }
  p.seat.z+=std::clamp(advance,0.f,.24f);
  seated=sample_driver_pose(*driver,p,.5f,0,false);
  float head_top=-INFINITY;
  Vec head=point(seated.parts[6],{});
  for(size_t i=0;i<driver->geometry.meshes.size();++i){
   const auto&mesh=driver->geometry.meshes[i];
   for(size_t v=0;v<mesh.vertices.size();++v){
    const auto&weights=driver->weights[i][v];
    Vec pos{};unsigned head_weight=0;
    for(int j=0;j<4;++j)if(weights.weight[j]){
     if(weights.bone[j]==6)head_weight+=weights.weight[j];
     pos+=point(seated.skin[weights.bone[j]],mesh.vertices[v].position)*(weights.weight[j]/255.f);
    }
    if(head_weight>=128)head_top=std::max(head_top,pos.y);
   }
  }
  float roof=INFINITY;
  for(const auto&mesh:body.meshes){
   if(body.materials[mesh.material].shader!=5)continue;
   for(size_t i=0;i+2<mesh.indices.size();i+=3){
    const Vec a=point(mesh.transform,mesh.vertices[mesh.indices[i]].position);
    const Vec b=point(mesh.transform,mesh.vertices[mesh.indices[i+1]].position)-a;
    const Vec c=point(mesh.transform,mesh.vertices[mesh.indices[i+2]].position)-a;
    const float det=b.x*c.z-b.z*c.x;
    if(std::abs(det)<1e-5f)continue;
    const Vec d=head-a;
    const float u=(d.x*c.z-d.z*c.x)/det,v=(b.x*d.z-b.z*d.x)/det;
    if(u<0||v<0||u+v>1)continue;
    const float y=a.y+b.y*u+c.y*v;
    if(y>p.seat.y+.58f)roof=std::min(roof,y);
   }
  }
  if(std::isfinite(roof)&&std::isfinite(head_top))
   p.seat.y-=std::clamp(head_top+.045f-roof,0.f,.30f);
 }
 return p;
}
DriverPose sample_driver_pose(const NativeDriver&d,const DriverPlacement&p,float phase,float angle,bool lock){DriverPose o;phase=std::isfinite(phase)?std::clamp(phase,0.f,1.f):.5f;angle=steering_wheel_angle(angle);float time=float(d.start)+float(d.duration)*phase;for(size_t i=0;i<11;i++){auto&t=d.tracks[i];auto hi=std::lower_bound(t.keys.begin(),t.keys.end(),time,[](auto&k,float x){return k.time<x;});auto b=hi==t.keys.end()?t.keys.back():*hi;auto a=hi==t.keys.begin()?b:*(hi-1);float blend=b.time==a.time?0:std::clamp((time-a.time)/float(b.time-a.time),0.f,1.f);float sign=0;for(int k=0;k<4;k++)sign+=a.rotation[k]*b.rotation[k];auto q=a.rotation;for(int k=0;k<4;k++)q[k]=a.rotation[k]*(1-blend)+b.rotation[k]*blend*(sign<0?-1:1);auto local=multiply(translate(a.position*(1-blend)+b.position*blend),quaternion(q));o.bones[i]=t.parent<0?multiply(translate(p.seat),local):multiply(o.bones[t.parent],local);o.skin[i]=multiply(o.bones[i],t.inverse_bind);}
 if(lock&&p.has_steering)pose_grip(d,p,angle,o);
 for(size_t i=0;i<11;i++){o.parts[i]=multiply(o.skin[i],translate(d.segments[i].center));}
 return o;}
DriverPose sample_seated_driver_pose(const NativeDriver&d,const DriverPlacement&p,float steering){return sample_driver_pose(d,p,.5f,steering,true);}
Geometry make_driver_geometry(const NativeDriver&d){auto g=d.geometry;for(auto&m:g.meshes)m.transform=identity();return g;}
void deform_driver(const NativeDriver& d,const DriverPose& p,Geometry& g){
 if(g.meshes.size()!=d.geometry.meshes.size())throw std::runtime_error("Driver output topology mismatch");
 for(size_t i=0;i<g.meshes.size();++i){
  auto& mesh=g.meshes[i];
  const auto& source=d.geometry.meshes[i];
  if(mesh.vertices.size()!=source.vertices.size())throw std::runtime_error("Driver output vertex count mismatch");
  for(size_t j=0;j<mesh.vertices.size();++j){
   const auto& vertex=source.vertices[j];
   const auto& weights=d.weights[i][j];
   Vec position{},normal{};
   for(size_t k=0;k<4;++k){
    if(!weights.weight[k])continue;
    const auto index=weights.bone[k];
    const auto& bone=p.skin[index];
    const float weight=weights.weight[k]/255.f;
    Vec local_position=point(bone,vertex.position),local_normal=direction(bone,vertex.normal);
    if(index==8||index==10){
     const size_t hand=index==8?0:1;
     if(p.posed_hands[hand])skin_hand(d.hands[hand],p.hand_skin[hand],vertex,local_position,local_normal);
    }
    position+=local_position*weight;
    normal+=local_normal*weight;
   }
   mesh.vertices[j].position=position;
   mesh.vertices[j].normal=unit(normal);
  }
  bounds(mesh);
 }
}
DriverPose driver_ragdoll_pose(const NativeDriver&d,const std::array<Mat,11>&parts){DriverPose p;p.parts=parts;for(size_t i=0;i<11;i++){p.skin[i]=multiply(parts[i],translate(d.segments[i].center*-1));p.bones[i]=multiply(p.skin[i],translate(d.segments[i].joint));}return p;}
}
