#pragma once
#include "assets.hpp"
namespace ofo {
// Ported v0.23 title_smoke/track_particle_uv. Marker/atlas native; cadence is approximate.
inline std::vector<Vertex>menu_smoke_vertices(Vec origin,Vec right,Vec up,double clock){std::vector<Vertex>out;out.reserve(144);for(int i=0;i<24;i++){float age=float(std::fmod(std::max(0.,clock)+i*.125,3.)),t=age/3,size=.22f+age*.42f;Vec center=origin+Vec{std::sin(i*2.4f+age)*age*.12f,age*.8f,std::cos(i*1.7f)*age*.1f};int x=256+(i%2)*64,y=256+((i/2)%2)*64;float u0=(x+.5f)/512,v0=(y+.5f)/512,u1=(x+63.5f)/512,v1=(y+63.5f)/512;Vertex q[4];for(int k=0;k<4;k++){q[k].position=center+right*((k==0||k==3?-1:1)*size)+up*((k<2?1:-1)*size);q[k].u=k==0||k==3?u0:u1;q[k].v=k<2?v0:v1;q[k].color=(uint32_t(std::max(0.f,.32f*std::sin(t*3.14159265f))*255)<<24)|0x8c8c87;}for(int k:{0,1,2,0,2,3})out.push_back(q[k]);}return out;}
}
