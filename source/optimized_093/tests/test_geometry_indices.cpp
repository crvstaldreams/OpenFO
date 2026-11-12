#include "assets.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Native BGM/W32 indices address the shared vertex stream, even when a
// surface starts partway through it. Some absolute indices can be below nv.
namespace {
struct Fixture {
 ofo::Bytes bytes;
 template<class T>void put(T value){auto p=reinterpret_cast<const uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
 void u(uint32_t value){put(value);}void f(float value){put(value);}
 void str(const char*value){while(*value)bytes.push_back(*value++);bytes.push_back(0);}
 void zeros(size_t n){bytes.insert(bytes.end(),n,0);}
 void matrix(){for(float x:ofo::identity())f(x);}
 Fixture(const std::vector<uint16_t>&indices,uint32_t base=3){
  u(0x10004);u(1);u(0x4354414d);str("surface");u(0);zeros(8);u(0);zeros(100);str("");str("");str("");
  u(2);u(1);u(0);u(base+6);u(12);u(2);
  for(uint32_t i=0;i<base+6;i++){f(float(i));f(float(i%2));f(0);}
  u(2);u(0);u(uint32_t(indices.size()));for(auto index:indices)put(index);
  u(1);u(0);u(0);u(6);u(2);u(2);u(4);u(uint32_t(indices.size()));zeros(24);u(2);u(0);u(base*12);u(1);u(0);
  u(1);u(0x444f4d42);u(0);str("model");zeros(28);u(1);u(0);
  u(1);u(0x4853454d);str("body");str("");u(0);u(0);matrix();u(1);u(0);u(0);
 }
};
}
int main(){try{
 auto root=ofo::fs::temp_directory_path()/"openfo-geometry-index-tests";ofo::fs::create_directories(root);
 auto load=[&](const Fixture&fixture){std::ofstream out(root/"fixture.bgm",std::ios::binary);out.write(reinterpret_cast<const char*>(fixture.bytes.data()),fixture.bytes.size());out.close();return ofo::load_geometry(ofo::Resources(root),"fixture.bgm");};
 auto mesh=load(Fixture({3,4,5,6,7,8})).meshes.at(0);
 if(mesh.indices!=std::vector<uint32_t>{0,1,2,3,4,5}){for(auto i:mesh.indices)std::cerr<<i<<" ";std::cerr<<"\n";}
 if(mesh.indices!=std::vector<uint32_t>{0,1,2,3,4,5})throw std::runtime_error("absolute shared-stream indices must all be rebased, including values below surface vertex count");
 if(mesh.vertices.front().position.x!=3||mesh.vertices.back().position.x!=8)throw std::runtime_error("surface vertex offset");
 if(load(Fixture({0,1,2,3,4,5},0)).meshes.at(0).indices!=std::vector<uint32_t>{0,1,2,3,4,5})throw std::runtime_error("zero-base indices");
 bool rejected=false;try{load(Fixture({2,4,5,6,7,8}));}catch(const std::runtime_error&){rejected=true;}
 if(!rejected)throw std::runtime_error("index before surface base must be rejected");
 ofo::fs::remove_all(root);std::cout<<"geometry shared-stream indices passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
