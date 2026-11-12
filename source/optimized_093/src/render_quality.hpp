#pragma once
#include <algorithm>
namespace ofo {
struct RenderQuality {int level=2;float geometry_error=1,grass_distance=1,texture_bias=0;int particle_stride=1;};
inline RenderQuality render_quality(int level){switch(level=std::clamp(level,0,3)){case 0:return{level,4,.5f,.75f,4};case 1:return{level,2,.75f,.25f,2};case 3:return{level,0,1,0,1};default:return{level,1,1,0,1};}}
inline const char*render_quality_name(int level){static const char*names[]={"LOW","MEDIUM","HIGH","ULTRA"};return names[std::clamp(level,0,3)];}
}
