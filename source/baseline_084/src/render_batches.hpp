#pragma once
#include "assets.hpp"
namespace ofo {
struct RenderBatch {Mesh mesh;std::vector<size_t> members;std::vector<int> props;};
struct RenderBatchPlan {std::vector<RenderBatch> batches;std::vector<size_t> unbatched;Json report;};
// Render-only copies. A changed prop requires drawing every original member of its batch.
RenderBatchPlan build_render_batches(const Geometry&,float cell_size=48,size_t max_vertices=16384,size_t worker_limit=0);
// One decision path for rendering and tests: any active prop expands the entire
// merged packet into its original members; initial replay state can merge again.
template<class Batch,class Active,class DrawBatch,class DrawOriginal>
size_t select_render_world(const std::vector<Batch>&batches,const std::vector<size_t>&singles,Active active,DrawBatch batch,DrawOriginal original){
 for(auto i:singles)original(i);size_t fallbacks=0;
 for(const auto&b:batches){bool moving=false;for(int id:b.props)if(id>=0&&active(size_t(id))){moving=true;break;}if(moving){++fallbacks;for(auto i:b.members)original(i);}else batch(b);}
 return fallbacks;
}
// D3D clip convention: -w <= x,y <= w, 0 <= z <= w. Invalid bounds fail open.
class RenderFrustum {
 std::array<std::array<float,4>,6> planes_{};Vec eye_;float far_;
public:
 RenderFrustum(const Mat&vp,Vec eye,float far=650);
 bool visible(Vec center,float radius) const;
};
struct RenderSphere {Vec center;float radius;};
struct RenderCullStats {
 size_t mesh_tests=0,node_tests=0,rejected=0,fallbacks=0;
};
// A load-time hierarchy of immutable draw-packet bounds. Node rejection is
// conservative and surviving leaf spheres still use the original test.
class RenderSpatialIndex {
 struct Node {RenderSphere sphere;size_t begin,end,right=0;};
 std::vector<RenderSphere> spheres_;
 std::vector<size_t> order_,unbounded_;
 std::vector<Node> nodes_;
 size_t build_node(size_t begin,size_t end);
 template<class Visitor>
 void visit_node(size_t index,const RenderFrustum* frustum,RenderCullStats& stats,Visitor& visit)const{
  const auto& node=nodes_[index];
  if(frustum&&node.right){
   ++stats.node_tests;
   if(!frustum->visible(node.sphere.center,node.sphere.radius)){
    stats.rejected+=node.end-node.begin;
    return;
   }
  }
  if(node.right){
   visit_node(index+1,frustum,stats,visit);
   visit_node(node.right,frustum,stats,visit);
  }else{
   for(size_t i=node.begin;i<node.end;++i){
    const auto id=order_[i];
    const auto sphere=spheres_[id];
    ++stats.mesh_tests;
    if(!frustum||frustum->visible(sphere.center,sphere.radius))visit(id);
    else ++stats.rejected;
   }
  }
 }
public:
 void build(std::vector<RenderSphere> spheres);
 template<class Visitor>
 void visit(const RenderFrustum* frustum,RenderCullStats& stats,Visitor visit)const{
  if(!nodes_.empty())visit_node(0,frustum,stats,visit);
  for(auto id:unbounded_){++stats.mesh_tests;visit(id);}
 }
};
}
