#include "performance_drop.hpp"
#include <cassert>
#include <limits>

using namespace ofo;

int main(){
 PerformanceDropDetector detector;
 double now=0;
 auto frame=[&](double ms,bool eligible=true){
  now+=ms;
  return detector.observe(now,ms,eligible);
 };
 for(int i=0;i<90;++i)assert(frame(16.7)==PerformanceDrop::None);
 assert(frame(67)==PerformanceDrop::Relative);
 assert(frame(155)==PerformanceDrop::None); // cooldown
 for(int i=0;i<500;++i)assert(frame(16.7)==PerformanceDrop::None);
 assert(frame(140)==PerformanceDrop::Severe);
 assert(frame(9000,false)==PerformanceDrop::None); // focus or scene transition
 for(int i=0;i<30;++i)assert(frame(33.3)==PerformanceDrop::None);
 assert(frame(60)==PerformanceDrop::None); // an isolated moderate frame
 for(int i=0;i<250;++i)assert(frame(33.3)==PerformanceDrop::None);
 for(int i=0;i<3;++i)assert(frame(60)==PerformanceDrop::None);
 assert(frame(60)==PerformanceDrop::Sustained);
 assert(frame(100,false)==PerformanceDrop::None);
 assert(detector.observe(now,std::numeric_limits<double>::infinity(),true)==PerformanceDrop::None);

 PerformanceDropDetector slow;
 double slow_time=0;
 for(int i=0;i<30;++i)assert(slow.observe(slow_time+=100,100,true)==PerformanceDrop::None);
 for(int i=0;i<3;++i)assert(slow.observe(slow_time+=100,100,true)==PerformanceDrop::None);
 assert(slow.observe(slow_time+=100,100,true)==PerformanceDrop::Sustained);
}
