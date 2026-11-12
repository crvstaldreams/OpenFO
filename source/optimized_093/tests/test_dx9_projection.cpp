#include "scene_lighting.hpp"
#include <cassert>
#include <cmath>
using namespace ofo;
static bool close(Vec a,Vec b){return length(a-b)<.0002f;}
int main(){
 VehicleConfig cfg;cfg.lo={-1,-.5f,-2};cfg.hi={1,1.5f,2};auto mount=headlight_mount(cfg);
 // Nontrivial rotation and translation: projectors use the actual rolled pose,
 // so no camera orientation or LH/RH raster front-face flag enters lighting.
 Mat pose={0,0,-1,0,-.6f,.8f,0,0,.8f,.6f,0,0,13,7,-21,1};auto inverse=headlight_inverse_pose(pose);
 for(int side=0;side<2;side++){
  Vec lamp{(side*2-1)*mount[0],mount[1],mount[2]};
  auto view=multiply(rotate_x(-std::atan(headlight_beam_pitch)),multiply(translate(lamp*-1.f),inverse));
  constexpr float near=.12f, far=headlight_range_m;
  auto projection=perspective(2*std::atan(headlight_cone_vertical),headlight_cone_horizontal/headlight_cone_vertical,near,far);
  for(Vec q: {Vec{0,0,10},Vec{2,1,20},Vec{-3,-2,30},Vec{32,-.7f,110}}){
   float c=1/std::sqrt(1+headlight_beam_pitch*headlight_beam_pitch);
   Vec beam{q.x,(q.y+headlight_beam_pitch*q.z)*c,(q.z-headlight_beam_pitch*q.y)*c};
   Vec world=point(pose,lamp+q);assert(close(point(view,world),beam));
   Vec projected=point(multiply(projection,view),world)/beam.z;
   // Shadow UV and receiver cookie are identical despite pose rotation/roll.
   assert(std::abs((.5f+.5f*projected.x)-(.5f+.5f*beam.x/(beam.z*headlight_cone_horizontal)))<.0001f);
   assert(std::abs((.5f-.5f*projected.y)-(.5f-.5f*beam.y/(beam.z*headlight_cone_vertical)))<.0001f);
   float shaderDepth=far/(far-near)-(near*far/(far-near))/beam.z;
   assert(std::abs(projected.z-shaderDepth)<.0001f);
   float recovered=near*far/(far-projected.z*(far-near));
   assert(std::abs(recovered-beam.z)<.03f);
  }
 }
}
