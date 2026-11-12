#include "frame_clock.hpp"
#include <stdexcept>
#include <iostream>
using namespace ofo;
void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);}
int main(){FrameClock c;int ticks=0;for(int i=0;i<60;i++)ticks+=c.advance(1./60,true);check(ticks==100,"60 render frames produce 100 physics steps");for(int i=0;i<100;i++)check(c.advance(60,false)==0,"paused time is frozen");check(c.advance(60,true,true)==0,"resume drops paused wall time");check(c.advance(.01,true)==1,"resume has no catchup backlog");check(c.advance(30,true)==5,"stall catchup bounded");check(c.alpha()<1&&c.dropped()>29,"excess wall time counted");
 FrameClock slowed;
 check(slowed.advance(1,true,false,.25)==1&&std::abs(slowed.alpha()-.25f)<1e-6,"slow-motion stall cap applies before scaling");
 check(std::abs(slowed.dropped()-.95)<1e-6,"slow motion does not distort wall time metrics");
 std::cout<<"frame clock/pause checks passed\n";
}
