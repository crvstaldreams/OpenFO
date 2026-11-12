#include "render_batches.hpp"
#include "render_world_queues.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace ofo;
namespace {
void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
Mesh make_spread_mesh(){
 Mesh m;m.name="static_terrain";m.transform=translate({0,0,150});m.center={0,0,150};m.radius=540;
 constexpr int nx=192,ny=32;
 for(int y=0;y<=ny;++y)for(int x=0;x<=nx;++x){
  Vertex v;v.position={float(x*5-480),float(y*2-32),0};v.normal={0,0,1};m.vertices.push_back(v);
 }
 for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
  auto a=uint32_t(y*(nx+1)+x),b=a+1,c=a+nx+1,d=c+1;
  m.indices.insert(m.indices.end(),{a,b,d,a,d,c});
 }
 return m;
}
struct Packet{Vec center{};float radius=0;int prop_index=-1;bool glass=false,water=false;};
struct Batch{std::unique_ptr<Packet> gpu;std::vector<size_t> members;std::vector<int> props;};
}
int main(){try{
 Geometry g;g.materials.resize(1);g.meshes.push_back(make_spread_mesh());
 auto plan=build_render_batches(g);
 need(plan.unbatched.empty()&&plan.batches.size()>2,"large original replaced by independently drawable GPU clusters");
 need(g.meshes.size()==1&&g.meshes[0].solid,"source physics preserved");
 size_t triangles=0;
 std::vector<Batch> batches;
 for(auto& b:plan.batches){
  need(b.members.empty()&&b.props.empty(),"fragments are independent static packets, not moving-prop fallbacks");
  triangles+=b.mesh.indices.size()/3;
  batches.push_back({std::make_unique<Packet>(Packet{b.mesh.center,b.mesh.radius}),{}, {}});
 }
 need(triangles==g.meshes[0].indices.size()/3,"cluster plan preserves all indices");
 WorldRenderQueues<Packet,Batch> queues;
 std::vector<std::unique_ptr<Packet>> originals(1);
 queues.build(originals,batches,{},true);
 const auto vp=multiply(perspective(1.0f,16.f/9,.1f,1800),look_at({0,0,0},{0,0,1}));
 RenderFrustum frustum(vp,{0,0,0},650);
 size_t drawn=0;
 auto stats=queues.visit(WorldRenderPass::Opaque,&frustum,[](size_t){return false;},
  [&](Packet&,size_t){++drawn;});
 need(drawn>0&&drawn<plan.batches.size(),"world queue rejects off-camera clusters before drawing");
 need(stats.rejected>0,"culling statistics include rejected clusters");
 need(queues.moving().empty(),"static cluster packets never update moving props");
 std::cout<<"render cluster integration PASS: "<<plan.batches.size()<<" GPU clusters, "
          <<drawn<<" in frustum, "<<stats.rejected<<" rejected\n";
 }catch(const std::exception& e){std::cerr<<"render cluster integration FAIL: "<<e.what()<<'\n';return 1;}
}
