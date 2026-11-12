#pragma once
#include "minimap_composite.hpp"
#include <limits>

namespace ofo {
// Draw the authored spline once at track load. Keep the map background
// transparent and share the exact world-to-UV bounds with the moving markers.
struct RaceHudMap : MinimapImage {
 float left=0,top=0,right=0,bottom=0;
};
inline RaceHudMap race_hud_map(const Json&route,bool north_up=true) {
 RaceHudMap map;
 if(!route.value("available",false))return map;
 std::vector<Vec> points;
 float minx=std::numeric_limits<float>::infinity(),minz=minx,maxx=-minx,maxz=-minx;
 for(const auto&p:route.value("points",Json::array())){
  if(!p.is_array()||p.size()<3)return {};
  float x=p.at(0).get<float>(),z=p.at(2).get<float>();
  if(!std::isfinite(x)||!std::isfinite(z))return {};
  points.push_back({x,0,z});
  minx=std::min(minx,x);minz=std::min(minz,z);maxx=std::max(maxx,x);maxz=std::max(maxz,z);
 }
 if(points.size()<3)return map;
 float extent=std::max(maxx-minx,maxz-minz);
 if(!std::isfinite(extent)||extent<1)return map;
 extent*=1.20f;
 float cx=minx+(maxx-minx)*.5f,cz=minz+(maxz-minz)*.5f;
 map.left=cx-extent*.5f;map.right=cx+extent*.5f;
 map.top=cz+(north_up?.5f:-.5f)*extent;map.bottom=cz-(north_up?.5f:-.5f)*extent;
 constexpr int size=256;constexpr float radius=2.45f,halo=4.05f;
 map.width=map.height=size;map.pixels.assign(size*size,0);
 std::vector<float> distances(size*size,halo+1);
 auto pixel=[&](Vec p){return Vec{(p.x-map.left)/(map.right-map.left)*size,0,(p.z-map.top)/(map.bottom-map.top)*size};};
 for(size_t i=0;i<points.size();++i){
  auto a=pixel(points[i]),b=pixel(points[(i+1)%points.size()]);Vec d=b-a;float len=d.x*d.x+d.z*d.z;
  if(len<1e-6f)continue;
  int lx=std::max(0,int(std::floor(std::min(a.x,b.x)-halo-1))),hx=std::min(size-1,int(std::ceil(std::max(a.x,b.x)+halo+1)));
  int ly=std::max(0,int(std::floor(std::min(a.z,b.z)-halo-1))),hy=std::min(size-1,int(std::ceil(std::max(a.z,b.z)+halo+1)));
  for(int y=ly;y<=hy;++y)for(int x=lx;x<=hx;++x){
   float dx=x+.5f-a.x,dz=y+.5f-a.z,t=std::clamp((dx*d.x+dz*d.z)/len,0.f,1.f);
   auto&distance=distances[size_t(y)*size+x];
   distance=std::min(distance,std::hypot(dx-t*d.x,dz-t*d.z));
  }
 }
 for(size_t i=0;i<distances.size();++i){
  float w=std::clamp(radius+.5f-distances[i],0.f,1.f);
  float h=.64f*std::clamp(halo+.5f-distances[i],0.f,1.f)*(1-w),a=w+h;
  unsigned gray=a>0?unsigned(w/a*255+.5f):0;
  map.pixels[i]=(uint32_t(a*255+.5f)<<24)|(gray*0x010101u);
 }
 return map;
}
}
