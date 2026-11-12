#pragma once
#include "assets.hpp"
namespace ofo {inline std::string native_material_texture(const Resources&r,const std::string&geometry,const Material&m,int slot,const std::string&stage_lightmap){if(slot==0&&lower(fs::path(clean_path(m.texture[0])).filename().string())=="colormap.tga"&&r.exists(stage_lightmap))return clean_path(stage_lightmap);return r.texture(geometry,m.texture[slot]);}}
