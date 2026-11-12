#include "render_clusters.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace ofo {
namespace {
bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool affine_finite(const Mat& m){
 for(float x:m)if(!std::isfinite(x))return false;
 return m[3]==0&&m[7]==0&&m[11]==0&&m[15]==1;
}
bool supported(const Mesh& m,const Material& mat,const RenderClusterOptions& options){
 if(m.prop_index>=0||m.billboard||m.vertices.empty()||m.indices.size()%3||
    m.indices.size()/3<options.min_source_triangles||
    !std::isfinite(m.radius)||m.radius<options.min_source_radius||
    !finite(m.center)||!affine_finite(m.transform)||
    m.vertices.size()>std::numeric_limits<uint32_t>::max())return false;
 // Conservatively exclude translucent materials, animated water, foliage
 // billboards and special object-space/native shader programs.
 if(mat.alpha!=0)return false;
 auto name=mat.name;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
 if(name.find("water")!=name.npos||name.find("glass")!=name.npos||name.find("window")!=name.npos)return false;
 switch(mat.shader){case 0:case 1:case 3:case 4:case 7:case 8:case 9:
                    case 15:case 16:case 18:case 19:break;
                    default:return false;}
 return true;
}
struct Extent {
 Vec lo{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
 Vec hi{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
 void add(Vec x){for(int i=0;i<3;++i){lo[i]=std::min(lo[i],x[i]);hi[i]=std::max(hi[i],x[i]);}}
 Vec span()const{return hi-lo;}
 Vec middle()const{return (hi+lo)*.5f;}
};
}
RenderClusterResult build_render_clusters(const Mesh& source,const Material& material,
                                          const RenderClusterOptions& o){
 RenderClusterResult result;
 if(o.min_source_triangles<2||o.target_triangles<1||o.min_leaf_triangles<1||
    o.max_clusters<2||o.max_clusters>256||!std::isfinite(o.min_cluster_span)||
    o.min_cluster_span<=0||!std::isfinite(o.max_vertex_expansion)||
    o.max_vertex_expansion<1.f||!supported(source,material,o))return result;
 const size_t triangles=source.indices.size()/3;
 // Validate before producing any partially rendered geometry: invalid data
 // fails open to the original mesh instead of silently dropping triangles.
 std::vector<Vec> world(source.vertices.size());
 for(size_t i=0;i<world.size();++i){world[i]=point(source.transform,source.vertices[i].position);if(!finite(world[i]))return result;}
 std::vector<Vec> centroids(triangles);
 Extent full,centroid_extent;
 for(size_t i=0;i<triangles;++i){
  const auto a=source.indices[3*i],b=source.indices[3*i+1],c=source.indices[3*i+2];
  if(a>=world.size()||b>=world.size()||c>=world.size())return result;
  centroids[i]=(world[a]+world[b]+world[c])/3.f;
  if(!finite(centroids[i]))return result;
  centroid_extent.add(centroids[i]);
  full.add(world[a]);full.add(world[b]);full.add(world[c]);
 }
 const Vec extent=full.span(),cextent=centroid_extent.span();
 if(std::max({extent.x,extent.y,extent.z})<=o.min_cluster_span||
    std::max({cextent.x,cextent.y,cextent.z})<=o.min_cluster_span*.5f)return result;
 size_t depth_budget=0;
 while((size_t(1)<<(depth_budget+1))<=o.max_clusters&&depth_budget<8)++depth_budget;
 if(!depth_budget)return result;
 std::vector<size_t> order(triangles);
 std::iota(order.begin(),order.end(),size_t(0));
 struct Leaf{size_t begin,end;};
 std::vector<Leaf> leaves;
 // Partition on triangle CENTROIDS, but compute bounding spheres from every
 // referenced VERTEX. A triangle crossing a cluster boundary cannot disappear.
 auto partition=[&](auto&& self,size_t begin,size_t end,size_t depth)->void{
  const size_t count=end-begin;
  if(count<=o.target_triangles||count<2*o.min_leaf_triangles||depth>=depth_budget){
   leaves.push_back({begin,end});return;
  }
  Extent ranges;
  for(size_t i=begin;i<end;++i)ranges.add(centroids[order[i]]);
  Vec span=ranges.span();int axis=0;
  if(span.y>span[axis])axis=1;
  if(span.z>span[axis])axis=2;
  if(span[axis]<=o.min_cluster_span*.5f){leaves.push_back({begin,end});return;}
  const size_t mid=begin+count/2;
  std::nth_element(order.begin()+begin,order.begin()+mid,order.begin()+end,
   [&](size_t a,size_t b){float x=centroids[a][axis],y=centroids[b][axis];return x==y?a<b:x<y;});
  self(self,begin,mid,depth+1);
  self(self,mid,end,depth+1);
 };
 partition(partition,0,triangles,0);
 if(leaves.size()<2)return result;
 result.meshes.reserve(leaves.size());
 std::vector<uint32_t> remap(world.size(),std::numeric_limits<uint32_t>::max());
 std::vector<uint32_t> touched;
 size_t vertex_total=0,index_total=0;
 for(size_t slot=0;slot<leaves.size();++slot){
  const Leaf leaf=leaves[slot];
  Mesh mesh;
  mesh.name=source.name+"_cluster_"+std::to_string(slot);
  mesh.material=source.material;
  mesh.source_surface=source.source_surface;
  mesh.captured=source.captured;
  mesh.solid=false;                         // only a GPU draw packet
  mesh.transform=source.transform;
  mesh.indices.reserve((leaf.end-leaf.begin)*3);
  mesh.vertices.reserve(std::min(world.size(),(leaf.end-leaf.begin)*3));
  touched.clear();
  Extent bounds;
  for(size_t i=leaf.begin;i<leaf.end;++i){
   const size_t tri=order[i];
   for(int k=0;k<3;++k){
    const uint32_t original=source.indices[tri*3+k];
    if(remap[original]==std::numeric_limits<uint32_t>::max()){
     const auto local=uint32_t(mesh.vertices.size());
     remap[original]=local;
     touched.push_back(original);
     mesh.vertices.push_back(source.vertices[original]);
     bounds.add(world[original]);
    }
    mesh.indices.push_back(remap[original]);
   }
  }
  for(auto i:touched)remap[i]=std::numeric_limits<uint32_t>::max();
  mesh.center=bounds.middle();
  double radius=0;
  for(auto i:touched){const auto p=world[i];
   const double dx=double(p.x)-mesh.center.x,dy=double(p.y)-mesh.center.y,dz=double(p.z)-mesh.center.z;
   radius=std::max(radius,std::sqrt(dx*dx+dy*dy+dz*dz));}
  // Error padding prevents sphere-plane false rejections at large coordinates.
  radius=radius*1.0001+0.001;
  if(!std::isfinite(radius)||radius>std::numeric_limits<float>::max())return {};
  mesh.radius=std::nextafter(float(radius),std::numeric_limits<float>::infinity());
  vertex_total+=mesh.vertices.size();index_total+=mesh.indices.size();
  result.meshes.push_back(std::move(mesh));
 }
 // Avoid excessive VRAM/upload growth from degenerate mesh topology.
 if(index_total!=source.indices.size()||
    double(vertex_total)>double(source.vertices.size())*o.max_vertex_expansion)return {};
 result.source_triangles=triangles;
 result.output_triangles=index_total/3;
 result.output_vertices=vertex_total;
 return result;
}
}
