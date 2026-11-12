#include "render_batches.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
using namespace ofo;
namespace {bool track=false;size_t large_bytes=0,large_allocations=0;}
void* operator new(std::size_t n){if(track&&n>=128*1024){large_bytes+=n;++large_allocations;}if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
Mesh mesh(size_t count,float x){Mesh m;m.vertices.resize(count);m.indices.resize(count);m.transform=translate({x,0,10});m.center={x+.5f,.5f,10};m.radius=1;for(size_t i=0;i<count;i++){auto&v=m.vertices[i];v.position={float(i%3==1),float(i%3==2),0};v.normal={0,0,1};v.color=0xff123456;v.u=.25f;v.lu=.75f;m.indices[i]=uint32_t(i);}return m;}
int main(){try{
 Geometry g;g.materials.resize(1);for(int i=0;i<8;i++)g.meshes.push_back(mesh(12000,float(i*100)));
 track=true;auto singles=build_render_batches(g);track=false;
 std::cout<<"singleton large geometry allocations="<<large_allocations<<" bytes="<<large_bytes<<'\n';
 need(singles.batches.empty()&&singles.unbatched.size()==8,"singleton membership preserved");need(large_allocations==0,"singleton planning must not allocate geometry copies");
 g.meshes.clear();for(int i=0;i<3;i++)g.meshes.push_back(mesh(3000,float(i)));
 large_allocations=large_bytes=0;track=true;auto merged=build_render_batches(g);track=false;
 need(merged.batches.size()==1,"merged membership preserved");auto&b=merged.batches[0];
 std::cout<<"merged large geometry allocations="<<large_allocations<<" bytes="<<large_bytes<<" retained vertices="<<b.mesh.vertices.capacity()<<" indices="<<b.mesh.indices.capacity()<<'\n';
 need(b.mesh.vertices.capacity()==b.mesh.vertices.size()&&b.mesh.indices.capacity()==b.mesh.indices.size(),"batch buffers must retain no geometric growth slack");
 need(large_allocations==1&&large_bytes==9000*sizeof(Vertex),"merged vertex storage allocated exactly once");
 for(size_t i=0;i<9000;i++){auto&src=g.meshes[i/3000];const auto&v=b.mesh.vertices[i];need(b.mesh.indices[i]==i,"triangle remap preserved");need(length(v.position-point(src.transform,src.vertices[i%3000].position))<1e-6f,"world positions preserved");need(v.color==0xff123456&&v.u==.25f&&v.lu==.75f,"lighting/UV attributes preserved");}
 need(merged.report["lossless"]==true&&merged.report["source_triangles"]==3000,"triangle totals preserved");
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
