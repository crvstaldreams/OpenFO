#include "mesh_lod.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
using namespace ofo;
namespace {bool track=false;size_t allocations=0,bytes=0;}
void* operator new(std::size_t n){if(track){allocations++;bytes+=n;}if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 std::vector<Vertex>v(12);std::vector<uint32_t>ix(12);for(size_t i=0;i<v.size();i++){v[i].position={float(i%3),float(i/3),0};ix[i]=uint32_t(i);}
 track=true;auto lod=build_mesh_lods(v,ix);track=false;
 std::cout<<"small mesh LOD allocations="<<allocations<<" bytes="<<bytes<<'\n';
 need(allocations==3&&bytes==3*ix.size()*sizeof(uint32_t),"small meshes must allocate only the three returned original index arrays");
 for(size_t level=0;level<3;level++){need(lod.indices[level]==ix,"small meshes preserve every original triangle");need(lod.error[level]==0,"small meshes retain original LOD choice");}
 v.emplace_back();v.back().lv=std::numeric_limits<float>::infinity();bool rejected=false;try{build_mesh_lods(v,ix);}catch(const std::runtime_error&){rejected=true;}need(rejected,"small-mesh fallback still validates every vertex attribute");
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
