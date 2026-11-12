#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace ofo {
// Simulation-clock envelope. No speed term: ordinary acceleration is invisible.
class NitroCameraEnvelope {
 float amount_=0;double time_=0;uint64_t generation_=0;bool ready_=false;
 public:
 float update(bool active,double time,uint64_t generation,bool reset=false){
  double elapsed=time-time_;bool invalid=!std::isfinite(time);
  if(reset||!ready_||invalid||generation!=generation_||elapsed<0||elapsed>.25)amount_=0;
  else if(elapsed>0)amount_+=(float(active)-amount_)*(1-std::exp(-(active?8.f:4.f)*float(elapsed)));
  time_=invalid?0:time;generation_=generation;ready_=!invalid;return amount_;
 }
};
}
