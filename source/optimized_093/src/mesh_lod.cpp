#include "mesh_lod.hpp"
#include "bounded_cpu_jobs.hpp"
#include "meshoptimizer/meshoptimizer.h"
#include <map>
#include <set>
#include <tuple>
#include <cstring>
#include <stdexcept>
namespace ofo {
namespace {
using Position=std::array<float,3>;

}
MeshLods build_mesh_lods(const std::vector<Vertex>&v,const std::vector<uint32_t>&ix){
 if(ix.size()%3)throw std::runtime_error("LOD: incomplete triangle");
 for(auto i:ix)if(i>=v.size())throw std::runtime_error("LOD: index outside vertices");
 MeshLods out;for(auto&level:out.indices)level=ix;
 // Tiny meshes keep their native triangles; validate their attributes without
 // allocating welding maps, seam tables or simplification attributes.
 if(ix.size()<72){for(const auto&a:v){float values[]={a.position.x,a.position.y,a.position.z,a.normal.x,a.normal.y,a.normal.z,a.u,a.v,a.lu,a.lv};for(float f:values)if(!std::isfinite(f))throw std::runtime_error("LOD: nonfinite vertex attribute");}return out;}
 Vec lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};
 std::map<Position,uint32_t>positions;std::vector<uint32_t>pos(v.size()),first;std::vector<bool>seam;
 std::map<std::array<uint32_t,11>,uint32_t>weld;std::vector<uint32_t>canonical(v.size());
 std::vector<float>attributes;attributes.reserve(v.size()*11);
 for(uint32_t i=0;i<v.size();i++){
  const auto&a=v[i];float values[]={a.position.x,a.position.y,a.position.z,a.normal.x,a.normal.y,a.normal.z,a.u,a.v,a.lu,a.lv};for(float f:values)if(!std::isfinite(f))throw std::runtime_error("LOD: nonfinite vertex attribute");
  for(int k=0;k<3;k++){lo[k]=std::min(lo[k],a.position[k]);hi[k]=std::max(hi[k],a.position[k]);}
  std::array<uint32_t,11>key{};std::memcpy(key.data(),values,sizeof(values));key[10]=a.color;canonical[i]=weld.emplace(key,i).first->second;
  auto entry=positions.emplace(Position{a.position.x,a.position.y,a.position.z},uint32_t(first.size()));auto id=entry.first->second;pos[i]=id;
  if(entry.second){first.push_back(i);seam.push_back(false);}else{const auto&b=v[first[id]];if(a.u!=b.u||a.v!=b.v||a.lu!=b.lu||a.lv!=b.lv)seam[id]=true;}
  for(float f:{a.normal.x,a.normal.y,a.normal.z,a.u,a.v,a.lu,a.lv})attributes.push_back(f);
  for(int shift:{16,8,0,24})attributes.push_back(float((a.color>>shift)&255)/255);
 }
 std::map<std::pair<uint32_t,uint32_t>,size_t>edges;for(size_t i=0;i<ix.size();i+=3)for(int k=0;k<3;k++){auto a=pos[ix[i+k]],b=pos[ix[i+(k+1)%3]];if(a!=b)++edges[std::minmax(a,b)];}
 for(auto&e:edges)if(e.second==1){seam[e.first.first]=true;seam[e.first.second]=true;}
 std::vector<unsigned char>locks(v.size());for(size_t i=0;i<v.size();i++)locks[i]=seam[pos[i]]?1:0;
 std::vector<uint32_t>welded;welded.reserve(ix.size());for(auto i:ix)welded.push_back(canonical[i]);
 float span=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});if(span<1e-6f||!std::isfinite(span))return out;
 const float weights[11]={.5f,.5f,.5f,1,1,1,1,.3f,.3f,.3f,1};
 for(int level=1;level<3;level++){
  std::vector<uint32_t>result(ix.size());float error=0;size_t target=std::max(size_t(12),size_t(ix.size()*(level==1?.5:.2))/3*3);
  auto count=meshopt_simplifyWithAttributes(result.data(),welded.data(),welded.size(),&v[0].position.x,v.size(),sizeof(Vertex),attributes.data(),11*sizeof(float),weights,11,locks.data(),target,span*(level==1?.02f:.06f),meshopt_SimplifyPermissive|meshopt_SimplifyErrorAbsolute,&error);
  result.resize(count);
  // Edge collapses may leave collinear faces; do not send them to the GPU.
  std::vector<uint32_t>clean;clean.reserve(count);for(size_t i=0;i<count;i+=3){auto a=result[i],b=result[i+1],c=result[i+2];if(a==b||b==c||a==c)continue;if(length(cross(v[b].position-v[a].position,v[c].position-v[a].position))<=1e-10f)continue;clean.insert(clean.end(),{a,b,c});}result=std::move(clean);count=result.size();
  if(count>=3&&count<out.indices[level-1].size()*.95f&&std::isfinite(error)){
   out.indices[level]=std::move(result);out.error[level]=std::max({error,out.error[level-1],span*.001f});
  }else{out.indices[level]=out.indices[level-1];out.error[level]=out.error[level-1];}
 }
 return out;
}
std::vector<MeshLods> build_mesh_lods_many(const std::vector<const Mesh*>&meshes,size_t worker_limit){
 std::vector<MeshLods> results(meshes.size());
 size_t indices=0;for(auto mesh:meshes){if(!mesh)throw std::invalid_argument("LOD: null mesh");indices+=mesh->indices.size();}
 // Tiny collections cost less than starting workers. The renderer invokes this
 // once per upload collection, never once per frame or inside another pool.
 bounded_cpu_for(meshes.size(),[&](size_t i){results[i]=build_mesh_lods(meshes[i]->vertices,meshes[i]->indices);},indices<16384?1:worker_limit);
 return results;
}
int choose_mesh_lod(const std::array<float,3>&error,float scale,float distance,int height,float projection_y,int previous){
 if(distance<35||height<=0||!std::isfinite(distance)||!std::isfinite(scale)||!std::isfinite(projection_y))return 0;
 previous=std::clamp(previous,0,2);float pixels=std::max(0.f,scale)*height*std::abs(projection_y)*.5f/distance;
 for(int l=2;l>0;l--){float limit=l>previous?1.25f:1.75f;if(error[l]>0&&error[l]*pixels<=limit)return l;}return 0;
}
}
