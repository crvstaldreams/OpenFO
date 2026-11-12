#include "headlight_visibility.hpp"
#include <stdexcept>
static void need(bool b){if(!b)throw std::runtime_error("headlight emitter visibility regression");}
int main(){using namespace ofo;
 auto front=headlight_emitter_visibility(1,1),rear=headlight_emitter_visibility(-1,1),cockpit=headlight_emitter_visibility(-.8f,1),side=headlight_emitter_visibility(0,1),broken=headlight_emitter_visibility(-1,0);
 need(front.lens>0&&front.shaft>0);
 need(rear.lens==0&&rear.shaft>0);
 need(cockpit.lens==0&&cockpit.shaft>0);
 need(side.lens==0&&side.shaft>=rear.shaft);
 need(broken.lens==rear.lens&&broken.shaft==rear.shaft);
 auto broken_front=headlight_emitter_visibility(1,0);need(broken_front.lens==front.lens&&broken_front.shaft==front.shaft);
 auto partial=headlight_emitter_visibility(-1,.5f);need(partial.shaft==rear.shaft);
}
