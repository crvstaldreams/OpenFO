#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ofo {
enum class PerformanceDrop { None, Relative, Severe, Sustained };

inline const char* performance_drop_name(PerformanceDrop drop){
 switch(drop){
  case PerformanceDrop::Relative:return "relative_spike";
  case PerformanceDrop::Severe:return "frame_125_ms";
  case PerformanceDrop::Sustained:return "sustained_low_fps";
  default:return "none";
 }
}

class PerformanceDropDetector {
 std::array<double,90> history_{};
 size_t count_=0,cursor_=0;
 int consecutive_=0;
 double last_drop_ms_=-8000;
public:
 PerformanceDrop observe(double elapsed_ms,double frame_ms,bool eligible){
  if(!eligible||!std::isfinite(frame_ms)||frame_ms<=0){
   count_=cursor_=0;
   consecutive_=0;
   return PerformanceDrop::None;
  }
  PerformanceDrop drop=PerformanceDrop::None;
  if(count_>=30&&elapsed_ms-last_drop_ms_>=8000){
   if(frame_ms>=125)drop=PerformanceDrop::Severe;
   else if(frame_ms>=50){
    auto history=history_;
    auto median=history.begin()+count_/2;
    std::nth_element(history.begin(),median,history.begin()+count_);
    if(frame_ms>=55&&frame_ms>=2.25 * (*median))drop=PerformanceDrop::Relative;
    else if(frame_ms>=83.33||frame_ms>=1.6 * (*median)){
     if(++consecutive_>=4)drop=PerformanceDrop::Sustained;
    }else consecutive_=0;
   }else consecutive_=0;
  }
  history_[cursor_]=frame_ms;
  cursor_=(cursor_+1)%history_.size();
  count_=std::min(history_.size(),count_+1);
  if(drop!=PerformanceDrop::None){
   last_drop_ms_=elapsed_ms;
   consecutive_=0;
  }
  return drop;
 }
};
}
