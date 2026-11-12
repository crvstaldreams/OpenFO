#include "lan_session.hpp"
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ofo;
using namespace std::chrono_literals;

void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void run(bool online){
 LanSession host,a,b;
 LanRules initial;initial.stage="tracks/forest/forest1/a";
 LanPlayer first;first.name="Alpha";first.car_id=18;
 LanPlayer second;second.name="Player Two";second.car_id=19;
 if(online){
  host.serve(0,"Forest",initial);
  host.set_directory([&]{return std::vector<LanRoomInfo>{host.room_info()};});
 }else host.host(0,first,initial);
 const auto address="localhost:"+std::to_string(host.port());
 auto pump=[&](auto&& done){
  for(int i=0;i<1600;++i){
   host.poll();a.poll();b.poll();
   if(done())return true;
   if(host.phase()==LanSession::Phase::Failed||a.phase()==LanSession::Phase::Failed||b.phase()==LanSession::Phase::Failed)return false;
   std::this_thread::sleep_for(2ms);
  }
  return false;
 };
 if(online){
  auto rooms=std::async(std::launch::async,[&]{return LanSession::list_rooms(address);});
  require(pump([&]{return rooms.wait_for(0ms)==std::future_status::ready;}),"room lookup");
  auto list=rooms.get();
  require(list.size()==1&&list[0].name=="Forest"&&list[0].port==host.port()&&list[0].open,"directory fields");
  a.join(address,first);
  require(pump([&]{return a.is_host()&&a.players().size()==1;}),"dedicated owner");
 }
 b.join(address,second);
 auto& owner=online?a:host;
 require(pump([&]{return b.players().size()==2&&owner.players().size()==2;}),"room roster");
 require(b.players()[0].car_id==18&&b.players()[1].car_id==19,"alpha selections");
 require(!owner.can_start(),"guest starts unready");
 LanRules changed=initial;changed.mode=RaceMode::FreeRoam;changed.laps=2;
 require(owner.set_rules(changed),"owner can change rules");
 require(pump([&]{return b.rules().laps==2;}),"rules forwarded");
 LanVoiceFrame samples{};samples.fill(121);
 b.send_voice(samples);
 std::vector<LanVoicePacket> voice;
 require(pump([&]{voice=owner.take_voice();return !voice.empty();}),"lobby voice delivery");
 require(voice.front().sender==b.local_id()&&voice.front().samples==samples,"voice payload");
 owner.set_ready(true);b.set_ready(true);
 require(pump([&]{return owner.can_start();}),"ready barrier");
 require(owner.start(),"prepare command");
 require(pump([&]{return owner.phase()==LanSession::Phase::Loading&&b.phase()==LanSession::Phase::Loading;}),"prepare reaches everyone");
 require(owner.take_load_request()&&b.take_load_request(),"load requests");
 owner.loaded();pump([&]{return b.players().front().loaded;});
 require(b.phase()==LanSession::Phase::Loading,"load barrier");
 b.loaded();
 require(pump([&]{return b.phase()==LanSession::Phase::Playing&&owner.phase()==LanSession::Phase::Playing;}),"launch reaches everyone");
 if(online)require(!host.room_info().open&&host.room_info().players==2,"directory in-progress state");
 LanPose p;p.car=translate({3,0,10});p.velocity={0,0,10};p.damage=.2f;
 b.publish(p);
 require(pump([&]{return owner.peer_pose(b.local_id()).has_value();}),"race pose delivery");
 std::this_thread::sleep_for(52ms);
 p.car[14]=20;b.publish(p);
 require(pump([&]{const auto pose=owner.peer_pose(b.local_id(),false);return pose&&pose->car[14]==20;}),"next pose");
 std::this_thread::sleep_for(28ms);
 const auto interpolated=owner.peer_pose(b.local_id());
 require(interpolated&&interpolated->car[14]>10&&interpolated->car[14]<20,"interpolated race position");
 owner.send_voice(samples);
 require(pump([&]{voice=b.take_voice();return !voice.empty();}),"race voice delivery");
 if(online){
  a.close();b.close();
  require(pump([&]{return host.room_info().open&&host.room_info().players==0;}),"room recycled");
 }
}
int main(){
 try{run(false);run(true);std::cout<<"LAN and DNS-addressed dedicated room passed\n";return 0;}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
