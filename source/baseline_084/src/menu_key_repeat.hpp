#pragma once
#include <cmath>
namespace ofo {
class MenuKeyRepeat {bool held_=false;double next_=0,last_=0;public:
 bool update(bool down,double now){if(!std::isfinite(now)){held_=false;return false;}if(!down){held_=false;last_=now;return false;}if(!held_||now<last_){held_=true;last_=now;next_=now+.4;return true;}last_=now;if(now<next_)return false;next_=now+.11;return true;}
};
}
