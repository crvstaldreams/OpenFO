#pragma once
#include "vegetation.hpp"

namespace ofo {
// Static vertex payload: the native vertex program expands each corner from
// its base, direction, phase, signed half width and corner height.
inline std::array<Vertex,4> plant_render_vertices(const PlantSprite&s){
 std::array<Vertex,4> out{};
 const float u=(s.tile%4)*.25f,v=(s.tile/4)*.25f;
 const std::array<std::array<float,2>,4> uv{{{u+.25f,v+.25f},{u,v+.25f},{u,v},{u+.25f,v}}};
 const uint32_t color=0xff000000u|(uint32_t(s.color[0]*255)<<16)|(uint32_t(s.color[1]*255)<<8)|uint32_t(s.color[2]*255);
 for(int n=0;n<4;n++){
  auto&vertex=out[n];vertex.position={s.position[0],s.position[1],s.position[2]};
  vertex.normal={s.direction[0],s.direction[1],float(s.wind_phase&15)};vertex.color=color;
  vertex.u=uv[n][0];vertex.v=uv[n][1];vertex.lu=(n==0||n==3)?s.width:-s.width;vertex.lv=n<2?0:s.height;
 }
 return out;
}
inline constexpr std::array<uint16_t,6> plant_render_indices{0,1,2,0,2,3};
}
