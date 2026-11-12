#include "assets.hpp"
#include "native_material_profile.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
using namespace ofo;
struct HeaderReader{const Bytes&b;size_t p=0;uint32_t u(){if(p+4>b.size())throw std::runtime_error("material header truncated");uint32_t v;std::memcpy(&v,b.data()+p,4);p+=4;return v;}void skip(size_t n){if(n>b.size()-p)throw std::runtime_error("material header span");p+=n;}std::string str(){std::string s;for(;;){if(p>=b.size()||s.size()>65536)throw std::runtime_error("material string");auto c=b[p++];if(!c)return s;s+=char(c);}}};
int main(int argc,char**argv){try{if(argc<3)throw std::runtime_error("data report.json");Resources r(argv[1]);Json groups=Json::object(),errors=Json::array();size_t files=0,materials=0;
for(auto&e:fs::recursive_directory_iterator(argv[1]))if(e.is_regular_file()){
 auto ext=lower(e.path().extension().string());if(ext!=".bgm"&&ext!=".w32")continue;auto path=fs::relative(e.path(),argv[1]).generic_string();
 try{auto bytes=r.read(path);HeaderReader h{bytes};auto version=h.u();if(version!=0x10004&&version!=0x10005)continue;auto n=h.u();if(n>10000)throw std::runtime_error("material count");++files;
 for(unsigned j=0;j<n;j++){if(h.u()!=0x4354414d)throw std::runtime_error("MATC");auto name=h.str();auto alpha=h.u();h.skip(8);auto shader=h.u();h.skip(100);std::string tex[3];for(auto&t:tex)t=h.str();auto key=std::to_string(shader);auto profile=native_material_profile(shader);auto&g=groups[key];if(g.is_null())g={{"role",profile.role},{"mode",profile.mode},{"scene_material",profile.scene},{"materials",0},{"with_Tex2",0},{"alpha_flags",Json::object()},{"examples",Json::array()}};g["materials"]=g["materials"].get<int>()+1;if(!tex[2].empty())g["with_Tex2"]=g["with_Tex2"].get<int>()+1;auto ak=std::to_string(alpha);g["alpha_flags"][ak]=g["alpha_flags"].value(ak,0)+1;
 if(g["examples"].size()<5){Json slots=Json::array();for(auto&t:tex){auto resolved=r.texture(path,t);slots.push_back({{"authored",t},{"resolved",resolved},{"exists",!resolved.empty()&&r.exists(resolved)}});}g["examples"].push_back({{"geometry",path},{"material",name},{"textures",slots}});}++materials;
 }
 }catch(const std::exception&x){errors.push_back({{"path",path},{"error",x.what()}});}
}
Json ids=Json::array();auto library=bed_file(r,"shader/shaderlib_pro.ini").at("Shaders");for(unsigned i=0;i<40;i++){auto p=native_material_profile(i);auto entry=library.at(std::to_string(i+1));ids.push_back({{"id",i},{"effect",entry.at("EffectFile")},{"role",p.role},{"mode",p.mode},{"scene_material",p.scene}});}Json out={{"geometry_files",files},{"materials",materials},{"observed",groups},{"shader_library",ids},{"errors",errors},{"scope","Original material headers/slots and semantic classification; not Windows visual verification"}};std::ofstream(argv[2])<<out.dump(2)<<'\n';std::cout<<files<<" geometry files, "<<materials<<" materials, "<<groups.size()<<" observed IDs, "<<errors.size()<<" errors\n";return errors.empty()?0:1;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
