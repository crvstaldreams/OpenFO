#pragma once
#include "scene_lighting.hpp"

namespace ofo {
inline bool radeon_gl_adapter(std::string adapter){
 adapter=lower(std::move(adapter));
 return adapter.find("radeon")!=adapter.npos||adapter.rfind("ati ",0)==0||
        adapter.rfind("amd ",0)==0||adapter.rfind("advanced micro devices",0)==0;
}
struct LampRenderBudget {
 unsigned resolution,draws,triangles;
};
inline LampRenderBudget render_lamp_budget(bool radeon,int quality){
 if(radeon&&quality<3)
  return {quality<2?256u:512u,120,150000};
 return {headlight_shadow_map_size,headlight_shadow_draw_budget,headlight_shadow_triangle_budget};
}
inline unsigned render_anisotropy_limit(bool radeon,int quality,unsigned maximum){
 return radeon&&quality<3?std::min(maximum,4u):maximum;
}
}
