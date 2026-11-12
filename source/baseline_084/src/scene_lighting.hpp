#pragma once
#include "assets.hpp"
namespace ofo {
// Downward slope, shared by lens shafts and both physical lamp shadow views.
// A .7m lamp height places the beam axis on level road at 5m.
inline constexpr float headlight_beam_pitch=.11f;
inline constexpr float headlight_range_m=150.f;
inline constexpr float headlight_cone_horizontal=.70f;
inline constexpr float headlight_cone_vertical=.38f;
inline constexpr int headlight_shadow_map_size=512;
inline constexpr unsigned headlight_shadow_draw_budget=180;
inline constexpr unsigned headlight_shadow_triangle_budget=230000;
// Range, soft cone, pitch and depth frusta must match both GLSL and HLSL.
// Physics car poses are rigid. No general matrix inversion or scene readback.
inline Mat headlight_inverse_pose(const Mat&m){Mat r=identity();for(int c=0;c<3;c++)for(int row=0;row<3;row++)r[c*4+row]=m[row*4+c];auto t=direction(r,Vec{m[12],m[13],m[14]})*-1.f;r[12]=t.x;r[13]=t.y;r[14]=t.z;return r;}
inline std::array<float,4> headlight_mount(const VehicleConfig&cfg){return{std::clamp((cfg.hi.x-cfg.lo.x)*.32f,.35f,1.2f),cfg.lo.y+(cfg.hi.y-cfg.lo.y)*.42f,cfg.hi.z+.08f,0};}
}
