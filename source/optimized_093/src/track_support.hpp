#pragma once
#include "assets.hpp"
#include <btBulletDynamicsCommon.h>
#include <map>
#include <tuple>

namespace ofo {
inline constexpr float min_wheel_support_up=.40f;
// Force normals only: collision triangles, sweep reach, materials and walls are
// unchanged. Shared-edge interpolation avoids a new force axis at each seam.
class TrackSupportNormals {
 struct Face {std::array<Vec,3> p,edge_normal;Vec normal;};
 std::vector<Face> faces;
 static std::array<float,3> barycentric(Vec p,const Face&f){
  Vec a=f.p[1]-f.p[0],b=f.p[2]-f.p[0],v=p-f.p[0];
  float aa=dot(a,a),ab=dot(a,b),bb=dot(b,b),av=dot(a,v),bv=dot(b,v),d=aa*bb-ab*ab;
  if(d<1e-12f)return {-1,-1,-1};float y=(bb*av-ab*bv)/d,z=(aa*bv-ab*av)/d;return {1-y-z,y,z};
 }
public:
 void add(Vec a,Vec b,Vec c){auto n=unit(cross(b-a,c-a));faces.push_back({{{a,b,c}},{{n,n,n}},n});}
 void build(){
  using Key=std::tuple<long long,long long,long long>;
  using Edge=std::pair<Key,Key>;
  std::map<Edge,std::vector<std::pair<size_t,int>>> edges;
  auto key=[](Vec p){return Key{std::llround(p.x*10000),std::llround(p.y*10000),std::llround(p.z*10000)};};
  for(size_t i=0;i<faces.size();++i)for(int j=0;j<3;++j){auto a=key(faces[i].p[(j+1)%3]),b=key(faces[i].p[(j+2)%3]);if(b<a)std::swap(a,b);edges[{a,b}].push_back({i,j});}
  for(const auto&entry:edges)for(auto index:entry.second){auto&face=faces[index.first];Vec sum{};
   for(auto neighbor:entry.second){Vec n=faces[neighbor.first].normal;if(dot(n,face.normal)<0)n=n*-1;
    // Approximately 20 degrees: curbs, walls and sharp crests remain hard.
    if(dot(n,face.normal)>=.94f)sum=sum+n;
   }face.edge_normal[index.second]=unit(sum,face.normal);
  }
 }
 // A whole short triangle must fit the established road support band. Checking
 // all vertices preserves tall walls even when their lowest contact is at road level.
 bool low_riser(int triangle,Vec point,Vec normal,float height)const{
  if(triangle<0||size_t(triangle)>=faces.size())return false;
  for(auto p:faces[size_t(triangle)].p)if(std::abs(dot(p-point,normal))>height)return false;
  return true;
 }
 // Interior convex/plane support is analytic. GJK sweeps of native tire hulls
 // against very large thin triangles can return alternating late/missing hits.
 // This uses the unchanged hull's support mapping, not a larger support radius.
 struct PlaneContact {bool hit=false;float travel=0;Vec point{},normal{};int triangle=-1;};
 PlaneContact plane_support(const btBvhTriangleMeshShape&shape,const btConvexShape&tire,const btTransform&from,Vec up,float travel)const{
  struct Query final:btTriangleCallback {
   const TrackSupportNormals&mesh;const btConvexShape&tire;const btTransform&from;Vec up;PlaneContact result;
   Query(const TrackSupportNormals&m,const btConvexShape&t,const btTransform&f,Vec u,float reach):mesh(m),tire(t),from(f),up(u){result.travel=reach;}
   void processTriangle(btVector3*,int,int id)override{
    if(id<0||size_t(id)>=mesh.faces.size())return;const auto&face=mesh.faces[size_t(id)];Vec n=face.normal;if(dot(n,up)<0)n=n*-1;float vertical=dot(n,up);if(vertical<=min_wheel_support_up)return;
    btVector3 normal(n.x,n.y,n.z),axis=from.getBasis().transpose()*(-normal);
    // Average ties on a support face, avoiding an arbitrary left/right vertex
    // becoming the force lever arm on flat tread. All samples stay on the hull.
    btVector3 support(0,0,0);for(btVector3 epsilon:{btVector3(1e-5f,0,0),btVector3(-1e-5f,0,0),btVector3(0,0,1e-5f),btVector3(0,0,-1e-5f)})support+=from*tire.localGetSupportingVertex(axis+epsilon);support*=.25f;
    Vec point{support.x(),support.y(),support.z()};float distance=dot(n,point-face.p[0])/vertical;if(distance<-.002f||distance>result.travel)return;
    point=point-up*distance;auto b=TrackSupportNormals::barycentric(point,face);if(*std::min_element(b.begin(),b.end())<-.00001f)return;
    result={true,std::max(0.f,distance),point,n,id};
   }
  }query(*this,tire,from,up,travel);
  btVector3 lo,hi;tire.getAabb(from,lo,hi);btVector3 delta(up.x*travel,up.y*travel,up.z*travel);lo.setMin(lo-delta);hi.setMax(hi-delta);shape.processAllTriangles(&query,lo,hi);return query.result;
 }
 Vec sample(const btBvhTriangleMeshShape&shape,Vec hit,Vec up,Vec forward,float width,float radius,int triangle)const{
  if(triangle<0||size_t(triangle)>=faces.size())return up;
  const auto&base=faces[size_t(triangle)];Vec normal=dot(base.normal,up)<0?base.normal*-1:base.normal;
  if(dot(normal,up)<=.55f)return normal;
  Vec fw=unit(forward-up*dot(forward,up),{0,0,1}),right=unit(cross(up,fw));
  struct Query final:btTriangleCallback {
   const TrackSupportNormals&mesh;Vec hit,up,normal;std::array<Vec,5>offsets;
   std::array<Vec,5>normals{};std::array<float,5>heights;float continuity,patch;
   Query(const TrackSupportNormals&m,Vec h,Vec u,Vec n,std::array<Vec,5>o,float c,float p):mesh(m),hit(h),up(u),normal(n),offsets(o),continuity(c),patch(p){heights.fill(-1e9f);}
   void processTriangle(btVector3*,int,int id)override{
    if(id<0||size_t(id)>=mesh.faces.size())return;const auto&face=mesh.faces[size_t(id)];auto n=dot(face.normal,up)<0?face.normal*-1:face.normal;
    float vertical=dot(n,up);if(vertical<=.55f||dot(n,normal)<.94f)return;
    for(size_t i=0;i<offsets.size();++i){Vec at=hit+offsets[i];float t=dot(n,face.p[0]-at)/vertical;
     float expected=-dot(normal,offsets[i])/dot(normal,up);if(std::abs(t-expected)>continuity||t<heights[i])continue;
     auto b=TrackSupportNormals::barycentric(at+up*t,face);if(*std::min_element(b.begin(),b.end())<-.0001f)continue;
     // Barycentric distance to each shared edge, bounded by tire patch:
     // huge road triangles must not tilt forces meters before a ramp.
     Vec smooth=face.normal;
     for(int edge=0;edge<3;++edge){Vec a=face.p[(edge+1)%3],e=face.p[(edge+2)%3]-a;float altitude=length(cross(face.p[edge]-a,e))/std::max(1e-6f,length(e));float x=std::clamp(1-b[edge]*altitude/patch,0.f,1.f);float weight=x*x*(3-2*x);smooth=smooth+(face.edge_normal[edge]-face.normal)*weight;}
     smooth=unit(smooth,face.normal);if(dot(smooth,up)<0)smooth=smooth*-1;
     // Edge interpolation cannot soften a sharp contact discontinuity.
     if(dot(smooth,normal)<.94f)smooth=n;normals[i]=smooth;heights[i]=t;
    }
   }
  }query(*this,hit,up,normal,{{{},right*(width*.35f),right*(-width*.35f),fw*(radius*.35f),fw*(-radius*.35f)}},std::min(.045f,radius*.15f),std::max(.01f,std::min(width,radius)*.35f));
  Vec extent{.05f,.05f,.05f};for(auto o:query.offsets){float travel=std::abs(dot(normal,o)/dot(normal,up))+query.continuity;extent.x=std::max(extent.x,std::abs(o.x)+std::abs(up.x)*travel+.002f);extent.y=std::max(extent.y,std::abs(o.y)+std::abs(up.y)*travel+.002f);extent.z=std::max(extent.z,std::abs(o.z)+std::abs(up.z)*travel+.002f);}
  auto lo=hit-extent,hi=hit+extent;shape.processAllTriangles(&query,{lo.x,lo.y,lo.z},{hi.x,hi.y,hi.z});
  Vec sum{};for(size_t i=0;i<query.normals.size();++i)if(query.heights[i]>-1e8f)sum=sum+query.normals[i]*(i==0?2.f:1.f);return unit(sum,normal);
 }
};
}
