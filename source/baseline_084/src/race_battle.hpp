#pragma once
#include "simulation.hpp"
#include <algorithm>
#include <numeric>
namespace ofo {
// Last-man-standing adapter: physical damage determines elimination; no scripted
// health decay or fabricated contact score. Native bonus prices live in career.
class RaceBattle {
 std::vector<double>out_;double time_=0;bool done_=false;
public:
 explicit RaceBattle(size_t participants):out_(participants,-1){}
 void update(double dt,const State&s){if(done_)return;if(std::isfinite(dt)&&dt>0)time_+=std::min(dt,.1);for(size_t i=0;i<out_.size();i++){float damage=i==0?s.damage:s.opponents&&i-1<s.opponents->size()?(*s.opponents)[i-1].damage:1;if(out_[i]<0&&damage>=.999f)out_[i]=time_;}size_t alive=std::count(out_.begin(),out_.end(),-1.);done_=alive<=1||(!out_.empty()&&out_[0]>=0);}
 bool finished()const{return done_;}double elapsed()const{return time_;}size_t survivors()const{return std::count(out_.begin(),out_.end(),-1.);}
 Json standings(const State&s)const{auto health=[&](size_t i){return i==0?1-s.damage:s.opponents&&i-1<s.opponents->size()?1-(*s.opponents)[i-1].damage:0;};std::vector<size_t>order(out_.size());std::iota(order.begin(),order.end(),0);std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){if((out_[a]<0)!=(out_[b]<0))return out_[a]<0;if(out_[a]>=0&&out_[a]!=out_[b])return out_[a]>out_[b];return health(a)>health(b);});const bool sole_survivor=survivors()==1;Json rows=Json::array();for(size_t rank=0;rank<order.size();rank++){auto i=order[rank];rows.push_back({{"car",i},{"rank",rank+1},{"lap",int(std::clamp(health(i),0.f,1.f)*100)},{"laps",100},{"time",out_[i]<0?time_:out_[i]},{"finished",done_},{"status",out_[i]<0?(done_?(sole_survivor?"WINNER":"SURVIVED"):"RUNNING"):"WRECKED"}});}return rows;}
};
}
