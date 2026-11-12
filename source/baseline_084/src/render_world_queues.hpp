#pragma once
#include "render_batches.hpp"

namespace ofo {
enum class WorldRenderPass {Opaque, Glass, Water};

// Renderers own GPU packets. Rebuild these non-owning queues after replacing
// a scene, before any draw or transform update can use the new packets.
template<class GPU,class Batch>
class WorldRenderQueues {
 struct Entry {GPU* mesh; const Batch* batch; size_t order;};
 std::array<std::vector<Entry>,3> passes_;
 std::vector<Entry> static_opaque_;
 std::vector<GPU*> moving_;
 RenderSpatialIndex index_;
 const std::vector<std::unique_ptr<GPU>>* originals_=nullptr;

 static WorldRenderPass pass(const GPU& mesh){
  return mesh.water?WorldRenderPass::Water:mesh.glass?WorldRenderPass::Glass:WorldRenderPass::Opaque;
 }
public:
 void build(const std::vector<std::unique_ptr<GPU>>& originals,const std::vector<Batch>& batches,
            const std::vector<size_t>& singles,bool spatial){
  originals_=&originals;
  for(auto& list:passes_)list.clear();
  static_opaque_.clear();moving_.clear();
  for(const auto& mesh:originals)if(mesh&&mesh->prop_index>=0)moving_.push_back(mesh.get());

  size_t order=0;
  std::vector<RenderSphere> bounds;
  auto append=[&](GPU* mesh){
   if(!mesh)return;
   const auto kind=pass(*mesh);
   if(spatial&&kind==WorldRenderPass::Opaque&&mesh->prop_index<0){
    bounds.push_back({mesh->center,mesh->radius});
    static_opaque_.push_back({mesh,nullptr,order++});
   }else passes_[size_t(kind)].push_back({mesh,nullptr,order++});
  };
  for(auto id:singles)append(originals[id].get());
  for(const auto& batch:batches){
   if(batch.props.empty()){
    append(batch.gpu.get());
    continue;
   }
   // The held packet may have different pass flags than an original member.
   // Register every relevant pass, then make one held/moving decision per pass.
   std::array<bool,3> kinds{};
   kinds[size_t(pass(*batch.gpu))]=true;
   for(auto id:batch.members)if(originals[id])kinds[size_t(pass(*originals[id]))]=true;
   for(size_t kind=0;kind<kinds.size();++kind)
    if(kinds[kind])passes_[kind].push_back({batch.gpu.get(),&batch,order});
   order+=batch.members.size()+1;
  }
  index_.build(std::move(bounds));
 }
 const std::vector<GPU*>& moving()const{return moving_;}

 template<class Active,class Visitor>
 RenderCullStats visit(WorldRenderPass kind,const RenderFrustum* frustum,Active active,Visitor visible)const{
  RenderCullStats stats;
  if(kind==WorldRenderPass::Opaque)index_.visit(frustum,stats,[&](size_t id){
   const auto& entry=static_opaque_[id];
   visible(*entry.mesh,entry.order);
  });
  auto test=[&](GPU* mesh,size_t order){
   if(!mesh||pass(*mesh)!=kind)return;
   ++stats.mesh_tests;
   if(!frustum||frustum->visible(mesh->center,mesh->radius))visible(*mesh,order);
   else ++stats.rejected;
  };
  for(const auto& entry:passes_[size_t(kind)]){
   const auto* batch=entry.batch;
   if(!batch||std::none_of(batch->props.begin(),batch->props.end(),[&](int id){return id>=0&&active(size_t(id));})){
    test(entry.mesh,entry.order);
    continue;
   }
   ++stats.fallbacks;
   size_t order=entry.order+1;
   for(auto id:batch->members)test((*originals_)[id].get(),order++);
  }
  return stats;
 }
};
}
