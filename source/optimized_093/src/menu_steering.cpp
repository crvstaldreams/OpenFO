#include "menu_steering.hpp"
#include "alpha_car.hpp"

namespace ofo {
namespace {
constexpr char menu_stage[]="tracks/menu/menu1/a";
constexpr char menu_geometry[]="tracks/menu/menu1/geometry/track_geom.w32";
constexpr char wheel_material[]="menu_steering_wheel";

struct Bounds {
 Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
 void add(Vec p){
  for(int i=0;i<3;++i){lo[i]=std::min(lo[i],p[i]);hi[i]=std::max(hi[i],p[i]);}
 }
 Vec center()const{return (lo+hi)*.5f;}
};

template<class Select>
Mesh triangles_from(const Mesh& source,Select select){
 Mesh out;
 out.material=source.material;
 out.name=source.name;
 out.transform=source.transform;
 out.source_surface=source.source_surface;
 std::vector<int> mapping(source.vertices.size(),-1);
 for(size_t i=0;i+2<source.indices.size();i+=3){
  const auto a=source.indices[i],b=source.indices[i+1],c=source.indices[i+2];
  if(!select(source.vertices[a],source.vertices[b],source.vertices[c]))continue;
  for(auto index:{a,b,c}){
   if(mapping[index]<0){
    mapping[index]=int(out.vertices.size());
    out.vertices.push_back(source.vertices[index]);
   }
   out.indices.push_back(uint32_t(mapping[index]));
  }
 }
 return out;
}

std::pair<float,float> rim_dimensions(const Mesh& mesh,Vec center){
 auto radial=[&](const Vertex& v){return std::hypot(v.position.x-center.x,v.position.y-center.y);};
 float radius=0;
 for(const auto& v:mesh.vertices)radius=std::max(radius,radial(v));
 std::vector<float> depths;
 for(const auto& v:mesh.vertices)
  if(radial(v)>radius*.9f)depths.push_back(v.position.z);
 if(depths.empty())return {};
 const auto mid=depths.begin()+depths.size()/2;
 std::nth_element(depths.begin(),mid,depths.end());
 return {radius,*mid};
}

void set_mesh_bounds(Mesh& mesh){
 Bounds bounds;
 for(const auto& v:mesh.vertices)bounds.add(point(mesh.transform,v.position));
 mesh.center=bounds.center();
 mesh.radius=length(bounds.hi-bounds.lo)*.5f;
}
}

std::optional<SteeringMount> steering_mount(const Geometry& body){
 for(size_t i=0;i<body.meshes.size();++i){
  const auto& mesh=body.meshes[i];
  if(lower(mesh.name).find("steering_wheel")==std::string::npos||mesh.vertices.empty())continue;
  Bounds bounds;
  for(const auto& v:mesh.vertices)bounds.add(v.position);
  const auto center=bounds.center();
  // The stock BGM origin is the pivot. CARC and baked steering wheels can
  // place the rim away from that origin.
  const bool baked=std::hypot(center.x,center.y)>.12f;
  const auto offset=alpha_car_resource(body.source)||baked?Vec{center.x,center.y,0}:Vec{};
  const auto [radius,depth]=rim_dimensions(mesh,offset);
  if(radius<.02f||radius>1)continue;
  return SteeringMount{i,multiply(mesh.transform,translate(offset)),radius,depth,offset};
 }
 return std::nullopt;
}

MenuSteeringWheel menu_steering_from_track(const Resources& resources,const Geometry& track){
 if(track.source!=menu_geometry)return {};
 for(const auto& mesh:track.meshes){
  const auto& mat=track.materials.at(mesh.material);
  if(lower(fs::path(mat.texture[0]).stem().string())!="dashboard")continue;
  // The lower-right atlas quadrant belongs to the rim, spokes and hub.
  // The dashboard and the steering column occupy the other UV islands.
  auto in_wheel=[](const Vertex& v){return v.u>.5f&&v.v>.5f;};
  auto rim=triangles_from(mesh,[&](const Vertex& a,const Vertex& b,const Vertex& c){
   return in_wheel(a)&&in_wheel(b)&&in_wheel(c);
  });
  if(rim.indices.empty())continue;
  auto texture=resources.texture(track.source,mat.texture[0]);
  if(texture.empty())return {};
  // The donor column in the W32 points forward along this measured axis.
  const auto forward=unit(Vec{-.59449f,-.28624f,-.75143f});
  const auto right=unit(cross({0,1,0},forward));
  const auto up=cross(forward,right);
  const auto origin=point(mesh.transform,rim.vertices.front().position);
  auto local=[&](Vec v){return Vec{dot(v,right),dot(v,up),dot(v,forward)};};
  Bounds bounds;
  for(auto& v:rim.vertices){
   v.position=local(point(mesh.transform,v.position)-origin);
   v.normal=unit(local(direction(mesh.transform,v.normal)));
   bounds.add(v.position);
  }
  const auto center=bounds.center();
  const auto [radius,depth]=rim_dimensions(rim,center);
  if(radius<.12f||radius>.23f)continue;
  for(auto& v:rim.vertices)v.position=v.position-Vec{center.x,center.y,depth};
  return {std::move(rim),std::move(texture),radius};
 }
 return {};
}

MenuSteeringWheel load_menu_steering(const Resources& resources){
 if(!resources.exists(menu_geometry))return {};
 return menu_steering_from_track(resources,load_geometry(resources,menu_geometry,menu_stage));
}

bool MenuSteeringWheel::apply(Geometry& body)const{
 if(radius<=0)return false;
 const auto mount=steering_mount(body);
 if(!mount)return false;
 auto& original=body.meshes[mount->mesh];
 const auto& material=body.materials.at(original.material);
 if(material.name==wheel_material)return false;
 // Stock BGM and some CARC wheels include a long shaft in the same surface.
 // Keep its original UVs and length. The replacement has its own rim and hub.
 auto close_to_axis=[&](const Vertex& v){
  return std::hypot(v.position.x-mount->local_center.x,v.position.y-mount->local_center.y)<mount->radius*.45f;
 };
 auto shaft=triangles_from(original,[&](const Vertex& a,const Vertex& b,const Vertex& c){
  return close_to_axis(a)&&close_to_axis(b)&&close_to_axis(c)&&
         std::max({a.position.z,b.position.z,c.position.z})>mount->rim_z+.11f;
 });

 auto wheel=rim;
 wheel.name=original.name;
 wheel.source_surface=original.source_surface;
 wheel.transform=mount->frame;
 wheel.material=body.materials.size();
 const float scale=mount->radius/radius;
 for(auto& v:wheel.vertices)v.position=v.position*scale+Vec{0,0,mount->rim_z};
 set_mesh_bounds(wheel);
 Material adapted;
 adapted.name=wheel_material;
 adapted.shader=material.shader;
 adapted.texture[0]=texture;
 body.materials.push_back(std::move(adapted));
 body.triangles=body.triangles-original.indices.size()/3+wheel.indices.size()/3+shaft.indices.size()/3;
 original=std::move(wheel);
 if(!shaft.indices.empty()){
  shaft.name="steering_column";
  set_mesh_bounds(shaft);
  body.meshes.push_back(std::move(shaft));
 }
 return true;
}
}
