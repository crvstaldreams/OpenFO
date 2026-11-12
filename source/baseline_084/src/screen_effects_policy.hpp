#pragma once
#include "math.hpp"
#include <utility>
namespace ofo {
struct ScreenEffectsBudget {
 bool enabled=false; int ao_divisor=4,ao_samples=0,ssr_steps=0;
 float ao_radius=1.8f,ao_strength=1.8f,ssr_distance=50.f,ssr_strength=.5f;
};
inline ScreenEffectsBudget screen_effects_budget(int level){
 ScreenEffectsBudget b;
 switch(std::clamp(level,0,3)){
 case 0:break;
 case 1:b.enabled=true;b.ao_samples=4;b.ssr_steps=8;break;
 case 2:b.enabled=true;b.ao_divisor=2;b.ao_samples=6;b.ssr_steps=12;break;
 case 3:b.enabled=true;b.ao_divisor=2;b.ao_samples=8;b.ssr_steps=16;break;
 }return b;
}
inline std::pair<int,int> screen_effects_size(int w,int h,int divisor){
 divisor=std::max(1,divisor);w=std::max(1,w);h=std::max(1,h);
 return {w/divisor+(w%divisor!=0),h/divisor+(h%divisor!=0)};
}
// Portable reference of the shader's D3D LH projection and sampling math.
namespace screen_effects_reference {
struct UV {float x=0,y=0;};
struct Projection {float x,y,a,b;explicit Projection(const Mat&p):x(p[0]),y(p[5]),a(p[10]),b(p[14]){}};
inline float view_z(float depth,const Projection&p){float d=depth-p.a;return p.b/(std::abs(d)>1e-7f?d:-1e-7f);}
inline Vec reconstruct(UV uv,float depth,const Projection&p){float z=view_z(depth,p);return {(2*uv.x-1)*z/p.x,(1-2*uv.y)*z/p.y,z};}
inline UV project(Vec v,const Projection&p){return {.5f+.5f*v.x*p.x/v.z,.5f-.5f*v.y*p.y/v.z};}
inline float depth(Vec v,const Projection&p){return p.a+p.b/v.z;}
inline bool inside(UV u){return u.x>.001f&&u.x<.999f&&u.y>.001f&&u.y<.999f;}
inline Vec reflect(Vec incident,Vec normal){return incident-normal*(2*dot(incident,normal));}
inline float ao_neighbor(Vec center,Vec normal,Vec neighbor,float radius){Vec d=neighbor-center;float l=length(d);return l>1e-4f?std::max(0.f,dot(normal,d/l)-.08f)*std::max(0.f,1-l/radius):0.f;}
inline float ao_factor(float contribution,int count,float strength){return std::max(.78f,1-strength*contribution/std::max(1,count));}
struct Hit {bool found=false;UV uv{};float distance=0;};
// depth_at returns raw D3D [0,1] depth; 1 is sky. Same march/refine limits as HLSL.
template<class DepthAt> Hit trace(Vec position,Vec normal,Vec ray,const Projection&p,int steps,float max_distance,DepthAt depth_at){
 steps=std::clamp(steps,1,16);Vec start=position+normal*.08f+ray*.12f;
 if(start.z<=.01f)return {};
 UV start_uv=project(start,p);if(!inside(start_uv))return {};
 float start_depth=depth_at(start_uv);
 float previous=0;bool previous_front=start_depth<.99999f&&start.z<view_z(start_depth,p);
 for(int i=1;i<=steps;++i){float t=max_distance*float(i)/steps;Vec q=start+ray*t;
  if(q.z<=.01f)return {};
  UV uv=project(q,p);if(!inside(uv))return {};
  float d=depth_at(uv);if(d>=.99999f){previous=t;previous_front=false;continue;}
  float delta=q.z-view_z(d,p);
  if(delta>=0&&previous_front&&t>.2f){float lo=previous,hi=t;
   for(int k=0;k<3;++k){float mid=(lo+hi)*.5f;Vec v=start+ray*mid;UV m=project(v,p);float md=depth_at(m);if(md<.99999f&&v.z>=view_z(md,p))hi=mid;else lo=mid;}
   Vec v=start+ray*hi;UV hit=project(v,p);float hd=depth_at(hit);float gap=v.z-view_z(hd,p);
   if(inside(hit)&&hd<.99999f&&gap>=0&&gap<.25f+.015f*v.z)return {true,hit,hi};
   return {};
  }
  previous=t;previous_front=delta<0;
 }return {};
}
}
}
