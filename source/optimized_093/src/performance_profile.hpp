#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <string>

namespace ofo {
// Heuristic for default, opt-out low-spec policy; not a GPU benchmark.
// Never classify a high-end vendor as slow solely because it is Intel/AMD/NVIDIA.
struct GpuPerformanceProfile {
 std::string vendor="Unknown";
 bool integrated=false;
 int detail_cap=3;   // 0=LOW, 1=MEDIUM, 2=HIGH, 3=ULTRA
 bool skip_ssao=false,skip_ssr=false;
 unsigned mirror_stride=1;
 unsigned shadow_resolution=0,shadow_draws=0,shadow_triangles=0; // 0 = renderer defaults
};
inline std::string gpu_lower(std::string s){
 std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;
}
inline bool gpu_contains(const std::string& s,const char* needle){return s.find(needle)!=std::string::npos;}
inline std::string gpu_compact(const std::string& s){
 std::string result;result.reserve(s.size());
 for(unsigned char c:s)if(std::isalnum(c))result.push_back(char(std::tolower(c)));
 return result;
}
// Reference labels are provided by driver/Direct3D and can include trademark markers,
// e.g. "Intel(R) Iris(R) Xe Graphics". Conservative detection avoids demoting Intel Arc,
// modern Radeon RX, or newer integrated Radeon 780M without a performance measurement.

inline GpuPerformanceProfile gpu_performance_profile(std::string name){
 auto s=gpu_lower(std::move(name));auto compact=gpu_compact(s); GpuPerformanceProfile p;
 if(gpu_contains(s,"intel")){
  p.vendor="Intel";
  if(gpu_contains(s,"arc"))return p; // Arc discrete is not an Intel HD iGPU.
  if((gpu_contains(compact,"irisxe")||(gpu_contains(s,"iris")&&gpu_contains(s,"xe graphics")))){p.integrated=true;p.detail_cap=2;p.mirror_stride=2;return p;}
  if(gpu_contains(s,"iris")||gpu_contains(s,"uhd")||gpu_contains(s,"hd graphics")||gpu_contains(s,"gma")){
   p.integrated=true;p.detail_cap=1;p.skip_ssao=p.skip_ssr=true;p.mirror_stride=3;
   p.shadow_resolution=256;p.shadow_draws=72;p.shadow_triangles=70000;
   if(gpu_contains(compact,"hdgraphics4000")||gpu_contains(compact,"hdgraphics3000")||gpu_contains(compact,"hdgraphics2500")||gpu_contains(s,"gma")){
    p.detail_cap=0;p.mirror_stride=4;p.shadow_draws=48;p.shadow_triangles=40000;
   }
  }
  return p;
 }
 if(gpu_contains(s,"amd")||gpu_contains(s,"radeon")||gpu_contains(s,"ati ")){
  p.vendor="AMD";
  if(gpu_contains(s,"radeon graphics")||gpu_contains(compact,"radeon610m")||gpu_contains(compact,"radeon780m")||gpu_contains(compact,"radeon680m")){
   p.integrated=true;p.detail_cap=2;p.mirror_stride=2; // ambiguous APU: medium reduction only
  }
  if(gpu_contains(compact,"vega3")||gpu_contains(compact,"vega8")||gpu_contains(compact,"vega11")||gpu_contains(compact,"radeon610m")||gpu_contains(compact,"radeonr5")||gpu_contains(compact,"radeonr7")){
   p.integrated=true;p.detail_cap=1;p.skip_ssao=p.skip_ssr=true;p.mirror_stride=3;
   p.shadow_resolution=256;p.shadow_draws=96;p.shadow_triangles=90000;
  }
  return p;
 }
 if(gpu_contains(s,"nvidia")||gpu_contains(s,"geforce")||gpu_contains(s,"quadro")){
  p.vendor="NVIDIA";
  if(gpu_contains(s,"k1100m")||gpu_contains(s,"k1000m")||gpu_contains(s,"k610m")||gpu_contains(s,"geforce 610m")||gpu_contains(s,"geforce 710m")||gpu_contains(s,"geforce 820m")||gpu_contains(s,"geforce 920m")){
   p.detail_cap=1;p.skip_ssao=p.skip_ssr=true;p.mirror_stride=3;
   p.shadow_resolution=256;p.shadow_draws=96;p.shadow_triangles=90000;
  }
  return p;
 }
 return p;
}
inline int gpu_effective_detail(int requested,const GpuPerformanceProfile& p,bool automatic){
 return automatic?std::min(std::clamp(requested,0,3),p.detail_cap):std::clamp(requested,0,3);
}
inline unsigned gpu_mirror_stride(int detail,const GpuPerformanceProfile& p,bool automatic){
 if(!automatic)return 1;
 const unsigned by_detail=detail==0?4u:detail==1?3u:detail==2?2u:1u;
 return std::max(by_detail,p.mirror_stride);
}
inline bool gpu_rearview_due(uint64_t frame,uint64_t renders,bool has_texture,unsigned stride){
 return !renders||!has_texture||stride<=1||frame%stride==0;
}
// CPU-side, display-only data can be sampled at a slower rate than simulation.
// Always refresh on important transitions (e.g., the race finishing).
inline bool gpu_display_sample_due(double wall_seconds,double last_seconds,bool force,double period_seconds=0.125){
 return force || !std::isfinite(last_seconds) || last_seconds<0 || wall_seconds<last_seconds || wall_seconds-last_seconds>=period_seconds;
}
}
