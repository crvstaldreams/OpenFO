#include "plant_render.hpp"
#include "native_shader_source.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
using namespace ofo;
namespace {
using Register=std::array<float,4>;
struct Machine {
 std::map<std::string,Register> r;
 std::array<Register,96> c{};
 static int component(char x){return std::string("xyzw").find(x);}
 Register read(std::string s){
  bool neg=s[0]=='-';if(neg)s.erase(0,1);
  auto dot=s.find('.',s.rfind(']')==s.npos?0:s.rfind(']'));
  std::string name=s.substr(0,dot),sw=dot==s.npos?"xyzw":s.substr(dot+1);
  Register value{};
  if(name=="c[a0.x]")value=c.at(size_t(r["a0"][0]));
  else if(name[0]=='c')value=c.at(std::stoul(name.substr(1)));
  else value=r[name];
  Register out{};for(int k=0;k<4;k++)out[k]=(neg?-1.f:1.f)*value[component(sw[std::min(size_t(k),sw.size()-1)])];return out;
 }
 void write(std::string s,const Register&value){auto dot=s.find('.');auto name=s.substr(0,dot),mask=dot==s.npos?"xyzw":s.substr(dot+1);for(char k:mask)r[name][component(k)]=value[component(k)];}
 void run(const std::string&assembly){
  std::istringstream source(assembly);std::string line;
  while(std::getline(source,line)){
   std::replace(line.begin(),line.end(),',',' ');std::istringstream tokens(line);std::string op,dst,a,b,d;tokens>>op;
   if(op.empty()||op=="vs.1.1"||op.rfind("dcl_",0)==0)continue;
   tokens>>dst>>a>>b>>d;
   // VS1.1 permits only one distinct vertex input and one constant per instruction.
   std::set<std::string> inputs,constants;for(auto s:{a,b,d}){if(s.empty())continue;if(s[0]=='-')s.erase(0,1);if(s[0]=='v')inputs.insert(s.substr(0,s.find('.')));if(s[0]=='c')constants.insert(s.substr(0,s.find(']')==s.npos?s.find('.'):s.find(']')+1));if(s[0]=='r')assert(std::stoi(s.substr(1))<12);}assert(inputs.size()<=1&&constants.size()<=1);
   Register x=read(a),y{},z{},out{};if(!b.empty())y=read(b);if(!d.empty())z=read(d);
   if(op=="m4x4"){for(int k=0;k<4;k++){auto row=c[std::stoul(b.substr(1))+k];for(int j=0;j<4;j++)out[k]+=x[j]*row[j];}}
   else if(op=="dp3"){float v=0;for(int k=0;k<3;k++)v+=x[k]*y[k];out.fill(v);}
   else for(int k=0;k<4;k++){
    if(op=="mov")out[k]=x[k];else if(op=="add")out[k]=x[k]+y[k];else if(op=="sub")out[k]=x[k]-y[k];else if(op=="mul")out[k]=x[k]*y[k];else if(op=="mad")out[k]=x[k]*y[k]+z[k];else if(op=="max")out[k]=std::max(x[k],y[k]);else if(op=="min")out[k]=std::min(x[k],y[k]);else if(op=="sge")out[k]=x[k]>=y[k]?1.f:0.f;else if(op=="rsq")out[k]=1/std::sqrt(std::abs(x[0]));else if(op=="rcp")out[k]=1/x[0];else assert(false&&"Unsupported instruction");
   }
   write(dst,out);
  }
 }
};
void near(float a,float b){if(std::abs(a-b)>3e-5f){std::cerr<<a<<" != "<<b<<'\n';std::abort();}}
Machine execute(const Vertex&v,const PlantWind&wind,Vec eye){
 Machine m;m.r["v0"]={v.position.x,v.position.y,v.position.z,1};m.r["v1"]={v.normal.x,v.normal.y,v.normal.z,0};m.r["v2"]={float((v.color>>16)&255)/255,float((v.color>>8)&255)/255,float(v.color&255)/255,float(v.color>>24)/255};m.r["v3"]={v.u,v.v,0,0};m.r["v4"]={v.lu,v.lv,0,0};
 for(int k=0;k<4;k++)m.c[k][k]=1;m.c[8]={eye.x,eye.y,eye.z,1};m.c[15]={0,.5f,1,2};m.c[32]={150,1.f/50,64,.00001f};for(int k=0;k<16;k++)m.c[k+64]={wind[k][0],wind[k][1],wind[k][2],0};m.run(plant_wind_assembly());return m;
}
}
int main(){
 static_assert(sizeof(Vertex)==44);assert((plant_render_indices==std::array<uint16_t,6>{0,1,2,0,2,3}));assert(plant_wind_shader==65537);
 // Altering a signed corner, phase, bend sign, fade distance, or UV fails below.
 PlantSprite p;p.position={11,3,-7};p.color={.25f,.5f,.75f};p.width=1.7f;p.height=2.3f;
 for(int phase=0;phase<32;phase++)for(int tile=0;tile<16;tile++)for(float angle:{0.f,.7f,2.9f}){
  p.wind_phase=phase;p.tile=tile;p.direction={std::cos(angle),std::sin(angle)};auto packed=plant_render_vertices(p);
  for(double time:{0.,.001,1.23,4.095,4.096,9.875}){
   auto wind=plant_wind(time);auto reference=plant_quad(p,wind);
   for(int n=0;n<4;n++){
    auto&v=packed[n];assert(v.position.x==p.position[0]&&v.position.y==p.position[1]&&v.position.z==p.position[2]);assert(v.normal.x==p.direction[0]&&v.normal.y==p.direction[1]&&v.normal.z==float(phase&15));assert(v.color==0xff3f7fbf);assert(v.lu==(n==0||n==3?p.width:-p.width));assert(v.lv==(n<2?0:p.height));
    near(v.u,reference.uv[n][0]);near(v.v,reference.uv[n][1]);
    for(float distance:{0.f,90.f,100.f,125.f,150.f,170.f}){
     Vec eye{p.position[0]+distance*.6f,p.position[1]+distance*.8f,p.position[2]};auto m=execute(v,wind,eye);for(int k=0;k<3;k++)near(m.r["oPos"][k],reference.position[n][k]);near(m.r["oPos"][3],1);near(m.r["oD0"][3],plant_opacity(distance,100,150));for(int k=0;k<3;k++)near(m.r["oD0"][k],float((v.color>>(16-8*k))&255)/255);near(m.r["oT0"][0],v.u);near(m.r["oT0"][1],v.v);
    }
   }
  }
 }
 // Replaying a timestamp after advancing the clock produces exactly the same vertices.
 auto v=plant_render_vertices(p)[2];auto first=execute(v,plant_wind(1.234),{});execute(v,plant_wind(9.876),{});auto replay=execute(v,plant_wind(1.234),{});assert(first.r["oPos"]==replay.r["oPos"]);
 std::cout<<"plant packing and VS1.1 expansion match CPU reference across all phases/tiles; replay deterministic\n";
}
