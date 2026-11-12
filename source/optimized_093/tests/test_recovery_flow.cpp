#undef NDEBUG
#include "recovery_flow.hpp"
#include <cassert>
#include <cmath>
int main(){ofo::RecoveryFlow r;assert(!r.step(.01,true));assert(r.remaining()==3);for(int i=0;i<299;++i)assert(!r.step(.01,true));assert(r.active());assert(r.step(.01,true));assert(!r.active());for(int i=0;i<400;++i)assert(!r.step(.01,true));r.reset();r.request();r.step(NAN,false);assert(r.remaining()==3);r.request();for(int i=0;i<100;++i)r.step(.01,false);double paused=r.remaining();assert(std::abs(paused-2)<1e-8);r.request();assert(r.remaining()==paused);r.step(.01,false,true);assert(!r.active());r.reset();assert(!r.step(.01,false));assert(!r.step(.01,true));assert(r.remaining()==3);}
