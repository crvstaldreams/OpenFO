#include "frontend.hpp"
#include <set>
#include <stdexcept>
#include "file_io.hpp"

namespace ofo {
namespace {
std::vector<const MenuTrack*> track_choices(const Frontend& menu,RaceMode mode){
 std::vector<const MenuTrack*> out;
 std::set<std::pair<std::string,std::string>> seen;
 for(const auto& track:menu.track_slots){
  if(mode!=RaceMode::FreeRoam&&race_mode(track.track,track.event)!=mode)continue;
  if(mode==RaceMode::Stunt&&stunt_kind(track.subevent)==StuntKind::None)continue;
  const auto detail=mode==RaceMode::Stunt?track.subevent:std::string{};
  if(menu.assets&&menu.assets->exists(track.track)&&seen.emplace(track.stage,detail).second)out.push_back(&track);
 }
 if(mode==RaceMode::FreeRoam)
  for(const auto& track:menu.alpha_tracks)if(seen.emplace(track.stage,"").second)out.push_back(&track);
 return out;
}
const MenuTrack* selected_track(const Frontend& menu,const LanRules& rules){
 for(const auto* t:track_choices(menu,rules.mode))
  if(t->stage==rules.stage&&(rules.mode!=RaceMode::Stunt||t->subevent==rules.subevent))return t;
 return nullptr;
}
void set_lan_track(LanRules& rules,const MenuTrack& track){
 rules.stage=track.stage;
 rules.subevent=rules.mode==RaceMode::Stunt?track.subevent:"";
 rules.laps=std::clamp(track.laps,1,9);
}
const char* mode_label(RaceMode mode){
 switch(mode){
 case RaceMode::Circuit:return "QUICK RACE";
 case RaceMode::Battle:return "DERBY";
 case RaceMode::Stunt:return "STUNT";
 case RaceMode::FreeRoam:return "FREE DRIVE";
 }
 return "";
}
LanPlayer selected_player(const Frontend& menu){
 LanPlayer player;
 player.name=menu.profile_name.empty()?"PLAYER":menu.profile_name;
 player.car_id=menu.selected_car_id();
 player.skin=menu.choice.skin;
 return player;
}
}
const MenuTrack* Frontend::lan_track()const{return lan?selected_track(*this,lan->rules()):nullptr;}
std::array<std::string,5> Frontend::lan_setting_values(const MenuTrack* track)const{
 const auto& rules=lan->rules();
 return {ui_localize(mode_label(rules.mode),language),track?track->name:rules.stage,
         rules.mode==RaceMode::Circuit?std::to_string(rules.laps):"—",
         cars.empty()?ui_localize("NO CARS",language):cars[car_index].name,std::to_string(choice.skin)};
}
std::vector<std::string> Frontend::lan_labels()const{
 auto l=[&](const char* value){return ui_localize(value,language);};
 if(page==MenuPage::LanStart)
  return {l("HOST LAN"),l("HOST ADDRESS")+": "+(lan_address.empty()?"< IP:27267 >":lan_address),
          l("JOIN LAN"),l("MASTER SERVER")+": "+(master_address.empty()?"< DNS:27267 >":master_address),
          l("BROWSE ONLINE ROOMS"),l("CREATE ONLINE ROOM"),
          l("ROOM NAME")+": "+(room_draft.empty()?"< "+l("ENTER ROOM NAME")+" >":room_draft),l("BACK")};
 if(page==MenuPage::LanRooms){
  std::vector<std::string> rows;
  if(room_lookup.valid())rows.push_back(l("SEARCHING FOR ROOMS"));
  else for(const auto& room:online_rooms)
   rows.push_back(room.name+"  "+std::to_string(room.players)+"/8"+(room.open?"":"  "+l("RACE IN PROGRESS")));
  rows.push_back(l("BACK"));return rows;
 }
 if(!lan)return {l("BACK")};
 if(lan->phase()==LanSession::Phase::Connecting)return {l("CONNECTING TO HOST"),l("CANCEL")};
 if(lan->phase()==LanSession::Phase::Failed)return {l("LAN CONNECTION LOST"),l("BACK")};
 const auto& rules=lan->rules();
 const auto local=std::find_if(lan->players().begin(),lan->players().end(),[&](const auto& p){return p.id==lan->local_id();});
 const bool ready=local!=lan->players().end()&&local->ready;
 const auto values=lan_setting_values(lan_track());
 const std::string mark=lan->is_host()?"  < >":"";
 std::vector<std::string> rows={l("MODE")+": "+values[0]+mark,
         l("TRACK")+": "+values[1]+mark,
         l("LAPS")+": "+values[2]+(rules.mode==RaceMode::Circuit?mark:""),
         l("CAR")+": "+values[3]+"  < >",
         l("PAINT")+": "+values[4]+"  < >",
         l(ready?"NOT READY":"READY")};
 if(lan->is_host())rows.push_back(l("START RACE"));
 rows.push_back(l("LEAVE LAN LOBBY"));
 return rows;
}
void Frontend::lan_leave(){
 if(lan)lan->close();
 lan.reset();
 page=page==MenuPage::LanStart?MenuPage::Main:MenuPage::LanStart;
 row=page==MenuPage::Main?1:0;
 message.clear();
}
void Frontend::setup_master_server(const fs::path& file){
 master_settings=file;
 bed_settings=load_openfo_bed_settings(file.parent_path()/"openfo_scripts"/"settings.bed");
 master_address=bed_settings.master_server.value_or(OPENFO_MASTER_SERVER);
 try{
  if(fs::exists(file)){
   const auto contents=read_file(file,1024);
   std::string saved(contents.begin(),contents.end());
   if(saved.compare(0,3,"\xef\xbb\xbf")==0)saved.erase(0,3);
   const auto first=saved.find_first_not_of(" \t\r\n");
   const auto last=saved.find_last_not_of(" \t\r\n");
   if(first==std::string::npos)return;
   saved=saved.substr(first,last-first+1);
   // The original test VM address was replaced during the cloud deployment.
   if(master_address=="34.118.26.31:27267"&&
      (saved=="34.116.190.254"||saved=="34.116.190.254:27267"))return;
   master_address=std::move(saved);
  }
 }catch(const std::exception&){}
}
void Frontend::poll_room_directory(){
 if(room_creation.valid()&&room_creation.wait_for(std::chrono::seconds(0))==std::future_status::ready){
  try{
   const auto port=room_creation.get();
   if(page==MenuPage::LanStart){
    auto session=std::make_shared<LanSession>();
    session->join(LanSession::room_address(master_address,port),selected_player(*this));
    lan=std::move(session);page=MenuPage::LanLobby;row=0;message.clear();
   }
  }catch(const std::exception& e){if(page==MenuPage::LanStart)message=e.what();}
 }
 if(!room_lookup.valid()||room_lookup.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
 try{online_rooms=room_lookup.get();if(page==MenuPage::LanRooms&&online_rooms.empty())message="NO ONLINE ROOMS";}
 catch(const std::exception& e){if(page==MenuPage::LanRooms)message=e.what();}
}
void Frontend::lan_character(uint32_t code){
 if(page!=MenuPage::LanStart||(row!=1&&row!=3&&row!=6))return;
 if(row==6){
  if(code==8){
   if(!room_draft.empty()){room_draft.pop_back();while(!room_draft.empty()&&(static_cast<unsigned char>(room_draft.back())&0xc0)==0x80)room_draft.pop_back();}
   return;
  }
  // WM_CHAR supplies UTF-16 code units. BMP letters (including Ukrainian) are
  // encoded as UTF-8; control characters and isolated surrogates are rejected.
  if(code<32||code==127||code>=0xd800&&code<=0xdfff)return;
  std::string bytes;
  if(code<0x80)bytes=char(code);
  else if(code<0x800){bytes+=char(0xc0|(code>>6));bytes+=char(0x80|(code&63));}
  else if(code<=0xffff){bytes+=char(0xe0|(code>>12));bytes+=char(0x80|((code>>6)&63));bytes+=char(0x80|(code&63));}
  if(room_draft.size()+bytes.size()<=60)room_draft+=bytes;
  return;
 }
 auto& address=row==1?lan_address:master_address;
 if(code==8){if(!address.empty())address.pop_back();return;}
 if(address.size()<253&&((code>='0'&&code<='9')||(code>='A'&&code<='Z')||
     (code>='a'&&code<='z')||code=='.'||code==':'||code=='-'))address.push_back(char(code));
}
void Frontend::lan_key(int k){
 if(k==27){lan_leave();return;}
 const auto count=int(lan_labels().size());
 row=std::clamp(row,0,count-1);
 if(k==38||k==40||k==9){row=(row+count+(k==38?-1:1))%count;return;}
 try{
  if(page==MenuPage::LanStart){
   if(k!=13)return;
   if(row==7){lan_leave();return;}
   if(row==6){row=5;return;}
   if(row==5){
    if(room_creation.valid())return;
    if(room_draft.empty()){message="ENTER ROOM NAME";row=6;return;}
    if(master_address.empty()){message="ENTER MASTER SERVER DNS";row=3;return;}
    room_creation=std::async(std::launch::async,[address=master_address,name=room_draft]{
     return LanSession::create_room(address,name);
    });
    message="CREATING ONLINE ROOM";return;
   }
   if(row==1||row==3){++row;return;}
   if(row==4){
    if(master_address.empty()){message="ENTER MASTER SERVER DNS";row=3;return;}
    if(room_lookup.valid())return;
    room_lookup=std::async(std::launch::async,[address=master_address]{return LanSession::list_rooms(address);});
    online_rooms.clear();message.clear();page=MenuPage::LanRooms;row=0;
    if(!master_settings.empty()){
     try{
      if(master_address!=bed_settings.master_server.value_or(OPENFO_MASTER_SERVER)||fs::exists(master_settings))
       write_file_atomic(master_settings,master_address+"\n");
     }
     catch(const std::exception&){message="Cannot save online_server.txt";}
    }
    return;
   }
   if(cars.empty())throw std::runtime_error("No cars available for LAN");
   auto session=std::make_shared<LanSession>();
   if(row==0){
    LanRules rules;
    auto available=track_choices(*this,rules.mode);
    if(available.empty()){rules.mode=RaceMode::FreeRoam;available=track_choices(*this,rules.mode);}
    if(available.empty())throw std::runtime_error("No tracks available for LAN");
    set_lan_track(rules,*available.front());
    session->host(lan_default_port,selected_player(*this),rules);
   }else session->join(lan_address,selected_player(*this));
   lan=std::move(session);page=MenuPage::LanLobby;row=0;message.clear();
   return;
  }
  if(page==MenuPage::LanRooms){
   if(k!=13)return;
   if(row==count-1){lan_leave();return;}
   if(room_lookup.valid()||size_t(row)>=online_rooms.size()||!online_rooms[size_t(row)].open)return;
   auto session=std::make_shared<LanSession>();
   session->join(LanSession::room_address(master_address,online_rooms[size_t(row)].port),selected_player(*this));
   lan=std::move(session);page=MenuPage::LanLobby;row=0;message.clear();return;
  }
  if(!lan)return;
  if(lan->phase()==LanSession::Phase::Connecting||lan->phase()==LanSession::Phase::Failed){
   if(k==13)lan_leave();
   return;
  }
  if(lan->phase()!=LanSession::Phase::Lobby)return;
  if(row==count-1&&k==13){lan_leave();return;}
  if(row==5&&k==13){
   const auto p=std::find_if(lan->players().begin(),lan->players().end(),[&](const auto& member){return member.id==lan->local_id();});
   if(p!=lan->players().end()&&selected_track(*this,lan->rules())){lan->set_ready(!p->ready);message.clear();}
   else message="Selected LAN track is missing on this computer";
   return;
  }
  if(row==6&&lan->is_host()&&k==13){
   if(!lan->start())message="All players must be ready";
   return;
  }
  if(k!=37&&k!=39&&k!=13)return;
  const int delta=k==37?-1:1;
  if(row==3||row==4){
   if(cars.empty())return;
   if(row==3)select_car(size_t((int(car_index)+int(cars.size())+delta)%int(cars.size())));
   else cycle_skin(delta);
   lan->set_car(selected_car_id(),choice.skin);return;
  }
  if(!lan->is_host())return;
  auto rules=lan->rules();
  if(row==0){
   for(int step=1;step<=4;++step){
    auto mode=RaceMode((int(rules.mode)+4+step*delta)%4);
    auto tracks=track_choices(*this,mode);
    if(!tracks.empty()){rules.mode=mode;set_lan_track(rules,*tracks.front());break;}
   }
  }else if(row==1){
   auto tracks=track_choices(*this,rules.mode);
   const auto t=std::find(tracks.begin(),tracks.end(),selected_track(*this,rules));
   if(!tracks.empty()){
    const int index=t==tracks.end()?0:int(t-tracks.begin());
    set_lan_track(rules,*tracks[size_t((index+int(tracks.size())+delta)%int(tracks.size()))]);
   }
  }else if(row==2&&rules.mode==RaceMode::Circuit)rules.laps=std::clamp(rules.laps+delta,1,9);
  else return;
  lan->set_rules(rules);message.clear();
 }catch(const std::exception& e){message=e.what();}
}
bool Frontend::apply_lan_match(){
 if(!lan)return false;
 const auto& rules=lan->rules();
 const auto track=selected_track(*this,rules);
 if(!track){message="Selected LAN track is missing on this computer";return false;}
 const auto player=std::find_if(lan->players().begin(),lan->players().end(),[&](const auto& p){return p.id==lan->local_id();});
 if(player==lan->players().end())return false;
 const auto car=std::find_if(cars.begin(),cars.end(),[&](const auto& c){return c.number==player->car_id;});
 if(car==cars.end()){message="Selected LAN car is missing on this computer";return false;}
 career=false;choice.career_race=false;choice.time_trial=false;choice.track_project.clear();
 select_car(size_t(car-cars.begin()));choice.skin=player->skin;
 const auto original=std::find_if(track_slots.begin(),track_slots.end(),[&](const auto& t){return &t==track;});
 if(original!=track_slots.end())track_slot=size_t(original-track_slots.begin());
 choice.track=track->track;choice.stage=rules.stage;choice.laps=rules.laps;choice.level=track->level;choice.bots=0;
 choice.environment=rules.environment;choice.subevent=rules.subevent;
 choice.bonus_free_roam=rules.mode==RaceMode::FreeRoam;
 choice.event=rules.mode==RaceMode::Battle?"DERBY":rules.mode==RaceMode::Stunt?"STUNT":rules.mode==RaceMode::FreeRoam?"FREEROAM":"RACE";
 return true;
}
}
