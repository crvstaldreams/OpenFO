#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace ofo {
using MenuCameraPoint=std::array<float,3>;
// Original MenuScene evaluator (0x467820): uniform Catmull-Rom, with
// reflected end neighbours, two segments for three authored BED points.
inline MenuCameraPoint menu_camera_spline(const std::array<MenuCameraPoint,3>& p,double t){
 if(!std::isfinite(t))t=.5;t=std::clamp(t,0.,1.);double q=t*2;int segment=std::min(1,int(q));float u=float(q-segment);MenuCameraPoint out{};
 for(int k=0;k<3;++k){float a=segment?p[0][k]:2*p[0][k]-p[1][k],b=p[segment][k],c=p[segment+1][k],d=segment?2*p[2][k]-p[1][k]:p[2][k];
 out[k]=b+.5f*u*((c-a)+u*(2*a-5*b+4*c-d+u*(3*(b-c)+d-a)));}return out;
}
// Original MenuScene update (0x465b73): 20-second back-and-forth cycle.
inline double menu_camera_phase(double seconds){
 if(!std::isfinite(seconds))return .5;
 return .5+.5*std::sin(std::fmod(seconds,20.)*.3141592741012573);
}
}
