#include "display_resolution_policy.hpp"
#include <cassert>
#include <iostream>
#include <cmath>
using namespace ofo;
int main(){
 auto modes=normalize_display_resolutions({{1920,1080},{640,480},{1280,720},{1920,1080},{320,240},{2560,1440},{1024,768}});
 assert(modes.size()==5);assert(modes.front()==DisplayResolution(640,480));
 assert(closest_display_resolution(modes,1920,1080)==DisplayResolution(1920,1080));
 assert(closest_display_resolution(modes,1921,1081)==DisplayResolution(1920,1080));
 assert(closest_display_resolution(modes,1279,720)==DisplayResolution(1280,720));
 assert(scaled_scene_size(1920,1080,50)==DisplayResolution(960,540));
 assert(scaled_scene_size(1920,1080,100)==DisplayResolution(1920,1080));
 assert(scaled_scene_size(1920,1080,200)==DisplayResolution(3840,2160));
 assert(scaled_scene_size(1920,1080,1000)==DisplayResolution(3840,2160));
 assert(scaled_scene_size(1920,1080,1)==DisplayResolution(960,540));
 assert(scaled_scene_size(1920,1080,200,2048)==DisplayResolution(2048,1152));
 const auto tall=scaled_scene_size(720,1280,125);assert(tall==DisplayResolution(900,1600));
 std::cout<<"display resolutions, caps, scale 50-200: PASS\n";
}
