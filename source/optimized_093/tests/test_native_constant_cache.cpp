#include "native_shader_source.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

// A stale key (camera, object, lighting, shader or water time) must never reuse
// constants from the preceding draw. The uncached builder is the output oracle.
int main(){
 ofo::NativeConstantCache cache;
 auto world=ofo::identity(),view=ofo::look_at({14,4,-7},{14,4,-6});
 auto projection=ofo::perspective(1.1f,1.7f,.1f,1000.f);
 ofo::Vec sun{.1f,-.9f,.2f};
 auto check=[&](const std::string&file,float time){
  auto expected=ofo::native_constants(file,world,view,projection,sun,time);
  const auto&actual=cache.get(file,world,view,projection,sun,time);
  assert(std::memcmp(expected.data(),actual.data(),sizeof(expected))==0);
 };
 for(auto file:{"shader/pro_static2x.sha","shader/pro_lightmapped_spec.sha","openfo/tree_billboard","shader/emu_water.sha","shader/pro_clouds.sha","shader/emu_car_window.sha","shader/emu_car_body.sha","openfo/plant_wind"}){
  check(file,1.f);auto builds=cache.builds;
  for(int i=0;i<1000;i++)check(file,1.f);
  assert(cache.builds==builds); // repeated draws perform no constant construction
  world[12]+=1;check(file,1.f);assert(cache.builds==++builds);
  view[13]+=.25f;check(file,1.f);assert(cache.builds==++builds);
  projection[0]+=.125f;check(file,1.f);assert(cache.builds==++builds);
  sun.x+=.1f;check(file,1.f);assert(cache.builds==++builds);
  check(file,2.f);assert(cache.builds==builds+ofo::native_uses_time(file));
 }
 // Signed zero keys are distinct: the builder can propagate their sign bits.
 world[12]=0.f;check("shader/pro_static2x.sha",0);auto builds=cache.builds;
 world[12]=-0.f;check("shader/pro_static2x.sha",0);assert(cache.builds==builds+1);
 // A rejected singular specular draw must not poison the previous good entry.
 world=ofo::identity();check("shader/pro_lightmapped_spec.sha",0);builds=cache.builds;
 world[0]=0;bool threw=false;try{check("shader/pro_lightmapped_spec.sha",0);}catch(const std::runtime_error&){threw=true;}
 assert(threw && cache.builds==builds);
 world=ofo::identity();check("shader/pro_lightmapped_spec.sha",0);assert(cache.builds==builds);
 assert(cache.hits>=5000);
 // Disabling render optimizations must execute the original builder each draw.
 auto before=cache.builds;
 for(int i=0;i<10;i++){
  auto expected=ofo::native_constants("shader/pro_static2x.sha",world,view,projection,sun);
  auto&actual=cache.get("shader/pro_static2x.sha",world,view,projection,sun,0,false);
  assert(std::memcmp(expected.data(),actual.data(),sizeof(expected))==0);
 }
 assert(cache.builds==before+10);
 std::cout<<"native constant cache: exact output; 5000 repeated builds avoided\n";
}
