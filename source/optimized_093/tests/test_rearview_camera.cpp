#include "rearview_camera.hpp"
#include <cassert>
#include <iostream>
using namespace ofo;
int main(){Vec lo{-1,0,-2},hi{1,1.5f,2};for(float yaw:{0.f,.7f,3.14159265f}){auto car=multiply(translate({10,3,20}),rotate_y(yaw));auto c=rearview_camera(car,lo,hi);Vec f=direction(car,{0,0,1});assert(dot(unit(c.target-c.eye),f)<-.999f);assert(dot(c.eye-point(car,{}),f)<-2.f);auto ahead=point(c.view,c.target);assert(ahead.z>50&&std::abs(ahead.x)<.001f);assert(std::abs(2*std::atan(1/c.projection[0])*180/3.14159265f-100)<.01f);assert(c.projection[0]>0&&std::abs(c.projection[5]/c.projection[0]-4)<.001f);for(float v:c.view)assert(std::isfinite(v));}auto rolled=rearview_camera(rotate_x(1.5707963f),lo,hi);for(float v:rolled.view)assert(std::isfinite(v));std::cout<<"rearview camera rear heading, bumper clearance, projection and degenerate pose passed\n";}
