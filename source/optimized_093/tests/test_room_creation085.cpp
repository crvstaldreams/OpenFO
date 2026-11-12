#include "lan_session.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ofo;
int main(){
 try{
  LanRules rules;rules.stage="tracks/forest/forest1/a";
  LanSession directory;directory.serve(0,"OpenFO",rules);
  std::unique_ptr<LanSession> custom;
  directory.set_directory([&]{
   std::vector<LanRoomInfo> items{directory.room_info()};
   if(custom)items.push_back(custom->room_info());
   return items;
  });
  directory.set_room_creator([&](const std::string& name)->uint16_t{
   if(name!="Тестова кімната")throw std::runtime_error("Wrong UTF-8 name");
   if(custom)throw std::runtime_error("Already exists");
   custom=std::make_unique<LanSession>();custom->serve(0,name,rules);
   return custom->port();
  });
  const auto endpoint="127.0.0.1:"+std::to_string(directory.port());
  auto f=std::async(std::launch::async,[&]{return LanSession::create_room(endpoint,"Тестова кімната");});
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(f.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready&&std::chrono::steady_clock::now()<deadline){
   directory.poll();if(custom)custom->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if(f.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)throw std::runtime_error("Timed out waiting for room creation");
  const auto port=f.get();if(!custom||port!=custom->port()||custom->room_name()!="Тестова кімната")throw std::runtime_error("Incorrect created room");
  auto rooms_future=std::async(std::launch::async,[&]{return LanSession::list_rooms(endpoint);});
  const auto read_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(rooms_future.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready&&std::chrono::steady_clock::now()<read_deadline){
   directory.poll();custom->poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if(rooms_future.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)throw std::runtime_error("Directory query timeout");
  const auto rooms=rooms_future.get();
  if(rooms.size()!=2||rooms.back().name!="Тестова кімната"||rooms.back().port!=port)throw std::runtime_error("Created room not listed");
  LanSession player;LanPlayer driver;driver.name="TEST";driver.car_id=3;player.join("127.0.0.1:"+std::to_string(port),driver);
  const auto join_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while((player.phase()!=LanSession::Phase::Lobby||!player.is_host())&&std::chrono::steady_clock::now()<join_deadline){
   directory.poll();custom->poll();player.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if(player.phase()!=LanSession::Phase::Lobby||!player.is_host())throw std::runtime_error("Creator could not join and own the room");
  std::cout<<"Named room creation, listing, and owner join passed\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
