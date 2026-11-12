#pragma once
#include "assets.hpp"
namespace ofo {bool water_material(const Material&);struct WaterAnimation{size_t frame=0;float u=0,v=0;};WaterAnimation water_animation(double,size_t);std::vector<std::string>water_frames(const Resources&);}
