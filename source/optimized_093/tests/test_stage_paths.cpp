#include "assets.hpp"
#include "file_io.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace ofo;
namespace {
void need(bool b,const char*msg){if(!b)throw std::runtime_error(msg);}
struct Fixture {
 Bytes bytes;
 template<class T>void put(T value){auto p=reinterpret_cast<const uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
 void u(uint32_t v){put(v);}void f(float v){put(v);}void str(const std::string&v){bytes.insert(bytes.end(),v.begin(),v.end());bytes.push_back(0);}void zeros(size_t n){bytes.insert(bytes.end(),n,0);}void matrix(){for(float v:identity())f(v);}
 Fixture(){
  u(0x10004);u(1);u(0x4354414d);str("prop");u(0);zeros(8);u(0);zeros(100);str("");str("");str("");
  u(1);u(1);u(0);u(3);u(12);u(2);for(int i=0;i<3;i++){f(float(i));f(0);f(0);}
  u(1);u(0);u(0);u(3);u(2);u(1);u(4);u(3);zeros(24);u(1);u(0);u(0);
  u(1);u(0);u(0);u(0);u(0); // static batch
  u(0);u(0);u(0);matrix(); // tree palette, LODs, trees, collision matrix
  u(1);u(0x444f4d42);u(4);str("box");zeros(12);f(1);f(1);f(1);f(1);u(1);u(0);
  u(0);u(0); // dummies, unknown count
  u(8);for(uint32_t i=0;i<8;i++){u(0x4853454d);str("prop_"+std::to_string(i));str("wood_box");u(i<<13);u(0xffffffff);matrix();u(1);u(0);}
 }
};
void masks(){
 auto root=fs::temp_directory_path()/"openfo-stage-mask-tests";fs::remove_all(root);auto file=root/"tracks/test/location/geometry/track_geom.w32";fs::create_directories(file.parent_path());Fixture fixture;write_file_atomic(file,std::string(fixture.bytes.begin(),fixture.bytes.end()));Resources r(root);
 auto full=load_geometry(r,"tracks/test/location/geometry/track_geom.w32");need(full.native_props.size()==8&&full.meshes.size()==9,"no-stage audit retains every placement");
 for(int v=0;v<3;v++){
  auto g=load_geometry(r,full.source,std::string("tracks/test/location/")+char('a'+v));need(g.native_props.size()==4&&g.filtered_compact_instances==4,"stage inclusion mask count");need(g.meshes.size()==5,"excluded props cannot leave visible/collidable meshes");need(g.meshes[0].name=="static_0","world geometry remains");
  for(auto&p:g.native_props){need((p.placement_flags&(0x2000u<<v))!=0,"only native stage inclusion bit accepted");need(p.native_index==std::stoul(p.name.substr(5)),"placement identity stable after filtering");}
 }
 bool rejected=false;try{load_geometry(r,full.source,"tracks/test/other/a");}catch(const std::runtime_error&){rejected=true;}need(rejected,"cross-location stage rejected");fs::remove_all(root);
}
}
int main(){try{for(auto suffix:{"a","a/","a\\","a/./"}){auto s=std::string("tracks/forest/forest1/")+suffix;auto actual=stage_geometry(s);if(actual!="tracks/forest/forest1/geometry/track_geom.w32")throw std::runtime_error("Stage geometry for "+s+": "+actual);if(clean_path(s)!="tracks/forest/forest1/a")throw std::runtime_error("Stage canonicalization retains trailing separator");}masks();std::cout<<"Native stage paths and placement masks passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
