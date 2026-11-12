#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
namespace ofo {
using DisplayResolution=std::pair<int,int>;
inline std::vector<DisplayResolution> normalize_display_resolutions(std::vector<DisplayResolution> modes){
 modes.erase(std::remove_if(modes.begin(),modes.end(),[](const auto&m){return m.first<640||m.second<480||m.first>16384||m.second>16384;}),modes.end());
 std::sort(modes.begin(),modes.end(),[](const auto&a,const auto&b){const auto aa=int64_t(a.first)*a.second,bb=int64_t(b.first)*b.second;return aa==bb?a<b:aa<bb;});
 modes.erase(std::unique(modes.begin(),modes.end()),modes.end());return modes;
}
inline DisplayResolution closest_display_resolution(const std::vector<DisplayResolution>&modes,int w,int h){
 if(modes.empty())return {std::max(640,w),std::max(480,h)};
 const double target=double(std::max(1,w))/std::max(1,h),area=double(std::max(1,w))*std::max(1,h);
 auto score=[&](const auto&m){const double ratio=double(m.first)/m.second,ar=std::abs(std::log(ratio/target));return 3*ar+std::abs(std::log(double(m.first)*m.second/area));};
 return *std::min_element(modes.begin(),modes.end(),[&](const auto&a,const auto&b){return score(a)<score(b);});
}
inline DisplayResolution scaled_scene_size(int output_w,int output_h,int percent,int max_dimension=8192){
 output_w=std::max(1,output_w);output_h=std::max(1,output_h);percent=std::clamp(percent,50,200);
 const double k=std::min(double(percent)/100.,std::min(double(std::max(1,max_dimension))/output_w,double(std::max(1,max_dimension))/output_h));
 return {std::max(1,int(std::lround(output_w*k))),std::max(1,int(std::lround(output_h*k)))};
}
}
