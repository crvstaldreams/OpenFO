#pragma once
#include "simulation.hpp"
#include <deque>
namespace ofo {
class Replay{std::deque<State> frames_;double cursor_=0;bool playing_=true;float rate_=1;size_t bytes_=0;template<class T>static size_t payload(const std::shared_ptr<const std::vector<T>>&p){return p?p->size()*sizeof(T):0;}
 template<class T,size_t N>static size_t payload(const std::shared_ptr<const std::array<T,N>>&p){return p?sizeof(T)*N:0;}
 static size_t opponent_bytes(const std::shared_ptr<const std::vector<OpponentState>>&p){size_t n=payload(p);if(p)for(const auto&b:*p)n+=b.name.size();return n;}
 static size_t changed_bytes(const State&s,const State*previous){return (!previous||previous->opponents!=s.opponents?opponent_bytes(s.opponents):0)+(!previous||previous->collision_events!=s.collision_events?payload(s.collision_events):0)+(!previous||previous->driver_bones!=s.driver_bones?payload(s.driver_bones):0)+(!previous||previous->props!=s.props?payload(s.props):0)+(!previous||previous->impacts!=s.impacts?payload(s.impacts):0)+(!previous||previous->collision_contacts!=s.collision_contacts?payload(s.collision_contacts):0)+(!previous||previous->panels!=s.panels?payload(s.panels):0);}public:
 void clear(){frames_.clear();bytes_=0;cursor_=0;playing_=true;rate_=1;}
 void record(const State&s){
 // Immutable snapshots can span many stationary frames. Charge their payload
 // once per contiguous run; nonconsecutive reuse is conservatively recharged.
 const auto added=changed_bytes(s,frames_.empty()?nullptr:&frames_.back());
 frames_.push_back(s);bytes_+=sizeof(State)+added;
 while(frames_.size()>2&&(frames_.size()>60000||bytes_>128u*1024*1024)){
  bytes_-=sizeof(State);
  bytes_-=changed_bytes(frames_[0],&frames_[1]);
  frames_.pop_front();
 }
 }
 bool available()const{return frames_.size()>1;}
 void begin(){cursor_=0;playing_=true;rate_=1;}
 void tick(double dt){if(playing_&&!frames_.empty()){cursor_=std::min(cursor_+std::clamp(dt,0.,.1)*100*rate_,double(frames_.size()-1));if(cursor_>=frames_.size()-1)playing_=false;}}
 void toggle(){playing_=!playing_;}void rewind(){cursor_=0;}void seek(double seconds){if(!frames_.empty())cursor_=std::clamp(cursor_+seconds*100,0.,double(frames_.size()-1));}void speed(float value){rate_=std::clamp(value,.25f,4.f);}float speed()const{return rate_;}
 State sample()const{return frames_.empty()?State{}:frames_[size_t(cursor_)];}
 Json diagnostics()const{return{{"frames",frames_.size()},{"max_frames",60000},{"bytes",bytes_},{"max_bytes",128u*1024*1024},{"position_s",cursor_/100},{"playing",playing_},{"speed",rate_},{"scope","10 minute bounded state recording; no physics resimulation"}};}
};
}
