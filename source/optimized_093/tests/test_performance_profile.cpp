#include "performance_profile.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace ofo;
int main(){
 auto hd=gpu_performance_profile("Intel(R) HD Graphics 4000");
 assert(hd.vendor=="Intel"&&hd.integrated&&hd.detail_cap==0);
 assert(hd.skip_ssao&&hd.skip_ssr&&hd.mirror_stride==4);
 auto xe=gpu_performance_profile("Intel(R) Iris(R) Xe Graphics");
 assert(xe.vendor=="Intel"&&xe.detail_cap==2&&xe.mirror_stride==2);
 auto arc=gpu_performance_profile("Intel(R) Arc(TM) A770 Graphics");
 assert(arc.detail_cap==3&&!arc.skip_ssao);
 auto vega=gpu_performance_profile("AMD Radeon Vega 8 Graphics");
 assert(vega.vendor=="AMD"&&vega.integrated&&vega.detail_cap==1);
 auto r7=gpu_performance_profile("AMD Radeon RX 7700 XT");
 assert(r7.vendor=="AMD"&&r7.detail_cap==3);
 auto modern_apu=gpu_performance_profile("AMD Radeon(TM) 780M Graphics");
 assert(modern_apu.detail_cap>=2);
 auto q=gpu_performance_profile("NVIDIA Quadro K1100M");
 assert(q.vendor=="NVIDIA"&&q.detail_cap==1&&q.shadow_resolution==256);
 auto n=gpu_performance_profile("NVIDIA GeForce RTX 3060");
 assert(n.detail_cap==3);
 assert(gpu_effective_detail(3,hd,true)==0);
 assert(gpu_effective_detail(3,hd,false)==3); // manual setting is never overridden
 assert(gpu_mirror_stride(0,hd,true)==4);
 assert(gpu_mirror_stride(0,hd,false)==1);
 assert(gpu_rearview_due(1,0,false,4));
 assert(!gpu_rearview_due(1,1,true,4));
 assert(gpu_rearview_due(4,1,true,4));
 assert(gpu_display_sample_due(1.,-1.,false));
 assert(!gpu_display_sample_due(1.1,1.,false));
 assert(gpu_display_sample_due(1.13,1.,false));
 assert(gpu_display_sample_due(1.1,1.,true));
 assert(gpu_display_sample_due(.8,1.,false)); // game clock reset
 assert(gpu_display_sample_due(1.,std::numeric_limits<double>::infinity(),false));
}
