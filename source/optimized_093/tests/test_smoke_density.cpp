#include "engine_effects.hpp"
#include <stdexcept>
#include <iostream>
using namespace ofo;
int main(){EffectParameters p;for(bool menu:{false,true})for(float damage:{.3f,.6f,1.f}){auto v=engine_effect_vertices(p,damage,{},{},{1,0,0},{0,1,0},1.125,menu);size_t limit=menu?8:damage<.5f?5:10;if(v.smoke.size()>limit*6)throw std::runtime_error("smoke count not reduced fourfold");for(const auto&q:v.smoke)if((q.color>>24)>51)throw std::runtime_error("smoke alpha not reduced fourfold");if(!menu&&damage==1&&v.fire.empty())throw std::runtime_error("fire must remain");}std::cout<<"smoke density and opacity reduced fourfold, fire retained\n";}
