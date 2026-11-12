#pragma once
#include <algorithm>
#include <cmath>
namespace ofo {
// Wall-time policy is independent of simulation and menus. Pause never accumulates a backlog.
class FrameClock {double remainder_=0,dropped_=0;public:
 int advance(double seconds,bool active,bool transition=false,double time_scale=1){
  if(!active||transition){remainder_=0;return 0;}
  if(!std::isfinite(seconds)||seconds<0)seconds=0;
  dropped_+=std::max(0.,seconds-.05);
  remainder_+=std::min(seconds,.05)*time_scale;
  int steps=std::min(int((remainder_+1e-10)/.01),5);
  remainder_=std::max(0.,remainder_-steps*.01);
  return steps;
 }
 float alpha()const{return float(remainder_/.01);}double dropped()const{return dropped_;}
};
}
