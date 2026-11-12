#pragma once
#include <algorithm>
#include <cmath>
namespace ofo {
// Advance only from fixed race steps. Pause, replay and photo mode never tick.
class RecoveryFlow {
 double left_=0;bool saw_ejected_=false;
public:
 void reset(){left_=0;saw_ejected_=false;}
 void request(){if(left_<=0)left_=3;}
 void cancel(){left_=0;}
 bool active()const{return left_>0;}
 double remaining()const{return left_;}
 // Stunts leave the ejected driver in play until the attempt ends.
 bool step(double dt,bool ejected,bool finished=false,bool auto_recover_on_eject=true){
  if(finished){left_=0;saw_ejected_=ejected;return false;}
  if(auto_recover_on_eject&&ejected&&!saw_ejected_){request();saw_ejected_=true;return false;}
  saw_ejected_=ejected;
  if(left_<=0||!std::isfinite(dt)||dt<=0)return false;
  left_=std::max(0.,left_-std::min(dt,.1));
  if(left_<1e-9){left_=0;return true;}return false;
 }
};
}
