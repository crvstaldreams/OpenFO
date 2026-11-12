#include "editor_scene_query.hpp"
#include "assets.hpp"
#include "water.hpp"
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <cmath>
namespace ofo {
namespace {
bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
btVector3 bt(Vec p){return {p.x,p.y,p.z};}
struct DVec {
 double x,y,z;
 explicit DVec(Vec p):x(p.x),y(p.y),z(p.z){}
 explicit DVec(const btVector3&p):x(p.x()),y(p.y()),z(p.z()){}
 DVec(double a,double b,double c):x(a),y(b),z(c){}
 DVec operator-(DVec b)const{return {x-b.x,y-b.y,z-b.z};}
 DVec cross(DVec b)const{return {y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x};}
 double dot(DVec b)const{return x*b.x+y*b.y+z*b.z;}
};
struct RayCallback final:btTriangleCallback {
 DVec origin,dir;
 double best;
 bool hit=false;
 size_t tests=0;
 RayCallback(Vec o,Vec d,float limit):origin(o),dir(d),best(limit){}
 void processTriangle(btVector3*t,int,int)override{
  ++tests;
  const DVec a(t[0]),e1=DVec(t[1])-a,e2=DVec(t[2])-a,p=dir.cross(e2);
  const double det=e1.dot(p);
  // Double precision avoids a fixed area threshold discarding small ground faces.
  if(det==0)return;
  const DVec offset=origin-a;
  const double u=offset.dot(p)/det;
  if(u<0||u>1)return;
  const DVec q=offset.cross(e1);
  const double v=dir.dot(q)/det;
  if(v<0||u+v>1)return;
  const double distance=e2.dot(q)/det;
  if(distance>.1f&&distance<=best){best=distance;hit=true;}
 }
};
}
struct EditorSceneQuery::Impl {
 // The shape borrows its mesh. Declaration order keeps the mesh alive until
 // after the shape and its BVH have been destroyed.
 btTriangleMesh triangles;
 std::unique_ptr<btBvhTriangleMeshShape> shape;
 mutable size_t last_tests=0;
 explicit Impl(const Geometry&g){
  for(const auto&m:g.meshes){
   if(m.prop_index>=0||m.captured||m.billboard||!m.solid||m.material>=g.materials.size())continue;
   const auto&material=g.materials[m.material];
   if(water_material(material)||lower(material.name).find("shadow")!=std::string::npos)continue;
   for(size_t i=0;i+2<m.indices.size();i+=3){
    if(m.indices[i]>=m.vertices.size()||m.indices[i+1]>=m.vertices.size()||m.indices[i+2]>=m.vertices.size())continue;
    const Vec a=point(m.transform,m.vertices[m.indices[i]].position),b=point(m.transform,m.vertices[m.indices[i+1]].position),c=point(m.transform,m.vertices[m.indices[i+2]].position);
    if(!finite(a)||!finite(b)||!finite(c))continue;
    const auto n=(DVec(b)-DVec(a)).cross(DVec(c)-DVec(a));
    if(n.x==0&&n.y==0&&n.z==0)continue;
    triangles.addTriangle(bt(a),bt(b),bt(c),false);
   }
  }
  if(triangles.getNumTriangles())shape=std::make_unique<btBvhTriangleMeshShape>(&triangles,true);
 }
};
EditorSceneQuery::EditorSceneQuery(const Geometry&g):impl_(std::make_unique<Impl>(g)){}
EditorSceneQuery::~EditorSceneQuery()=default;
EditorSceneQuery::EditorSceneQuery(EditorSceneQuery&&) noexcept=default;
EditorSceneQuery& EditorSceneQuery::operator=(EditorSceneQuery&&) noexcept=default;
size_t EditorSceneQuery::triangle_count()const{return impl_?size_t(impl_->triangles.getNumTriangles()):0;}
size_t EditorSceneQuery::last_triangle_tests()const{return impl_?impl_->last_tests:0;}
std::optional<Vec> EditorSceneQuery::ray(Vec origin,Vec direction,float maxDistance)const{
 if(!impl_)return {};
 impl_->last_tests=0;
 if(!impl_->shape||!finite(origin)||!finite(direction)||!std::isfinite(maxDistance)||maxDistance<=.1f)return {};
 const double norm=std::sqrt(DVec(direction).dot(DVec(direction)));
 if(norm==0)return {};
 const Vec dir={float(direction.x/norm),float(direction.y/norm),float(direction.z/norm)};
 const Vec end=origin+dir*maxDistance;
 if(!finite(end))return {};
 RayCallback callback(origin,dir,maxDistance);
 impl_->shape->performRaycast(&callback,bt(origin),bt(end));
 impl_->last_tests=callback.tests;
 if(!callback.hit)return {};
 return origin+dir*float(callback.best);
}
}
