#include "captured_scene.hpp"
#include "mesh_lod.hpp"
#include "render_batches.hpp"
#include <chrono>
#include <iostream>
#include <thread>
using namespace ofo;
using Clock=std::chrono::steady_clock;
namespace {
void require(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
bool same_vertex(const Vertex&a,const Vertex&b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.normal.x==b.normal.x&&a.normal.y==b.normal.y&&a.normal.z==b.normal.z&&a.u==b.u&&a.v==b.v&&a.lu==b.lu&&a.lv==b.lv&&a.color==b.color;}
void same_batches(const RenderBatchPlan&a,const RenderBatchPlan&b){require(a.report==b.report&&a.unbatched==b.unbatched&&a.batches.size()==b.batches.size(),"Batch plan mismatch");for(size_t i=0;i<a.batches.size();++i){auto&x=a.batches[i];auto&y=b.batches[i];require(x.members==y.members&&x.props==y.props&&x.mesh.indices==y.mesh.indices&&x.mesh.vertices.size()==y.mesh.vertices.size(),"Batch order mismatch");require(x.mesh.center.x==y.mesh.center.x&&x.mesh.center.y==y.mesh.center.y&&x.mesh.center.z==y.mesh.center.z&&x.mesh.radius==y.mesh.radius,"Batch bounds mismatch");for(size_t j=0;j<x.mesh.vertices.size();++j)require(same_vertex(x.mesh.vertices[j],y.mesh.vertices[j]),"Batch vertex mismatch");}}
}
int main(int argc,char**argv){try{
 if(argc<3)throw std::runtime_error("Pass package directory and original resource directory");
 Resources resources(argv[2]);resources.mount(fs::path(argv[1])/"capture_assets","capture_assets");
 auto geometry=load_geometry(resources,"tracks/forest/forest1/geometry/track_geom.w32");
 auto imported=append_captured_scene(geometry,resources);
 std::vector<const Mesh*> meshes;meshes.reserve(geometry.meshes.size());for(const auto&m:geometry.meshes)meshes.push_back(&m);
 auto start=Clock::now();auto serial_batches=build_render_batches(geometry,48,16384,1);double serial_batch_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 start=Clock::now();auto parallel_batches=build_render_batches(geometry,48,16384,0);double parallel_batch_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();same_batches(serial_batches,parallel_batches);
 // Use actual merged packets, matching renderer preparation rather than duplicating
 // every captured object instance. Exact output comparison includes error values.
 meshes.clear();for(const auto&b:serial_batches.batches)if(!b.mesh.billboard)meshes.push_back(&b.mesh);for(auto i:serial_batches.unbatched)if(!geometry.meshes[i].billboard)meshes.push_back(&geometry.meshes[i]);
 start=Clock::now();auto serial_lods=build_mesh_lods_many(meshes,1);double serial_lod_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 start=Clock::now();auto parallel_lods=build_mesh_lods_many(meshes,0);double parallel_lod_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 require(serial_lods.size()==parallel_lods.size(),"LOD count mismatch");size_t triangles=0;for(size_t i=0;i<serial_lods.size();++i){require(serial_lods[i].indices==parallel_lods[i].indices&&serial_lods[i].error==parallel_lods[i].error,"LOD output mismatch");triangles+=serial_lods[i].indices[0].size()/3;}
 std::cout<<Json{{"scene","Forest1 native + captured"},{"source_meshes",geometry.meshes.size()},{"render_lod_jobs",meshes.size()},{"render_triangles",triangles},{"hardware_concurrency",std::thread::hardware_concurrency()},{"worker_cap",8},{"serial_batch_ms",serial_batch_ms},{"parallel_batch_ms",parallel_batch_ms},{"serial_lod_ms",serial_lod_ms},{"parallel_lod_ms",parallel_lod_ms},{"batch_outputs_exact",true},{"lod_indices_and_errors_exact",true},{"measurement","single host CPU preprocessing run; not FPS or GPU timing"},{"gpu_backend_measured",false},{"fps_measured",false}}.dump(2)<<'\n';
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
