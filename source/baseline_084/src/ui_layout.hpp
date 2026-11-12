#pragma once
#include <algorithm>
#include <cmath>
#include <chrono>
namespace ofo {
inline double ui_monotonic_seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// Retargetable exponential easing: repeated input starts at the current visible
// value, and a changed page initializes immediately rather than flying across it.
struct UiMotion {
 mutable double time=-1;mutable float value=0;mutable int context=-1;
 float update(float target,double now,int page,float response=15)const{
  if(time<0||context!=page||now<time){value=target;context=page;time=now;return value;}
  double dt=std::clamp(now-time,0.,.1);time=now;
  value+=(target-value)*float(1-std::exp(-response*dt));
  if(std::abs(target-value)<.02f)value=target;return value;
 }
};

// Uniform sizing; anchors use the extra logical space instead of stretching art.
struct HudLayout {
 float scale=0,extra_x=0,extra_y=0;
 HudLayout(int width,int height){if(width<=0||height<=0)return;scale=std::min(width/1280.f,height/720.f);extra_x=width/scale-1280;extra_y=height/scale-720;}
 float x(float value,float anchor=0)const{return (value+extra_x*anchor)*scale;}
 float y(float value,float anchor=0)const{return (value+extra_y*anchor)*scale;}
};
}
