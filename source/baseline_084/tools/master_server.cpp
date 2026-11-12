#include "lan_session.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <fstream>
#include <set>
#include <thread>

namespace {
volatile std::sig_atomic_t stopping=0;
void shutdown(int){stopping=1;}
}
int main(int argc,char** argv){
 using namespace ofo;
 try{
  std::vector<std::string> settings;
  if(argc==3&&std::string_view(argv[1])=="--config"){
   std::ifstream file(argv[2]);
   if(!file)throw std::runtime_error("Could not open master server config");
   for(std::string line;std::getline(file,line);){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line.front()=='#')continue;
    settings.push_back(std::move(line));
    if(settings.size()>16)break;
   }
  }else for(int i=1;i<argc;++i)settings.emplace_back(argv[i]);
  if(settings.empty())settings.emplace_back("27267=OpenFO");
  if(settings.size()>16)throw std::runtime_error("Use up to 16 named rooms");
  std::vector<std::unique_ptr<LanSession>> rooms;
  std::set<int> ports;
  LanRules rules;rules.stage="tracks/forest/forest1/a";
  for(const auto& setting:settings){
   if(setting=="--help"){
    std::cout<<"OpenFOMasterServer [27267=Room name] [27268=Another room] or --config rooms.conf\n";
    return 0;
   }
   const auto split=setting.find('=');
   if(split==std::string::npos)throw std::runtime_error("Use PORT=NAME");
   size_t consumed=0;
   const int port=std::stoi(setting.substr(0,split),&consumed);
   if(consumed!=split||port<1||port>65535||!ports.insert(port).second)throw std::runtime_error("Invalid or duplicate room port");
   auto room=std::make_unique<LanSession>();
   room->serve(uint16_t(port),setting.substr(split+1),rules);
   std::cout<<room->room_name()<<" on TCP "<<room->port()<<std::endl;
   rooms.push_back(std::move(room));
  }
  for(auto& room:rooms)room->set_directory([&]{
   std::vector<LanRoomInfo> result;
   for(const auto& entry:rooms)result.push_back(entry->room_info());
   return result;
  });
  std::signal(SIGINT,shutdown);std::signal(SIGTERM,shutdown);
  while(!stopping){
   for(auto& room:rooms){
    room->poll();
    if(room->phase()==LanSession::Phase::Failed)throw std::runtime_error(room->error());
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(3));
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
