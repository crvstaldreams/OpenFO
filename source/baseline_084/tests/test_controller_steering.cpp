#include "controls.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){using ofo::controller_steering;assert(controller_steering(.2f,.3f,1)==0);assert(controller_steering(.2f,.1f,1)>.1f);assert(controller_steering(-.2f,.1f,1)<-.1f);assert(controller_steering(1,.2f,2)==1);assert(controller_steering(-1,.2f,2)==-1);assert(std::abs(controller_steering(.5f,.2f,2)-.75f)<1e-6);std::cout<<"controller deadzone and sensitivity affect actual steering\n";}
