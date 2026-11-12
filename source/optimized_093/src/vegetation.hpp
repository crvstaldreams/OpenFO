#pragma once
#include "assets.hpp"

namespace ofo {
struct PlantSprite {std::array<float,3> position{},color{1,1,1};float width=1,height=1;uint16_t variation=0;uint8_t tile=0,wind_phase=0;std::array<float,2>direction{1,0};};
struct PlantCell {size_t first=0,count=0;struct Bounds{std::array<float,3>lo{},hi{};}bounds;};
struct Vegetation {std::vector<PlantSprite>sprites;std::vector<PlantCell>cells;std::string source,texture,color_source,error;float fade_start=100,far_clip=150;};
Vegetation parse_vegetation(const Bytes&geometry,const Bytes&colors);
Vegetation load_vegetation(const Resources&,const std::string&track,const std::string&stage,int weather=2);
// Wind/quad construction recovered from the original CPU plant renderer.
using PlantWind=std::array<std::array<float,3>,16>;
struct PlantQuad {std::array<std::array<float,3>,4> position;std::array<std::array<float,2>,4> uv;};
PlantWind plant_wind(double seconds);
PlantQuad plant_quad(const PlantSprite&,const PlantWind&);
float plant_opacity(float view_depth,float start,float end);
}
