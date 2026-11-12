#include "menu_smoke.hpp"
#include <iostream>
#include <stdexcept>
int main(){auto a=ofo::menu_smoke_vertices({1,2,3},{1,0,0},{0,1,0},1.25);if(a.size()!=144)throw std::runtime_error("24 source smoke quads");for(auto&v:a)if(!std::isfinite(v.position.x)||v.u<0||v.u>1||v.v<0||v.v>1||(v.color>>24)>82)throw std::runtime_error("smoke bounds");std::cout<<"source title-smoke cadence and native atlas UV passed\n";}
