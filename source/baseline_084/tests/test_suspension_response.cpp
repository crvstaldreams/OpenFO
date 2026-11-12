#include "simulation.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
using namespace ofo;
namespace {
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Geometry floor(){Geometry g;g.materials.push_back({"road"});Mesh m;m.vertices={{{-1000,0,-1000}},{{1000,0,-1000}},{{1000,0,1000}},{{-1000,0,1000}}};m.indices={0,2,1,0,3,2};g.meshes.push_back(m);return g;}
}
// Third optional argument is a 073 baseline CSV produced by this same executable.
// --capture records an unchanged build even if its settling/load gate fails.
int main(int argc,char**argv){try{need(argc>1,"original resource directory required");Resources r(argv[1]);bool capture=argc>3&&std::string(argv[3])=="--capture";std::ifstream baseline;if(argc>3&&std::string(argv[3])!="--capture")baseline.open(argv[3]);if(argc>3&&!capture)need(bool(baseline),"baseline file must exist");if(baseline){std::string header;std::getline(baseline,header);}std::cout<<"car,height_m,clearance_m,load_weight,settle_range_m,min_tread_m,max_tread_m\n"<<std::fixed<<std::setprecision(6);for(int id=1;id<=16;++id){auto c=load_vehicle(r,id,argc>2?argv[2]:"");Simulation s(floor(),c,translate({0,1,0}));float lo=1000,hi=-1000;for(int i=0;i<1600;++i){s.step({});if(i>=1500){lo=std::min(lo,s.state().car[13]);hi=std::max(hi,s.state().car[13]);}}auto p=s.state();float load=0,min_tread=1000,max_tread=-1000,clearance=1000;for(int i=0;i<4;++i){need(p.wheel_contacts[i].grounded,"all stock wheels support the settled car");need(p.suspension[i]>=c.wheels[i].min&&p.suspension[i]<=c.wheels[i].max,"suspension stays inside authored travel");load+=p.wheel_load[i];float bottom=1000;auto&w=c.wheels[i];if(w.collision_vertices.empty())bottom=point(p.wheel_mounts[i],{0,-w.radius,0}).y;else for(auto v:w.collision_vertices)bottom=std::min(bottom,point(p.wheel_mounts[i],v).y);min_tread=std::min(min_tread,bottom);max_tread=std::max(max_tread,bottom);}
 for(float x:{c.lo.x,c.hi.x})for(float z:{c.lo.z,c.hi.z})clearance=std::min(clearance,point(p.car,{x,c.lo.y,z}).y);
 std::cout<<id<<','<<p.car[13]<<','<<clearance<<','<<load/(c.mass*9.81f)<<','<<hi-lo<<','<<min_tread<<','<<max_tread<<'\n';if(!capture)need(std::abs(load/(c.mass*9.81f)-1)<.01f,"static support must balance native weight");if(!capture)need(hi-lo<.003f,"stock suspension settles without chatter");need(min_tread>-.012f&&max_tread<.012f,"visual/native tire envelope stays on actual floor");need(clearance>.025f,"chassis has positive ground clearance");if(baseline){std::string row;need(bool(std::getline(baseline,row)),"baseline includes all 16 cars");for(char&ch:row)if(ch==',')ch=' ';std::istringstream in(row);int old_id;float old_y,old_clearance,old_weight,old_jitter;in>>old_id>>old_y>>old_clearance>>old_weight>>old_jitter;need(old_id==id,"baseline car ordering");if(old_jitter<.003f){need(std::abs(p.car[13]-old_y)<.015f,"settled ride height matches stock reference");need(std::abs(clearance-old_clearance)<.015f,"stock underbody clearance matches reference");}else std::cerr<<"car "<<id<<" baseline does not settle ("<<old_jitter<<"m); absolute physical gates checked\n";}
 if(argc<=3||std::string(argv[3])!="--capture")need(s.diagnostics().at("handling").value("suspension_lift_m",0.f)==.05f,"5cm declared suspension lift adapter");}
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
