#include "alpha_driver_seat.hpp"
#include "alpha_car.hpp"
#include "native_driver.hpp"

namespace ofo {
namespace {
constexpr char seat_name[]="alpha_driver_seat";
constexpr char donor_path[]="cars/car_3/body.bgm";
constexpr Vec donor_pelvis{-.359f,.217f,.067f};
constexpr float donor_floor=.1266666f;

bool donor_seat_vertex(const Vertex& v){
 const auto p=v.position;
 // Pepper's driver bucket and passenger bucket share one interior surface.
 // Use its model-space bounds and UV island to isolate the driver bucket.
 return p.x>=-.61571f&&p.x<=-.09653f&&p.y>=.12656f&&p.y<=.91855f&&
        p.z>=-.42365f&&p.z<=.26081f&&v.u>=.37339f&&v.u<=.72887f&&
        v.v>=.54315f&&v.v<=.99844f;
}

float floor_beneath(const Geometry& body,Vec pelvis){
 float floor=-INFINITY;
 for(const auto& mesh:body.meshes){
  if(mesh.name!="body"||body.materials[mesh.material].name!="interior")continue;
  for(size_t i=0;i+2<mesh.indices.size();i+=3){
   const Vec a=point(mesh.transform,mesh.vertices[mesh.indices[i]].position);
   const Vec b=point(mesh.transform,mesh.vertices[mesh.indices[i+1]].position)-a;
   const Vec c=point(mesh.transform,mesh.vertices[mesh.indices[i+2]].position)-a;
   const float det=b.x*c.z-b.z*c.x;
   if(std::abs(det)<1e-5f)continue;
   const Vec offset=pelvis-a;
   const float u=(offset.x*c.z-offset.z*c.x)/det;
   const float v=(b.x*offset.z-b.z*offset.x)/det;
   if(u<0||v<0||u+v>1)continue;
   const float y=a.y+u*b.y+v*c.y;
   if(y<pelvis.y-.018f)floor=std::max(floor,y);
  }
 }
 return std::isfinite(floor)?floor:pelvis.y-.09f;
}
}

bool add_alpha_driver_seat(const Resources& resources,Geometry& body,Vec pelvis){
 if(!alpha_car_resource(body.source)||fs::path(body.source).filename()=="model.wheels"||
    !resources.exists(donor_path))return false;
 for(const auto& mesh:body.meshes)if(mesh.name==seat_name)return false;

 const auto donor=load_geometry(resources,donor_path);
 for(const auto& source:donor.meshes){
  auto material=donor.materials[source.material];
  if(source.name!="body"||material.name!="interior")continue;
  material.texture[0]=resources.texture(donor.source,material.texture[0]);
  if(material.texture[0].empty())return false;
  Mesh seat;
  seat.name=seat_name;
  seat.solid=false;
  seat.material=body.materials.size();
  const float base_y=floor_beneath(body,pelvis);
  const float lower_scale=std::clamp((pelvis.y-base_y)/(donor_pelvis.y-donor_floor),.20f,3.f);
  std::vector<int> remap(source.vertices.size(),-1);
  Vec lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
  for(size_t i=0;i+2<source.indices.size();i+=3){
   const auto a=source.indices[i],b=source.indices[i+1],c=source.indices[i+2];
   if(!donor_seat_vertex(source.vertices[a])||!donor_seat_vertex(source.vertices[b])||
      !donor_seat_vertex(source.vertices[c]))continue;
   for(const auto index:{a,b,c}){
    if(remap[index]<0){
     auto vertex=source.vertices[index];
     const auto relative=point(source.transform,vertex.position)-donor_pelvis;
     const float y_scale=relative.y<0?lower_scale:.91f;
     vertex.position=pelvis+Vec{relative.x*.90f,relative.y*y_scale,relative.z};
     const auto normal=direction(source.transform,vertex.normal);
     vertex.normal=unit({normal.x/.90f,normal.y/y_scale,normal.z});
     for(int axis=0;axis<3;++axis){
      lo[axis]=std::min(lo[axis],vertex.position[axis]);
      hi[axis]=std::max(hi[axis],vertex.position[axis]);
     }
     remap[index]=int(seat.vertices.size());
     seat.vertices.push_back(vertex);
    }
    seat.indices.push_back(uint32_t(remap[index]));
   }
  }
  if(seat.indices.size()!=152*3)return false;
  seat.center=(lo+hi)*.5f;
  seat.radius=length(hi-lo)*.5f;
  material.name=seat_name;
  body.triangles+=seat.indices.size()/3;
  body.materials.push_back(std::move(material));
  body.meshes.push_back(std::move(seat));
  return true;
 }
 return false;
}

void add_alpha_showroom_seat(const Resources& resources,Geometry& body){
 for(int id=17;id<=20;++id){
  if(body.source!=alpha_car_directory(id)+"/model.showcar")continue;
  const auto driver=load_native_driver(resources);
  add_alpha_driver_seat(resources,body,load_driver_placement(resources,id,body,&driver).seat);
  return;
 }
}
}
