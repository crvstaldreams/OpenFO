#include "simulation.hpp"
#include "replay.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*why){if(!b)throw std::runtime_error(why);}
int main(int argc,char**argv){try{Resources r(argv[1]);auto cfg=load_vehicle(r,3);Geometry g;g.materials.push_back({});Mesh floor;for(auto p:{Vec{-100,0,-100},Vec{100,0,-100},Vec{100,0,100},Vec{-100,0,100}}){Vertex v;v.position=p;floor.vertices.push_back(v);}floor.indices={0,2,1,0,3,2};g.meshes.push_back(floor);Mesh wall;for(auto p:{Vec{-20,0,30},Vec{20,0,30},Vec{20,6,30},Vec{-20,6,30}}){Vertex v;v.position=p;wall.vertices.push_back(v);}wall.indices={0,2,1,0,3,2};g.meshes.push_back(wall);
Simulation sim(g,cfg,translate({0,1,0}));for(int i=0;i<300;i++)sim.step({});need(sim.state().headlight_health==std::array<float,2>{1,1},"settling must not break lamps");for(int i=0;i<2000;i++){Input in;in.throttle=1;sim.step(in);need(sim.state().headlight_health==std::array<float,2>{1,1},"front impact cannot disable lamps");}
auto s=sim.state();std::cout<<"front wall: health="<<s.headlight_health[0]<<","<<s.headlight_health[1]<<" damage="<<s.damage<<" speed="<<s.speed<<'\n';need(s.damage>.1f,"physical crash fixture produced real damage");need(s.headlight_health==std::array<float,2>{1,1},"lamps remain on after crash");auto saved=s;sim.recover();need(sim.state().headlight_health==saved.headlight_health,"recovery keeps lamps powered");need(saved.headlight_health==s.headlight_health,"snapshot remains immutable");
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
