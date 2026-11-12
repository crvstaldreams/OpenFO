#include "render_batches.hpp"
#include "render_world_queues.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <set>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Mesh triangle(float x=0){Mesh m;m.transform=translate({x,0,10});m.vertices.resize(3);m.vertices[0].position={0,0,0};m.vertices[1].position={1,0,0};m.vertices[2].position={0,1,0};for(auto&v:m.vertices){v.normal={0,0,1};v.u=.125f;v.lu=.75f;v.color=0xff123456;}m.indices={0,1,2};m.center={x+.5f,.5f,10};m.radius=1;return m;}
void check_world_queues(const RenderFrustum& frustum){
 struct Packet {
  size_t id; Vec center; float radius=1;
  int prop_index=-1; bool glass=false,water=false;
 };
 struct Batch {std::unique_ptr<Packet> gpu;std::vector<size_t> members;std::vector<int> props;};
 std::vector<std::unique_ptr<Packet>> original;
 std::vector<size_t> singles;
 for(int x=-25;x<=25;++x)for(int z=-25;z<=25;++z){
  const size_t id=original.size();
  original.push_back(std::make_unique<Packet>(Packet{id,{float(x*35),0,float(z*35)}}));
  singles.push_back(id);
 }
 const size_t invalid=original.size();
 original.push_back(std::make_unique<Packet>(Packet{invalid,{std::numeric_limits<float>::quiet_NaN(),0,0}}));
 singles.push_back(invalid);
 const size_t mover=original.size();
 original.push_back(std::make_unique<Packet>(Packet{mover,{1200,0,10},1,3}));
 const size_t glass=original.size();
 original.push_back(std::make_unique<Packet>(Packet{glass,{0,0,10},1,-1,true}));
 const size_t water=original.size();
 original.push_back(std::make_unique<Packet>(Packet{water,{0,0,10},1,-1,false,true}));
 singles.push_back(water);
 std::vector<Batch> batches;
 batches.push_back({std::make_unique<Packet>(Packet{water+1,{1200,0,10},1}),{mover,glass},{3}});
 WorldRenderQueues<Packet,Batch> queues;
 queues.build(original,batches,singles,true);
 std::set<size_t> expected,seen;
 for(auto id:singles)if(id!=water&&frustum.visible(original[id]->center,original[id]->radius))expected.insert(id);
 bool moving=false;
 const auto active=[&](size_t id){return moving&&id==3;};
 const auto record=[&](Packet& packet,size_t){seen.insert(packet.id);};
 auto result=queues.visit(WorldRenderPass::Opaque,&frustum,active,record);
 need(seen==expected,"opaque hierarchy preserves the exact flat-frustum visible set");
 need(result.mesh_tests+result.node_tests<singles.size()*3/4,"off-camera subtrees skip individual tests");
 need(queues.moving().size()==1&&queues.moving().front()==original[mover].get(),"only dynamic original meshes need transforms");
 seen.clear();queues.visit(WorldRenderPass::Glass,&frustum,active,record);
 need(seen.empty(),"held packet does not render an original glass member separately");
 moving=true;original[mover]->center={0,0,10};expected.insert(mover);seen.clear();
 result=queues.visit(WorldRenderPass::Opaque,&frustum,active,record);
 need(seen==expected&&result.fallbacks==1,"out-of-view held packet expands to in-view active prop");
 seen.clear();queues.visit(WorldRenderPass::Glass,&frustum,active,record);
 need(seen==std::set<size_t>{glass},"fallback members use their original render pass");
 seen.clear();queues.visit(WorldRenderPass::Water,&frustum,active,record);
 need(seen==std::set<size_t>{water},"water candidates do not scan or submit other passes");
 std::cout<<"world culling tests "<<result.mesh_tests+result.node_tests<<" vs "<<singles.size()+1<<" flat candidates\n";
}
int main(){try{
 Geometry g;Material mat;mat.texture[0]="A\\B.DDS";g.materials={mat,mat};g.materials[1].texture[0]="a/b.dds";g.meshes={triangle(),triangle(2)};g.meshes[1].material=1;g.meshes[1].prop_index=7;g.meshes[1].captured=true;auto before=g.meshes;
 auto p=build_render_batches(g);need(p.batches.size()==1&&p.unbatched.empty(),"equivalent material mesh batch");auto&b=p.batches[0];need(b.members==std::vector<size_t>({0,1})&&b.props==std::vector<int>({7}),"fallback member and held-prop IDs");need(b.mesh.material==0&&b.mesh.transform==identity()&&b.mesh.prop_index==-1,"first material and world baked transform");need(b.mesh.indices==std::vector<uint32_t>({0,1,2,3,4,5}),"all triangles exact remapping");for(int i=0;i<6;i++){auto&v=b.mesh.vertices[i];auto&src=before[i/3].vertices[i%3];need(length(v.position-point(before[i/3].transform,src.position))<1e-6f,"world positions");need(v.u==src.u&&v.lu==src.lu&&v.color==src.color,"exact UV and colors");need(length(v.position-b.mesh.center)<=b.mesh.radius,"conservative bounds");}need(g.meshes[1].transform==before[1].transform&&g.meshes[1].vertices.size()==3,"source immutable");
 std::vector<bool>active(10,false);std::vector<size_t>originals;size_t batch_draws=0;auto select=[&]{originals.clear();batch_draws=0;return select_render_world(p.batches,p.unbatched,[&](size_t id){return id<active.size()&&active[id];},[&](const auto&){batch_draws++;},[&](size_t id){originals.push_back(id);});};need(select()==0&&batch_draws==1&&originals.empty(),"held scene uses merged batch");active[7]=true;need(select()==1&&batch_draws==0&&originals==b.members,"released prop redraws ALL original batch members without merged duplicate");active[7]=false;active[9]=true;need(select()==0&&batch_draws==1&&originals.empty(),"unrelated active prop cannot disable another batch");active.assign(10,false);need(select()==0&&batch_draws==1,"replay seek initial state restores held batching");
 g.meshes[0].radius=15;p=build_render_batches(g);need(length(g.meshes[0].center-p.batches[0].mesh.center)+15<=p.batches[0].mesh.radius+.001f,"original conservative sphere retained");g.meshes[0].radius=1;
 g.meshes[1].transform=translate({100,0,10});g.meshes[1].center={100,0,10};p=build_render_batches(g);need(p.batches.empty()&&p.unbatched.size()==2,"spatial cells keep singletons original");
 g.meshes={triangle(),triangle(2)};g.materials[1].alpha=1;g.meshes[1].material=1;p=build_render_batches(g);need(p.batches.empty(),"alpha separates batches");g.materials[1].alpha=0;g.materials[1].texture[2]="other";need(build_render_batches(g).batches.empty(),"third texture separates batches");g.materials[1]=g.materials[0];
 need(build_render_batches(g,48,5).batches.empty(),"vertex budget caps batches");g.meshes[0].vertices.resize(7);need(build_render_batches(g,48,6).batches.empty(),"oversize retained unbatched");
 g.meshes={triangle(),triangle()};g.materials[0].shader=25;need(build_render_batches(g).batches.empty(),"water never merged");g.materials[0].shader=2;need(build_render_batches(g).batches.empty(),"object space specular never baked");g.materials[0].shader=0;g.meshes[1].transform[0]=2;need(build_render_batches(g).batches.empty(),"nonuniform transform retained");g.meshes[1]=triangle();g.meshes[1].transform[0]=std::numeric_limits<float>::quiet_NaN();need(build_render_batches(g).batches.empty(),"nonfinite transform retained");
 g.meshes={triangle(),triangle(2)};g.meshes[1].transform=multiply(translate({2,0,10}),rotate_y(.8f));p=build_render_batches(g);need(p.batches.size()==1,"rigid rotation accepted");need(length(p.batches[0].mesh.vertices[3].normal-direction(g.meshes[1].transform,{0,0,1}))<1e-5f,"normal uses world inverse transpose");
 g.meshes={triangle(),triangle(2)};for(auto&m:g.meshes){m.transform=identity();m.billboard=true;for(auto&v:m.vertices){v.position={15,0,10};v.normal={12,5,0};}}p=build_render_batches(g);need(p.batches.size()==1,"leaf batch");for(auto&v:p.batches[0].mesh.vertices){need(v.normal.x==12&&v.normal.y==5,"billboard offsets never normalized");auto center=v.position-v.normal;need(length(center-p.batches[0].mesh.center)+13<=p.batches[0].mesh.radius+.001f,"all camera rotations bounded");}g.meshes[1].billboard=false;need(build_render_batches(g).batches.empty(),"leaf cannot merge into ordinary mesh");
 // Enough distinct merged groups to exercise the parallel bake path.
 Geometry large;large.materials.resize(16);for(size_t i=0;i<16;++i)large.materials[i].texture[0]=std::to_string(i);for(size_t i=0;i<12000;++i){auto m=triangle(float(i%5));m.material=i%16;large.meshes.push_back(std::move(m));}
 auto serial=build_render_batches(large,48,16384,1),parallel=build_render_batches(large,48,16384,4);need(serial.report==parallel.report&&serial.unbatched==parallel.unbatched&&serial.batches.size()==parallel.batches.size(),"parallel batch plan exactly matches serial");for(size_t i=0;i<serial.batches.size();++i){const auto&a=serial.batches[i];const auto&b=parallel.batches[i];need(a.members==b.members&&a.props==b.props&&a.mesh.indices==b.mesh.indices&&a.mesh.vertices.size()==b.mesh.vertices.size(),"parallel member/index ordering");need(a.mesh.center.x==b.mesh.center.x&&a.mesh.center.y==b.mesh.center.y&&a.mesh.center.z==b.mesh.center.z&&a.mesh.radius==b.mesh.radius,"parallel bounds exact");for(size_t j=0;j<a.mesh.vertices.size();++j){const auto&x=a.mesh.vertices[j];const auto&y=b.mesh.vertices[j];need(x.position.x==y.position.x&&x.position.y==y.position.y&&x.position.z==y.position.z&&x.normal.x==y.normal.x&&x.normal.y==y.normal.y&&x.normal.z==y.normal.z&&x.u==y.u&&x.v==y.v&&x.lu==y.lu&&x.lv==y.lv&&x.color==y.color,"parallel baked attributes exact");}}
 auto vp=multiply(perspective(1.2f,16.f/9,.1f,1800),look_at({0,0,0},{0,0,1}));RenderFrustum f(vp,{0,0,0});need(f.visible({0,0,10},1),"front visible");need(!f.visible({0,0,-10},1),"behind culled");need(!f.visible({200,0,10},1),"side culled");need(!f.visible({0,0,660},1),"fog distance culled");need(f.visible({0,0,651},2),"sphere intersecting distance retained");need(f.visible({0,0,0},1),"near intersecting sphere retained");need(f.visible({std::numeric_limits<float>::quiet_NaN(),0,0},1),"invalid bounds fail open");
 check_world_queues(f);
 std::cout<<"render batching: exact triangles/attributes, semantics, transforms, bounds, fallback IDs and frustum passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
