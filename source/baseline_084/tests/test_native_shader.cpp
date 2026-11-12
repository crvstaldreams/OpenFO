#include "native_shader_source.hpp"
#include <cassert>
#include <iostream>
#include <sstream>
#include <set>
int main(int n,char**v){
 assert(n==2);ofo::Resources r(v[1]);
 const std::map<int,std::string> expected={{0,"pro_static2x.sha"},{1,"pro_lightmapped.sha"},{2,"pro_lightmapped_spec.sha"},{3,"emu_default_dynamic.sha"},{4,"emu_default_dynamic.sha"},{7,"emu_default_dynamic.sha"},{8,"emu_default_dynamic.sha"},{9,"emu_default_dynamic.sha"},{11,"emu_default_dynamic.sha"},{12,"emu_default_dynamic.sha"},{32,"emu_default_dynamic.sha"},{5,"emu_car_body.sha"},{6,"emu_car_window.sha"},{28,"pro_clouds.sha"},{29,"pro_clouds.sha"},{30,"emu_car_body.sha"},{14,"pro_car_lights_unlit.sha"},{15,"pro_default_static.sha"},{16,"pro_vertexcolor.sha"},{18,"pro_static2x.sha"},{19,"pro_static2x.sha"},{20,"pro_tree_branch.sha"},{21,"pro_static2x.sha"},{22,"pro_default_static.sha"},{23,"pro_sunflare.sha"},{25,"emu_water.sha"},{27,"pro_default_static.sha"},{31,"pro_vertexcolor.sha"}};
 for(auto [id,file]:expected){auto f=ofo::native_effect(r,id);if(f!="shader/"+file){std::cerr<<"wrong shader selection "<<id<<": "<<f<<'\n';return 1;}auto a=ofo::native_assembly(r.text(f));assert(a.first.find("m4x4")!=a.first.npos);assert(a.first.find("/*")==a.first.npos);if(id==1||id==2){assert(a.first.find("v4.xy")!=a.first.npos);assert(a.second.find("t1")!=a.second.npos);}// Every selected original program must fit the mesh declaration v0..v4.
 std::regex input(R"(\bv([0-9]+)\b)");for(auto i=std::sregex_iterator(a.first.begin(),a.first.end(),input);i!=std::sregex_iterator();++i)assert(std::stoi((*i)[1])<=4);
 if(id==5||id==6||id==30)assert(a.second.empty());
 std::cout<<id<<' '<<f<<'\n';}
 // Shader model 1.1 has one input-register read port per instruction.
 std::istringstream instructions(ofo::tree_billboard_assembly());std::string line;while(std::getline(instructions,line)){std::set<std::string> inputs;std::regex reg(R"(\bv[0-9]+\b)");for(auto i=std::sregex_iterator(line.begin(),line.end(),reg);i!=std::sregex_iterator();++i)inputs.insert(i->str());if(inputs.size()>1){std::cerr<<"vs1.1 input read-port violation: "<<line<<'\n';return 1;}}
 auto billboard=ofo::native_constants("openfo/tree_billboard",ofo::identity(),ofo::look_at({0,0,0},{1,0,0}),ofo::identity(),{});
 assert(billboard[10*4+2]==-1 && billboard[11*4+1]==1);
 assert(ofo::native_effect(r,ofo::tree_billboard_shader)=="shader/pro_static2x.sha");
 // Do not silently drop glow passes or select shaders requiring unsupported cubemaps/bones.
 for(int id:{10,13,17,24,26,33,34,35,36,37,38,39,999}){if(!ofo::native_effect(r,id).empty()){std::cerr<<"unexpected selection "<<id<<" "<<ofo::native_effect(r,id)<<std::endl;return 1;}}
 for(auto source:{"asm{vs.3.0}","asm{vs.1.1} asm{vs.1.1}","asm{vs.1.1} asm{ps.1.1} asm{ps.1.1}"}){bool bad=false;try{ofo::native_assembly(source);}catch(...){bad=true;}assert(bad);}
 // Specular SHA evaluates eye/light in object coordinates, including translated/scaled props.
 auto world=ofo::translate({10,2,3});world[0]=2;
 auto view=ofo::look_at({14,4,-7},{14,4,-6});
 auto spec=ofo::native_constants("shader/pro_lightmapped_spec.sha",world,view,ofo::identity(),{0,-1,0});
 assert(spec[32*4]==5 && spec[32*4+3]==0);
 assert(std::abs(spec[8*4]-2)<1e-5 && std::abs(spec[8*4+1]-2)<1e-5 && std::abs(spec[8*4+2]+10)<1e-5);
 assert(spec[9*4+2]==1 && spec[20*4+1]==1 && spec[22*4]>.0f);
 auto water=ofo::native_constants("shader/emu_water.sha",ofo::identity(),view,ofo::identity(),{},3.5f);
 for(int i=0;i<4;i++){assert(water[14*4+i]==3.5f);assert(water[94*4+i]==2);}
 assert(water[64*4]==.06f && water[64*4+2]==.01f && water[64*4+3]==-.008f);
 assert(water[95*4]==0 && water[95*4+1]==.4f && water[95*4+3]==.5f);
 // World-space eye must follow both camera translation and rotation. Window
 // normals/positions are transformed by c4 inside the original EMU assembly.
 auto glass=ofo::native_constants("shader/emu_car_window.sha",world,view,ofo::identity(),{});
 assert(std::abs(glass[8*4]-14)<1e-5 && std::abs(glass[8*4+1]-4)<1e-5 && std::abs(glass[8*4+2]+7)<1e-5);
 assert(glass[17*4]==.55f && glass[32*4]==.375f && glass[32*4+1]==.625f && glass[32*4+3]==0);
 auto rotated=ofo::look_at({8,3,12},{9,3,12});
 glass=ofo::native_constants("shader/emu_car_window.sha",ofo::identity(),rotated,ofo::identity(),{});
 assert(std::abs(glass[8*4]-8)<1e-5 && std::abs(glass[8*4+1]-3)<1e-5 && std::abs(glass[8*4+2]-12)<1e-5);
 auto cloud=ofo::native_constants("shader/pro_clouds.sha",ofo::identity(),view,ofo::identity(),{},625);
 assert(std::abs(cloud[32*4]-.25f)<1e-5 && std::abs(cloud[32*4+1]-.3125f)<1e-5);
 assert(cloud[32*4+2]==0 && cloud[32*4+3]==0);
 assert(ofo::native_effect_limit(r,10).find("additive glow")!=std::string::npos);
 assert(ofo::native_effect_limit(r,26).find("bone palette")!=std::string::npos);
 assert(ofo::native_effect_limit(r,24).find("four-component")!=std::string::npos);
 // Exhaustively catalogue all 40 material-library entries. Every unsupported
 // entry must explain a real missing binding/pass, rather than silently vanish.
 int supported=0;for(uint32_t id=0;id<40;id++){
  if(!ofo::native_effect(r,id).empty())++supported;
  else assert(!ofo::native_effect_limit(r,id).empty());
 }
 assert(supported==28);
 auto flare=ofo::native_assembly(r.text(ofo::native_effect(r,23)));
 assert(flare.first.find("oT1.xy")!=std::string::npos && flare.second.find("t1")!=std::string::npos);
 auto branch=ofo::native_constants("shader/pro_tree_branch.sha",ofo::identity(),view,ofo::identity(),{});
 for(int i=0;i<4;i++)assert(branch[32*4+i]==1);
}
