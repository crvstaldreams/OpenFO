#include "ui_ttf.hpp"
#include "ui_menu_layout.hpp"
#include "render_quality.hpp"
#include "frontend.hpp"
#include "file_io.hpp"
#include "legacy_ui/car_config.hpp"
#include <set>
#include <sstream>
#include <cstring>
#include "legacy_ui/race_ui.hpp"
#include <algorithm>
#include <regex>
#include <cmath>
#include <fstream>
namespace ofo {
namespace {
struct NavigationChange {
 Frontend& menu;std::string before;
 static std::string state(const Frontend&m){auto labels=m.labels();std::string focused=labels.empty()?"":labels[size_t(std::clamp(m.row,0,int(labels.size()-1)))];return focused+":"+ std::to_string(int(m.page))+":"+std::to_string(m.row)+":"+std::to_string(m.car_index)+":"+std::to_string(m.choice.skin)+":"+std::to_string(m.track_slot)+":"+std::to_string(m.upgrade_index)+":"+std::to_string(m.choice.laps)+":"+std::to_string(m.choice.bots)+":"+std::to_string(m.music_volume)+":"+std::to_string(m.race_music_volume)+":"+std::to_string(m.effects_volume)+":"+std::to_string(m.credits_index);}
 NavigationChange(Frontend&m):menu(m),before(state(m)){}
 ~NavigationChange(){if(state(menu)!=before)++menu.navigation_serial;}
};
bool circuit(const std::string& track){return race_mode(track)==RaceMode::Circuit;}}
Frontend::Frontend(const Resources&r,const fs::path&profile_dir,const std::string&requested):career_rules(r){assets=&r;upgrades=menu_upgrades(r);cars=menu_car_catalog(r);track_slots=menu_track_catalog(r);alpha_tracks=alpha_track_catalog(r);if(!cars.empty()){choice.car=cars[0].scene_index;}for(auto&p:r.paths())if(p.rfind("alpha_content/",0)!=0&&p.size()>15&&p.substr(p.size()-15)=="/track_geom.w32"&&p.find("/menu/")==std::string::npos)tracks.push_back(ui_canonical(p));if(tracks.empty())tracks.push_back("tracks/arena/arena1/geometry/track_geom.w32");auto it=std::find(tracks.begin(),tracks.end(),ui_canonical(requested.empty()?"data/tracks/arena/arena1/geometry/track_geom.w32":requested));if(it!=tracks.end())track_index=it-tracks.begin();choice.track=tracks[track_index];if(mode()==RaceMode::Battle)choice.bots=5;
 try{auto source=text_decode(r.read("data/scripts/levels.bed"));std::regex row(R"rx(Name\s*=\s*"([^"]+)"[\s\S]*?Laps\s*=\s*([0-9]+)[\s\S]*?TrackPath\s*=\s*BaseLevelPath\s*\.\.\s*"([^"]+)")rx");for(std::sregex_iterator i(source.begin(),source.end(),row),end;i!=end;++i){auto path=ui_canonical("data/tracks/"+(*i)[3].str()+"geometry/track_geom.w32");names[path]=(*i)[1];default_laps[path]=std::stoi((*i)[2])>0?std::clamp(std::stoi((*i)[2]),1,9):3;}if(default_laps.count(choice.track))choice.laps=default_laps[choice.track];}catch(...){}
 try{auto mapping=text_decode(r.read("data/scripts/trackmapping.bed"));std::regex file(R"rx(Filename\s*=\s*"([^"]+)")rx");std::vector<std::string> files;for(std::sregex_iterator i(mapping.begin(),mapping.end(),file),end;i!=end;++i)files.push_back(ui_canonical("data/menu/bg/"+(*i)[1].str()));for(auto&track:tracks){auto geometry=track.find("/geometry/");auto folder=track.substr(0,geometry);auto slug=folder.substr(folder.find_last_of('/')+1);for(auto&f:files){auto stem=fs::path(f).stem().string();if((stem==slug||stem==slug+"_1")&&r.exists(f)){previews[track]=f;if(stem==slug+"_1")break;}}}}catch(...){}
 for(const auto& track:alpha_tracks){names[track.track]=track.name;previews[track.track]=track.preview;}
 page=MenuPage::Start;
 if(!profile_dir.empty())init_profiles(profile_dir);

}
std::vector<size_t> Frontend::class_cars()const{std::vector<size_t> result;for(size_t i=0;i<cars.size();++i)if(cars[i].car_class==selected_class)result.push_back(i);return result;}
std::string Frontend::title()const{if(lan_page())return page==MenuPage::LanRooms?"ONLINE ROOMS":page==MenuPage::LanStart?"MULTIPLAYER":lan&&!lan->room_name().empty()?lan->room_name():"LAN LOBBY";switch(page){case MenuPage::Start:return "";case MenuPage::Profile:return "SELECT PROFILE";case MenuPage::CareerAward:return "CAREER RESULTS";case MenuPage::SmashBonus:return "OBJECT BONUS";case MenuPage::Garage:return "GARAGE";case MenuPage::Tuning:return "TUNING";case MenuPage::Upgrade:return "UPGRADE";case MenuPage::Stats:return "STATISTICS";case MenuPage::GameOptions:return "OPTIONS";case MenuPage::Main:return "MAIN MENU";case MenuPage::SinglePlayer:return "SINGLE PLAYER";case MenuPage::Options:return "OPTIONS / AUDIO";case MenuPage::Controls:return "CONTROLS";case MenuPage::Credits:return "CREDITS";case MenuPage::RaceSetup:return "SELECT RACE";case MenuPage::ClassSelect:return "SELECT CLASS";case MenuPage::AlphaTracks:return "ALPHA CONTENT";case MenuPage::TrackProjects:return "CUSTOM TRACKS";case MenuPage::CarSelect:return "CAR DEALER";case MenuPage::Transmission:return "SELECT TRANSMISSION";case MenuPage::Pause:return "PAUSED";case MenuPage::Confirm:return pending_action==1?"RESTART RACE?":"RETIRE FROM RACE?";}return {};}
std::vector<std::string> Frontend::labels()const{
 if(lan_page())return lan_labels();
 switch(page){case MenuPage::CareerAward:case MenuPage::SmashBonus:return {"CONTINUE"};case MenuPage::Start:return {"PRESS ENTER"};case MenuPage::Profile:{if(profile_editing)return {"NAME: "+profile_draft+"_",std::string("DRIVER: ")+driver_model_label(driver_model_from_id(profile_draft_gender)),std::string("HANDLING: ")+(profile_draft_difficulty==1?"NORMAL":"PRO"),"CREATE PROFILE"};std::vector<std::string> list;for(const auto&p:player_profiles)list.push_back(p.name+(p.id==profile_id?"  [ACTIVE]":""));list.push_back(profile_root.empty()?"PLAYER":"NEW PROFILE");return list;}case MenuPage::Garage:return {"CAREER EVENTS","TUNING","CAR DEALER","STATISTICS"};case MenuPage::Tuning:return {"ENGINE","TRANSMISSION","EXHAUST","SUSPENSION","WHEELS","BODY"};case MenuPage::Upgrade:{auto ids=category_upgrades();return ids.empty()?std::vector<std::string>{"NO PARTS"}:std::vector<std::string>{upgrades[ids[upgrade_index%ids.size()]].title};}case MenuPage::Stats:return {"BACK"};case MenuPage::GameOptions:return {std::string("IMPERIAL UNITS: ")+(imperial?"ON":"OFF"),std::string("DIRECTION ARROW: ")+(arrow_visible?"ON":"OFF"),std::string("TRACK MAP: ")+(map_visible?"ON":"OFF"),std::string("LOD: ")+(lod_enabled?"ON":"OFF"),std::string("DETAIL: ")+render_quality_name(detail_level),"BLOOM: "+std::to_string(int(std::round(bloom_intensity*100)))+"%",std::string("AMBIENT OCCLUSION: ")+(ambient_occlusion?"ON":"OFF"),std::string("WATER SSR: ")+(water_ssr?"ON":"OFF"),"DISPLAY RESOLUTION: "+std::to_string(screen_width)+" x "+std::to_string(screen_height),"3D RENDER SCALE: "+std::to_string(render_scale_percent)+"%",std::string("FULLSCREEN: ")+(fullscreen?"ON":"OFF"),std::string("LANGUAGE: ")+(language==UiLanguage::English?"ENGLISH":"УКРАЇНСЬКА"),std::string("AUTO LOW-SPEC OPTIMIZATION: ")+(auto_performance?"ON":"OFF"),std::string("VSYNC: ")+(vsync?"ON":"OFF"),"FPS LIMIT: "+std::to_string(std::clamp(fps_limit,30,200)),std::string("REARVIEW MIRROR: ")+(mirror_enabled?"ON":"OFF"),"CPU THREADS: AUTO ("+std::to_string(active_cpu_lanes)+")"};case MenuPage::Main:return {"SINGLE PLAYER", "MULTIPLAYER", "OPTIONS", "SELECT PROFILE", "QUIT GAME"};
 case MenuPage::SinglePlayer:{
  std::vector<std::string> entries{career_progress.initialized?"CONTINUE CAREER":"START CAREER","QUICK RACE","TIME TRIAL","MENU WORLD FREE ROAM"};
  if(!project_directory.empty())entries.push_back("CUSTOM TRACKS");
  return entries;
 }
 case MenuPage::TrackProjects:{
  std::vector<std::string> entries;
  for(const auto& project:track_projects)entries.push_back(project.name);
  return entries.empty()?std::vector<std::string>{"NO CUSTOM TRACKS"}:entries;
 }
 case MenuPage::Options:return {"MENU MUSIC: "+std::to_string(music_volume)+"%", "RACE MUSIC: "+std::to_string(race_music_volume)+"%", "EFFECTS: "+std::to_string(effects_volume)+"%", "CONTROLS", "BACK"};
 case MenuPage::Controls:{std::vector<std::string> out;for(int i=0;i<ControlSettings::Count;++i)out.push_back(std::string(ControlSettings::action_name(i))+": "+(binding==i?"PRESS KEY (ESC CANCEL)":ControlSettings::key_name(controls.keys[i])+(controls.secondary[i]?" / "+ControlSettings::key_name(controls.secondary[i]):"")));out.push_back("XBOX DEADZONE: "+std::to_string(int(std::round(controls.deadzone*100)))+"%  < >");out.push_back("XBOX SENSITIVITY: "+std::to_string(int(std::round(controls.sensitivity*100)))+"%  < >");out.push_back("RESTORE DEFAULTS");out.push_back("BACK");return out;}
 case MenuPage::Credits:return {"PAGE "+std::to_string(credits_index)+" / 34  <  >", "BACK"};
 case MenuPage::RaceSetup:return {"TRACK: "+(names.count(choice.track)?names.at(choice.track):fs::path(choice.track).parent_path().parent_path().filename().string()),(mode()==RaceMode::Battle?"MODE: LAST MAN STANDING":stunt_mode()?"MODE: STUNT":"LAPS: "+std::to_string(choice.laps)),choice.time_trial?"OPPONENTS: NONE (TIME TRIAL)":"OPPONENTS: "+std::to_string(choice.bots),"START RACE",std::string("TIME OF DAY: ")+(choice.environment.night?"NIGHT":"DAY"),std::string("WEATHER: ")+(choice.environment.rain?"RAIN":"DRY"),"AI HANDICAP: "+std::to_string(choice.ai_handicap_level)};
 case MenuPage::ClassSelect:if(career)return {"CLASS C",std::string("CLASS B")+(career_progress.highest_class<2?" (LOCKED)":""),std::string("CLASS A")+(career_progress.highest_class<3?" (LOCKED)":""),"BONUS"};{std::vector<std::string> classes{"CLASS C","CLASS B","CLASS A"};if(bonus_class_visible())classes.push_back("BONUS");if(!alpha_tracks.empty())classes.push_back("ALPHA CONTENT");return classes;}
 case MenuPage::AlphaTracks:{std::vector<std::string> result;for(const auto& track:alpha_tracks)result.push_back(track.name);return result;}
 case MenuPage::CarSelect:return {(cars.empty()?"NO CARS":cars[car_index].name)+"  <  >", "PAINT: "+(choice.skin>=101?"AI "+std::to_string(choice.skin-100):std::to_string(choice.skin))+"  <  >", (career?(!cars.empty()&&career_progress.car==cars[car_index].number?"USE CAR":(!cars.empty()&&career_car_unlocked(cars[car_index].car_class)?"BUY / TRADE CAR":"LOCKED")):"CONTINUE")};
 case MenuPage::Transmission:return {"AUTOMATIC", "MANUAL"};
 case MenuPage::Pause:if(lan)return {"CONTINUE GAME","LEAVE LAN RACE"};if(stunt_mode())return {"CONTINUE EVENT","START AGAIN","RETURN TO MENU","PHOTO MODE"};return {"CONTINUE GAME", "RESTART RACE", "RETIRE", "SETTINGS", "REPLAY", "PHOTO MODE"};case MenuPage::Confirm:return {"CANCEL","YES"};}return {};
}
void Frontend::open_pause(){pause_context=true;page=MenuPage::Pause;row=0;start=resume=recover=restart=retire=replay=quit=false;pending_action=0;}
void Frontend::back(){if(lan_page()){lan_leave();return;}if(!profile_delete_id.empty()){profile_delete_id.clear();profile_delete_yes=false;return;}if(profile_editing){profile_editing=false;profile_draft.clear();message.clear();return;}if(binding>=0){binding=-1;message.clear();return;}row=0;switch(page){case MenuPage::SmashBonus:page=MenuPage::CareerAward;break;case MenuPage::CareerAward:restore_career_car();page=MenuPage::Garage;break;case MenuPage::Start:break;case MenuPage::Profile:page=MenuPage::Start;break;case MenuPage::Garage:career=false;page=MenuPage::SinglePlayer;break;case MenuPage::Tuning:page=MenuPage::Garage;row=1;break;case MenuPage::Upgrade:page=MenuPage::Tuning;row=tuning_category;break;case MenuPage::Stats:page=MenuPage::Garage;row=3;break;case MenuPage::GameOptions:page=options_return;break;case MenuPage::TrackProjects:page=MenuPage::SinglePlayer;row=4;break;case MenuPage::CarSelect:if(!choice.track_project.empty()&&!career){choice.track_project.clear();page=MenuPage::TrackProjects;break;}if(career)restore_career_car();page=career?MenuPage::Garage:MenuPage::SinglePlayer;row=choice.time_trial?2:1;break;case MenuPage::Transmission:page=MenuPage::CarSelect;row=0;break;case MenuPage::AlphaTracks:page=MenuPage::ClassSelect;row=bonus_class_visible()?4:3;break;case MenuPage::ClassSelect:page=career?MenuPage::Garage:MenuPage::Transmission;row=choice.automatic?0:1;break;case MenuPage::SinglePlayer:page=MenuPage::Main;break;case MenuPage::Options:page=options_return;break;case MenuPage::Controls:page=MenuPage::Options;row=3;break;case MenuPage::Credits:page=MenuPage::Main;break;case MenuPage::RaceSetup:page=pause_context?MenuPage::Pause:MenuPage::ClassSelect;row=selected_class-1;break;case MenuPage::Confirm:page=MenuPage::Pause;row=pending_action==1?1:2;pending_action=0;break;case MenuPage::Pause:resume=true;break;case MenuPage::Main:if(pause_context)resume=true;break;}}
void Frontend::save_controls(){if(!controls_path.empty()&&!controls.save(controls_path))message="Cannot save controls.json";}
void Frontend::key(int k){
 NavigationChange navigation(*this);
 if(lan_page()){lan_key(k);return;}
 if(page==MenuPage::Profile&&!profile_editing){
  if(!profile_delete_id.empty()){
   if(k==27||k==8){back();return;}
   if(k==37||k==38)profile_delete_yes=false;
   if(k==39||k==40)profile_delete_yes=true;
   if(k==13)confirm_profile_delete();
   return;
  }
  if(k==46&&row>=0&&size_t(row)<player_profiles.size()){
   profile_delete_id=player_profiles[size_t(row)].id;profile_delete_yes=false;return;
  }
 }
 if(page==MenuPage::Profile&&profile_editing){if(k==27){back();return;}if(k==38||k==40){row=(row+4+(k==38?-1:1))%4;return;}if(k==37||k==39){if(row==1)profile_draft_gender=step_driver_model(profile_draft_gender,k==37?-1:1);if(row==2)profile_draft_difficulty=3-profile_draft_difficulty;return;}if(k==13){if(row==1){profile_draft_gender=step_driver_model(profile_draft_gender,1);return;}if(row==2){profile_draft_difficulty=3-profile_draft_difficulty;return;}if(create_profile(profile_draft)){profile_editing=false;page=MenuPage::Main;row=0;}}return;}

 if(binding>=0){if(k==27){binding=-1;message.clear();return;}if(controls.assign(binding,k)){binding=-1;message="Controls saved";save_controls();}else message="Reserved key; choose another, ESC cancels";return;}

 if((k==9||k==33||k==34)&&(page==MenuPage::Options||page==MenuPage::GameOptions||page==MenuPage::Controls)){const MenuPage tabs[]={MenuPage::GameOptions,MenuPage::Options,MenuPage::Controls};int tab=page==MenuPage::GameOptions?0:page==MenuPage::Options?1:2;page=tabs[(tab+(k==33?2:1))%3];row=0;message.clear();return;}
 if(page==MenuPage::Stats){
  const int installed=cars.empty()?0:int(car_upgrades(cars[car_index].number).size());
  if(k==38)stats_scroll=std::max(0,stats_scroll-1);
  if(k==40)stats_scroll=std::min(std::max(0,installed-3),stats_scroll+1);
  if(k==13||k==27||k==8)back();
  return;
 }
 if(page==MenuPage::Upgrade&&(k==37||k==39)){auto ids=category_upgrades();if(!ids.empty())upgrade_index=(upgrade_index+ids.size()+(k==37?-1:1))%ids.size();upgrade_detail_scroll=0;return;}
 if(page==MenuPage::Upgrade&&(k==38||k==40)){
  const auto ids=category_upgrades();
  if(!ids.empty()){
   const int count=int(upgrade_effect_lines(ids[upgrade_index%ids.size()]).size());
   upgrade_detail_scroll=std::clamp(upgrade_detail_scroll+(k==38?-1:1),0,std::max(0,count-3));
  }
  return;
 }
 if(k==27||k==8){back();return;}
 if(page==MenuPage::AlphaTracks){
  const int count=int(alpha_tracks.size());
  if(!count)return;
  row=std::clamp(row,0,count-1);
  if(k==37||k==38)row=(row+count-1)%count;
  if(k==39||k==40)row=(row+1)%count;
  if(k==13)select_alpha_track(size_t(row));
  return;
 }
 if(page==MenuPage::CarSelect&&(k==38||k==40)){cycle_skin(k==38?-1:1);row=1;return;}
 if(page==MenuPage::RaceSetup&&(k==9||((k==38||k==40)&&row!=0))){const int order[]={0,1,2,4,5,6,3};int i=0;for(int n=0;n<7;++n)if(order[n]==row)i=n;row=order[(i+7+(k==38?-1:1))%7];return;}
 if(page==MenuPage::RaceSetup&&row==0&&!track_slots.empty()&&(k==37||k==38||k==39||k==40)){
  const auto current=track_slots[track_slot];int col=current.column+(k==37?-1:k==39?1:0),line=current.row+(k==38?-1:k==40?1:0);
  bool moved=false;for(auto i:class_tracks())if(track_slots[i].column==col&&track_slots[i].row==line){select_track(i);moved=true;break;}if(!moved&&k==40)row=1;if(!moved&&k==38)row=6;
  return;
 }
 auto count=int(labels().size());if(!count)return;if(k==38)row=(row+count-1)%count;if(k==40)row=(row+1)%count;
 int delta=k==37?-1:k==39?1:0;
 if(delta&&(page==MenuPage::CareerAward||page==MenuPage::Start||page==MenuPage::Garage||page==MenuPage::Tuning||page==MenuPage::Main||page==MenuPage::SinglePlayer||page==MenuPage::Transmission||page==MenuPage::Pause||page==MenuPage::Confirm)){row=(row+count+delta)%count;if(page==MenuPage::Tuning)tuning_category=row;return;}
 if(page==MenuPage::RaceSetup&&delta&&!career){if(row==0&&class_tracks().empty()){do{track_index=(track_index+tracks.size()+delta)%tracks.size();}while(!circuit(tracks[track_index])&&std::any_of(tracks.begin(),tracks.end(),circuit));choice.track=tracks[track_index];choice.stage.clear();choice.event.clear();choice.subevent.clear();apply_track_settings();if(mode()==RaceMode::Battle&&!choice.time_trial)choice.bots=5;if(default_laps.count(choice.track))choice.laps=default_laps[choice.track];}if(row==1&&mode()==RaceMode::Circuit)choice.laps=std::clamp(choice.laps+delta,1,9);if(row==2&&!choice.time_trial&&!stunt_mode())choice.bots=std::clamp(choice.bots+delta,mode()==RaceMode::Battle?1:0,7);}
 if(page==MenuPage::RaceSetup&&delta){if(row==4){choice.environment.night=!choice.environment.night;remember_track_settings();}if(row==5){choice.environment.rain=!choice.environment.rain;remember_track_settings();}if(row==6){const int levels[]={1,4,7};int i=choice.ai_handicap_level<=2?0:choice.ai_handicap_level<=5?1:2;choice.ai_handicap_level=levels[(i+3+delta)%3];remember_track_settings(true);}}
 if(page==MenuPage::ClassSelect&&(delta||k==38||k==40)){if(delta)row=(row+count+delta)%count;if(career||row<(bonus_class_visible()?4:3))selected_class=row+1;}
 if(page==MenuPage::CarSelect&&delta){const auto count=car_selection_count();if(count)select_car((car_index+count+delta)%count);row=0;}
 if(page==MenuPage::Controls&&delta){if(row==ControlSettings::Count)controls.deadzone=std::clamp(controls.deadzone+delta*.01,.05,.4);else if(row==ControlSettings::Count+1)controls.sensitivity=std::clamp(controls.sensitivity+delta*.1,.5,2.);save_controls();}
 if(page==MenuPage::Tuning){tuning_category=row;}
 if(page==MenuPage::GameOptions&&delta){if(row==0)imperial=!imperial;if(row==1)arrow_visible=!arrow_visible;if(row==2)map_visible=!map_visible;if(row==3){lod_enabled=!lod_enabled;message.clear();}if(row==4){detail_level=(detail_level+(delta>0?1:3))%4;message.clear();}if(row==5){bloom_intensity=std::clamp(std::round(bloom_intensity*10+delta)/10.f,0.f,2.f);bloom=bloom_intensity>0;message=detail_level==0?"Screen effects disabled at LOW detail":"";}if(row==6){ambient_occlusion=!ambient_occlusion;message=detail_level==0?"Screen effects disabled at LOW detail":"";}if(row==7){water_ssr=!water_ssr;message=detail_level==0?"Screen effects disabled at LOW detail":"";}if(row==8){if(!display_resolutions.empty()){size_t i=0;for(size_t n=0;n<display_resolutions.size();++n)if(display_resolutions[n].first==screen_width&&display_resolutions[n].second==screen_height){i=n;break;}i=(i+display_resolutions.size()+(delta>0?1:-1))%display_resolutions.size();screen_width=display_resolutions[i].first;screen_height=display_resolutions[i].second;message.clear();}else message="No monitor display modes found";}if(row==9){render_scale_percent=std::clamp(render_scale_percent+5*delta,50,200);message.clear();}if(row==10){fullscreen=!fullscreen;message.clear();}if(row==11){language=language==UiLanguage::English?UiLanguage::Ukrainian:UiLanguage::English;message.clear();}if(row==12){auto_performance=!auto_performance;message=auto_performance?"Adaptive hardware profile enabled":"Manual quality: all user-selected effects enabled";}if(row==13){vsync=!vsync;message.clear();}if(row==14){fps_limit=std::clamp(fps_limit+delta,30,200);message.clear();}if(row==15){mirror_enabled=!mirror_enabled;message.clear();}save_profile();}
 if(page==MenuPage::Options&&delta){if(row==0)music_volume=std::clamp(music_volume+delta*10,0,100);if(row==1)race_music_volume=std::clamp(race_music_volume+delta*10,0,100);if(row==2)effects_volume=std::clamp(effects_volume+delta*10,0,100);save_profile();}
 if(page==MenuPage::Credits&&delta)credits_index=(credits_index-1+34+delta)%34+1;
 if(k!=13)return;
 switch(page){case MenuPage::CareerAward:page=MenuPage::SmashBonus;row=0;break;case MenuPage::SmashBonus:restore_career_car();page=MenuPage::Garage;row=0;break;case MenuPage::Start:page=MenuPage::Profile;break;case MenuPage::Profile:if(row<int(player_profiles.size())){if(activate_profile(player_profiles[size_t(row)].id)){page=MenuPage::Main;row=0;}}else if(profile_root.empty()){page=MenuPage::Main;row=0;}else{profile_editing=true;profile_draft.clear();profile_draft_gender=0;profile_draft_difficulty=1;row=0;message="Name, driver and handling; UP/DOWN selects, LEFT/RIGHT changes";}break;case MenuPage::Garage:if(row==0){if(!career_progress.car){page=MenuPage::CarSelect;row=0;message="Buy a starter car to enter career events";}else{restore_career_car();page=MenuPage::ClassSelect;row=0;}}else if(row==1){page=MenuPage::Tuning;row=tuning_category;}else if(row==2){page=MenuPage::CarSelect;row=0;}else{page=MenuPage::Stats;stats_scroll=0;row=0;}break;case MenuPage::Tuning:tuning_category=row;upgrade_index=0;upgrade_detail_scroll=0;page=MenuPage::Upgrade;row=0;break;case MenuPage::Upgrade:buy_upgrade();break;case MenuPage::Stats:back();break;case MenuPage::GameOptions:key(39);break;case MenuPage::Main:if(row==0)page=MenuPage::SinglePlayer;else if(row==1){career=false;choice.career_race=false;choice.track_project.clear();page=MenuPage::LanStart;message.clear();}else if(row==2){options_return=page;page=MenuPage::Options;}else if(row==3){refresh_profiles();page=MenuPage::Profile;}else quit=true;row=0;break;
 case MenuPage::SinglePlayer:choice.track_project.clear();if(row==0){enter_career();break;}if(row==4){refresh_track_projects();page=MenuPage::TrackProjects;row=0;message=track_projects.empty()?"Save a track to track_projects from Track Studio":"";break;}career=false;choice.career_race=false;choice.bonus_free_roam=row==3;choice.time_trial=row==2;if(choice.bonus_free_roam){choice.track="tracks/menu/menu1/geometry/track_geom.w32";choice.stage="tracks/menu/menu1/a";choice.event="FREEROAM";choice.subevent.clear();choice.bots=0;choice.level=0;choice.laps=1;}if(choice.time_trial){choice.bots=0;if(!circuit(choice.track)){auto road=std::find_if(tracks.begin(),tracks.end(),circuit);if(road!=tracks.end()){track_index=road-tracks.begin();choice.track=*road;}}}else if(mode()==RaceMode::Battle)choice.bots=5;page=MenuPage::CarSelect;row=0;message.clear();break;
 case MenuPage::Options:if(row<3)key(39);else if(row==3){page=MenuPage::Controls;row=0;}else back();break;
 case MenuPage::Controls:if(row<ControlSettings::Count){binding=row;message="Press a key; ESC cancels";}else if(row<ControlSettings::Count+2)key(39);else if(row==ControlSettings::Count+2){controls=ControlSettings{};message="Defaults restored";save_controls();}else back();break;
 case MenuPage::Credits:if(row==0)key(39);else back();break;
 case MenuPage::RaceSetup:if(row==3||row==0){if(career&&!career_event_unlocked(choice.level)){message="Locked: finish the previous stage in the top three";break;}if(stunt_mode()&&!supported_stunt()){message="This stunt has no scoring implementation yet";break;}remember_track_settings();start=true;}else key(39);break;
 case MenuPage::AlphaTracks:break;
 case MenuPage::TrackProjects:if(row>=0&&size_t(row)<track_projects.size()){select_track_project(track_projects[size_t(row)]);page=MenuPage::CarSelect;row=0;}break;
 case MenuPage::ClassSelect:{if(!career&&!alpha_tracks.empty()&&row==(bonus_class_visible()?4:3)){page=MenuPage::AlphaTracks;row=0;message.clear();break;}if(career&&row<3&&row+1>career_progress.highest_class){message="Complete every course final in the previous class";break;}selected_class=row+1;choice.bonus_free_roam=false;auto group=class_tracks();if(!group.empty()){auto first=group.front();if(career)for(auto index:group)if(career_event_unlocked(track_slots[index].level)){first=index;break;}select_track(first);}page=MenuPage::RaceSetup;row=0;break;}
 case MenuPage::CarSelect:if(career){if(buy_career_car()){page=MenuPage::Transmission;row=choice.automatic?0:1;}}else{page=MenuPage::Transmission;row=choice.automatic?0:1;}break;
 case MenuPage::Transmission:choice.automatic=row==0;if(career){save_profile();page=MenuPage::Garage;row=0;}else if(choice.bonus_free_roam||!choice.track_project.empty()){start=true;}else{page=MenuPage::ClassSelect;row=std::clamp(selected_class-1,0,bonus_class_visible()?3:2);}break;
 case MenuPage::Pause:if(lan){if(row==0)resume=true;else retire=true;break;}if(stunt_mode()){if(row==0)resume=true;else if(row==1)restart=true;else if(row==2){retire=true;pause_context=false;}else photo=true;break;}if(row==0)resume=true;else if(row==1||row==2){pending_action=row;page=MenuPage::Confirm;row=0;}else if(row==3){options_return=page;page=MenuPage::Options;row=0;}else if(row==4)replay=true;else photo=true;break;
 case MenuPage::Confirm:if(row==0){back();break;}if(pending_action==1)restart=true;else{retire=true;pause_context=false;}pending_action=0;page=MenuPage::Pause;row=0;break;}
}
std::vector<size_t> Frontend::class_tracks()const{std::vector<size_t> out;for(size_t i=0;i<track_slots.size();++i)if(track_slots[i].menu_class==selected_class&&(!choice.time_trial||circuit(track_slots[i].track)))out.push_back(i);return out;}
bool Frontend::bonus_class_visible()const{
 return !choice.time_trial||std::any_of(track_slots.begin(),track_slots.end(),[](const MenuTrack& track){return track.menu_class==4&&circuit(track.track);});
}
void Frontend::select_track(size_t index){if(index>=track_slots.size())return;choice.track_project.clear();track_slot=index;const auto&t=track_slots[index];choice.track=t.track;choice.stage=t.stage;choice.level=t.level;choice.event=t.event;choice.subevent=t.subevent;choice.laps=t.laps>0?std::clamp(t.laps,1,9):3;choice.ai_handicap_level=choice.ai_upgrade_level=1;choice.ai_profile_class=t.menu_class<=3?t.menu_class:1;choice.career_race=career;choice.bots=stunt_mode()||choice.time_trial?0:career?7:choice.bots;names[t.track]=t.name;previews[t.track]=t.preview;auto it=std::find(tracks.begin(),tracks.end(),t.track);if(it!=tracks.end())track_index=it-tracks.begin();if(mode()==RaceMode::Battle)choice.bots=career?7:5;
 try{auto source=text_decode(assets->read("data/scripts/levels.bed"));std::regex record("\\["+std::to_string(t.level)+R"(\]\s*=\s*\{\s*Name\s*=[\s\S]*?StagePath)");std::smatch m;if(std::regex_search(source,m,record)){auto block=m[0].str();std::smatch v;if(std::regex_search(block,v,std::regex(R"(\bClass\s*=\s*([1-3]))")))choice.ai_profile_class=std::stoi(v[1]);if(std::regex_search(block,v,std::regex(R"(AIHandicapLevel\s*=\s*([0-9]+))")))choice.ai_handicap_level=std::stoi(v[1]);if(std::regex_search(block,v,std::regex(R"(AIUpgradeLevel\s*=\s*([0-9]+))")))choice.ai_upgrade_level=std::stoi(v[1]);}}catch(const std::exception&){}apply_track_settings();}
void Frontend::select_alpha_track(size_t index){
 if(career||index>=alpha_tracks.size())return;
 choice.track_project.clear();
 const auto& track=alpha_tracks[index];
 choice.track=track.track;choice.stage=track.stage;choice.level=0;
 choice.event="FREEROAM";choice.subevent.clear();choice.career_race=false;
 choice.bonus_free_roam=true;choice.bots=0;choice.laps=1;
 choice.ai_profile_class=choice.ai_handicap_level=choice.ai_upgrade_level=1;
 apply_track_settings();message.clear();start=true;
}
size_t Frontend::car_selection_count()const{
 if(!career)return cars.size();
 return size_t(std::find_if(cars.begin(),cars.end(),[](const MenuCar& car){return car.car_class==alpha_car_class;})-cars.begin());
}
void Frontend::select_car(size_t index){
 if(index>=car_selection_count())return;
 car_index=index;choice.car=cars[index].scene_index;
 if(cars[index].car_class!=alpha_car_class)selected_class=cars[index].car_class;
 choice.skin=cars[index].skins.empty()?1:cars[index].skins.front().number;
}
void Frontend::cycle_skin(int delta){if(cars.empty())return;const auto& skins=cars[car_index].skins;if(skins.empty())return;size_t index=0;for(size_t i=0;i<skins.size();++i)if(skins[i].number==choice.skin)index=i;choice.skin=skins[(index+skins.size()+delta)%skins.size()].number;}
void Frontend::click(int x,int y,int w,int h){
 NavigationChange navigation(*this);
 if(binding>=0)return;
 if(w<=0||h<=0||x<0||y<0||x>=w||y>=h)return;
 MenuViewport viewport(w,h);if(!viewport.contains(x,y))return;double sx=(x-viewport.x)*640./viewport.width,sy=viewport.source_y(y);
 if(lan_page()){
  const int count=int(lan_labels().size());
  if(page==MenuPage::LanRooms){
   if(lan_footer_item(1).contains(sx,sy)){row=count-1;key(13);return;}
   const int rooms=room_lookup.valid()?0:int(online_rooms.size());
   const int first=lan_room_first(row,rooms);
   for(int i=first;i<std::min(first+lan_room_page_size,rooms);++i){
    if(lan_room_item(i-first).contains(sx,sy)){row=i;key(13);return;}
   }
  }else if(page==MenuPage::LanStart){
   for(int i=0;i<count;++i){
    if(!lan_start_item(i).contains(sx,sy))continue;
    row=i;
    if(i!=1&&i!=3&&i!=6)key(13);
    return;
   }
  }else if(lan&&lan->phase()==LanSession::Phase::Lobby){
   for(int i=0;i<5;++i){
    const auto hit=lan_setting_item(i,bed_settings.lobby);
    if(hit.contains(sx,sy)){row=i;key(sx<(hit.left+hit.right)*.5?37:39);return;}
   }
   if(lan_footer_item(0).contains(sx,sy)){row=5;key(13);return;}
   if(lan->is_host()&&lan_footer_item(1).contains(sx,sy)){row=6;key(13);return;}
   if(lan_footer_item(2).contains(sx,sy)){row=count-1;key(13);return;}
  }else if(lan_footer_item(1).contains(sx,sy)){
   row=count-1;
   if(lan)key(13);else lan_leave();
  }
  return;
 }
 if(page==MenuPage::GameOptions&&(row/7)==0&&sx>=370&&sx<=566&&sy>=287&&sy<307){row=5;bloom_intensity=std::clamp(float(std::round((sx-370)/196.*20)/10),0.f,2.f);bloom=bloom_intensity>0;save_profile();return;}
 // PC balk button positions from menutemplate.bed; no invisible list-sized hits.

 if((page==MenuPage::Options||page==MenuPage::GameOptions||page==MenuPage::Controls)&&sy>=393&&sy<429){page=sx<230?MenuPage::GameOptions:sx<415?MenuPage::Options:MenuPage::Controls;row=0;return;}
 if(page==MenuPage::Profile&&!profile_delete_id.empty()){
  if(sy>=249&&sy<287&&sx>=92&&sx<548){
   profile_delete_yes=sx>=320;confirm_profile_delete();
  }
  return;
 }
 if(sy>=448&&sy<466){if(sx>=350&&sx<440){back();return;}if(sx>=453&&sx<575){key(13);return;}}
 if(page==MenuPage::Profile&&sy>=140&&sy<340&&!profile_editing){int first=(row/5)*5;int hit=int((sy-140)/34)+first;if(sx>=55&&sx<582&&sy<310&&hit<int(labels().size())){row=hit;key(sx>=482&&size_t(hit)<player_profiles.size()?46:13);}return;}
 if(page==MenuPage::Profile&&profile_editing&&sx>=68&&sx<570&&sy>=174&&sy<302){row=std::clamp(int((sy-174)/32),0,3);if(row==3)key(13);else if(row)key(sx<320?37:39);return;}
 if((page==MenuPage::Options||page==MenuPage::Controls)&&sx>=(page==MenuPage::Controls?211:370)&&sx<=(page==MenuPage::Controls?315:566)&&sy>=160&&sy<352){int first=(page==MenuPage::Controls||page==MenuPage::TrackProjects)?(row/6)*6:0;int hit=int((sy-160)/32)+first;double t=page==MenuPage::Controls?std::clamp((sx-211)/104.,0.,1.):std::clamp((sx-370)/196.,0.,1.);if(page==MenuPage::Options&&hit<3){row=hit;int v=int(std::round(t*100));if(hit==0)music_volume=v;if(hit==1)race_music_volume=v;if(hit==2)effects_volume=v;save_profile();return;}if(page==MenuPage::Controls&&(hit==ControlSettings::Count||hit==ControlSettings::Count+1)){row=hit;if(hit==ControlSettings::Count)controls.deadzone=.05+t*.35;else controls.sensitivity=.5+t*1.5;save_controls();return;}}
 if(page==MenuPage::AlphaTracks){
  for(size_t i=0;i<alpha_tracks.size();++i){const float left=alpha_card_left(i);
   if(sx>=left&&sx<left+alpha_card_width&&sy>=alpha_card_top&&sy<alpha_card_top+alpha_card_height){row=int(i);key(13);return;}
  }
  return;
 }
 if(page==MenuPage::CarSelect){
  if(career&&sx>=390&&sx<600&&sy>=330&&sy<356){row=2;key(13);return;}
  if(!cars.empty()&&sx>=557&&sx<590){for(size_t i=0;i<cars[car_index].skins.size();++i){double top=183+i*19;if(sy>=top&&sy<top+16){choice.skin=cars[car_index].skins[i].number;row=1;return;}}}
  const int count=int(car_selection_count());
  if(sy>=380&&sy<448&&count){float visual=car_scroll.context==int(page)?car_scroll.value:float(car_index);for(int d=-std::min(3,count/2);d<=std::min(3,(count-1)/2);++d){int raw=int(car_index)+d;float left=280+(raw-visual)*109;if(sx>=left&&sx<left+79){select_car(size_t((raw+count)%count));row=0;break;}}return;}
 }
 if(page==MenuPage::Upgrade&&sy>=380&&sy<448){auto ids=category_upgrades();if(ids.empty())return;float visual=part_scroll.context==int(page)?part_scroll.value:float(upgrade_index);for(int d=-std::min(3,int(ids.size()/2));d<=std::min(3,int((ids.size()-1)/2));++d){int raw=int(upgrade_index)+d;float left=280+(raw-visual)*91;if(sx>=left&&sx<left+79){upgrade_index=size_t((raw+int(ids.size()))%int(ids.size()));break;}}return;}
 if(page==MenuPage::RaceSetup){
  if(sy>=393&&sy<429&&sx>=245&&sx<410){row=3;key(13);return;}
  // Visible parameter rows have matching, non-overlapping mouse regions.
  if(sx>=392&&sx<592&&sy>=149&&sy<279){const int rows[]={1,2,4,5,6};row=rows[std::clamp(int((sy-149)/26),0,4)];key(sx<500?37:39);return;}
  for(auto index:class_tracks()){const auto&t=track_slots[index];if(sx>=t.x&&sx<t.x+68&&sy>=t.y&&sy<t.y+41){select_track(index);row=0;return;}}
  return;
 }

 if(page==MenuPage::SmashBonus||page==MenuPage::CareerAward||page==MenuPage::Start||page==MenuPage::Garage||page==MenuPage::Tuning||page==MenuPage::Main||page==MenuPage::SinglePlayer||page==MenuPage::ClassSelect||page==MenuPage::Transmission||page==MenuPage::Pause||page==MenuPage::Confirm){if(sy>=393&&sy<429&&sx>=8&&sx<632){
  auto positions=horizontal_positions(-1,float(viewport.width/640.));
  const double visible_time=animation_time>=0?animation_time:menu_scroll.time;
  const float pulse=selection_brightness(visible_time);
  for(size_t i=0;i<positions.size();++i){
   const float fade=ui_menu_edge_alpha(positions[i].first+positions[i].second*.5f);
   // Match the physical command's alpha quantization, including active pulse.
   const uint32_t alpha=uint32_t(255*fade*(int(i)==row?.8f+.2f*pulse:.72f));
   if(alpha&&sx>=positions[i].first&&sx<positions[i].first+positions[i].second){row=int(i);if(page==MenuPage::Tuning)tuning_category=row;key(13);return;}
  }
 }return;}
 if(sx<48||sx>(page==MenuPage::Controls?324:590)||sy<160)return;int hit=int((sy-160)/(page==MenuPage::GameOptions?25:32))+((page==MenuPage::Controls||page==MenuPage::TrackProjects)?(row/6)*6:page==MenuPage::GameOptions?(row/7)*7:0);if(hit>=int(labels().size())||((page==MenuPage::Controls||page==MenuPage::TrackProjects)&&sy>=352))return;row=hit;key(13);
}
std::vector<std::pair<float,float>> Frontend::horizontal_positions(double clock,float viewport_scale)const{
 std::vector<std::pair<float,float>> out;float x=0;
 for(const auto&item:labels()){
  auto metrics=ui_menu_label(ui_localize(item,language),viewport_scale);
  out.push_back({x,metrics.slot_width});x+=metrics.slot_width+28;
 }
 if(out.empty())return out;
 const auto& selected=out[size_t(std::clamp(row,0,int(out.size()-1)))];
 const float target=selected.first+selected.second*.5f-320.f;
 double now=animation_time>=0?animation_time:clock>=0?clock:menu_scroll.time>=0?menu_scroll.time:ui_monotonic_seconds();
 float scroll=menu_scroll.update(target,now,int(page));
 for(auto&item:out)item.first-=scroll;
 return out;
}

std::vector<std::string> Frontend::car_upgrades(int number)const{auto key=std::to_string(number);try{if(owned.contains(key))return owned[key].get<std::vector<std::string>>();}catch(const std::exception&){}return {};}
std::vector<size_t> Frontend::category_upgrades()const{const char*names[]={"engine","drivetrain","exhaust","suspension","tires","body"};std::vector<size_t> out;for(size_t i=0;i<upgrades.size();++i)if(upgrades[i].category==names[std::clamp(tuning_category,0,5)])out.push_back(i);return out;}
bool Frontend::upgrade_available(size_t i)const{if(i>=upgrades.size()||cars.empty()||(career&&career_progress.car!=cars[car_index].number))return false;auto owned=car_upgrades(cars[car_index].number);const auto&u=upgrades[i];return !u.racing&&(u.required.empty()||std::find(owned.begin(),owned.end(),u.required)!=owned.end())&&std::find(owned.begin(),owned.end(),u.id)==owned.end();}
void Frontend::buy_upgrade(){auto ids=category_upgrades();if(ids.empty()||cars.empty())return;auto i=ids[upgrade_index%ids.size()];auto&u=upgrades[i];int price=u.prices[std::clamp(cars[car_index].car_class-1,0,2)];if(!upgrade_available(i)){message="Upgrade unavailable or already installed";return;}if(money<price){message="Not enough money";return;}money-=price;if(career)career_progress.car_price+=price;auto list=car_upgrades(cars[car_index].number);list.push_back(u.id);owned[std::to_string(cars[car_index].number)]=list;save_profile();message="Upgrade installed";}
namespace {
double numeric_setting(const Json& j,const char* key,double fallback=0){
 auto it=j.find(key);return it!=j.end()&&it->is_number()?it->get<double>():fallback;
}
double rated_hp(const Json& stock,const Json& tuned,double authored_hp){
 // Original ce.Horsepower drives the reference UI; scale it by the actual
 // sampled torque-curve power ratio after Settings/MaxSettings interpolation.
 const double p0=numeric_setting(stock,"peak_power_kw"),p1=numeric_setting(tuned,"peak_power_kw");
 if(p0>0&&p1>=0&&authored_hp>0)return authored_hp*p1/p0;
 if(p1>0)return p1*1.34102209; // SAE mechanical hp per kW.
 return authored_hp;
}
std::string whole(double value){return std::to_string(int(std::lround(value)));}
}
Json Frontend::tuning_profile(bool preview_selected)const{
 if(!assets||cars.empty())return Json::object();
 const auto& car=cars[car_index];if(car.car_class==alpha_car_class)return Json::object();
 auto parts=car_upgrades(car.number);
 if(preview_selected){auto ids=category_upgrades();if(page==MenuPage::Upgrade&&!ids.empty()){
  const auto& item=upgrades[ids[upgrade_index%ids.size()]];
  if(std::find(parts.begin(),parts.end(),item.id)==parts.end())parts.push_back(item.id);
 }}
 std::sort(parts.begin(),parts.end());parts.erase(std::unique(parts.begin(),parts.end()),parts.end());
 std::string key=std::to_string(car.number);for(const auto& part:parts){key+='|';key+=part;}
 if(auto it=tuning_profile_cache.find(key);it!=tuning_profile_cache.end())return it->second;
 try{auto profile=car_configuration(*assets,"cars/car_"+std::to_string(car.number),parts);
  if(tuning_profile_cache.size()>128)tuning_profile_cache.clear();
  tuning_profile_cache.emplace(key,profile);return profile;
 }catch(const std::exception&){return Json::object();}
}
Json Frontend::stock_tuning_profile()const{
 if(!assets||cars.empty()||cars[car_index].car_class==alpha_car_class)return Json::object();
 const int id=cars[car_index].number;
 const std::string key="stock:"+std::to_string(id);
 if(auto it=tuning_profile_cache.find(key);it!=tuning_profile_cache.end())return it->second;
 try{auto value=car_configuration(*assets,"cars/car_"+std::to_string(id));
  if(tuning_profile_cache.size()>128)tuning_profile_cache.clear();
  tuning_profile_cache.emplace(key,value);return value;
 }catch(const std::exception&){return Json::object();}
}
double Frontend::tuned_horsepower(bool preview_selected)const{
 if(cars.empty())return 0;
 const auto& car=cars[car_index];
 if(car.car_class==alpha_car_class)return car.stats.horsepower;
 return rated_hp(stock_tuning_profile(),tuning_profile(preview_selected),car.stats.horsepower);
}
std::vector<std::string> Frontend::upgrade_effect_lines(size_t index)const{
 if(cars.empty()||index>=upgrades.size())return {};
 const auto& car=cars[car_index];
 if(car.car_class==alpha_car_class)return {"Prototype: native tuning unavailable"};
 auto base=tuning_profile();
 const double old_hp=tuned_horsepower();
 // Build a preview from exactly the part selected, not a generic percentage.
 auto parts=car_upgrades(car.number);
 const auto& item=upgrades[index];
 if(std::find(parts.begin(),parts.end(),item.id)==parts.end())parts.push_back(item.id);
 std::sort(parts.begin(),parts.end());parts.erase(std::unique(parts.begin(),parts.end()),parts.end());
 std::string key=std::to_string(car.number);for(const auto& part:parts){key+='|';key+=part;}
 Json preview;
 if(auto it=tuning_profile_cache.find(key);it!=tuning_profile_cache.end())preview=it->second;
 else try{preview=car_configuration(*assets,"cars/car_"+std::to_string(car.number),parts);
  if(tuning_profile_cache.size()>128)tuning_profile_cache.clear();
  tuning_profile_cache.emplace(key,preview);
 }catch(const std::exception&){return {"Native upgrade data unavailable"};}
 const double new_hp=rated_hp(stock_tuning_profile(),preview,car.stats.horsepower);
 std::vector<std::string> result;
 result.push_back("HP: "+whole(old_hp)+" > "+whole(new_hp));
 const std::pair<const char*,const char*> fields[]={
  {"ce.PeakTorque","TORQUE Nm"},{"cd.Mass","WEIGHT kg"},{"cg.EndRatio","FINAL DRIVE"},
  {"cb.BrakeTorque","BRAKE Nm"},{"ce.NitroAcceleration","NITRO"},
  {"cs.FrontRollBarStiffness","FRONT ROLLBAR"},{"cs.RearRollBarStiffness","REAR ROLLBAR"},
  {"ct.LocalDynamicsOptimalSlipRatio","TIRE SLIP"},{"cb.DownForceConst","DOWNFORCE"}};
 for(const auto& [key,label]:fields){
  const double before=numeric_setting(base,key),after=numeric_setting(preview,key,before);
  if(std::abs(after-before)<1e-5)continue;
  const bool small_change=std::abs(before)<10||std::abs(after-before)<.2;
  std::ostringstream line;line.setf(std::ios::fixed);line.precision(small_change?2:0);
  line<<label<<": "<<before<<" > "<<after;
  result.push_back(line.str());
 }
 // Every additional numeric leaf changed by the actual native interpolation
 // is visible on the detail pages, even if it was not in our curated HUD list.
 // This includes engine friction, torque samples, gearbox clutch parameters,
 // suspension spring/damping and individual tire coefficients.
 if(base.contains("source_settings")&&preview.contains("source_settings")){
  std::function<void(const Json&,const Json&,const std::string&)> compare;
  compare=[&](const Json& before,const Json& after,const std::string& path){
   if(before.is_object()&&after.is_object()){
    for(auto it=after.begin();it!=after.end();++it)
     if(before.contains(it.key()))compare(before.at(it.key()),it.value(),path.empty()?it.key():path+"."+it.key());
   }else if(before.is_number()&&after.is_number()){
    const double a=before.get<double>(),b=after.get<double>();
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)<1e-5)return;
    std::string name=path;
    const std::pair<const char*,const char*> aliases[]={
      {"suspension.","SUSP."},{"gearbox.","GEAR."},{"engine.","ENG."},
      {"tires.","TIRE."},{"body.","BODY."}};
    for(const auto& alias:aliases)if(name.rfind(alias.first,0)==0){name=std::string(alias.second)+name.substr(std::strlen(alias.first));break;}
    std::ostringstream line;line.setf(std::ios::fixed);
    line.precision(std::abs(a)<10||std::abs(b)<10?2:0);
    line<<name<<": "<<a<<" > "<<b;
    result.push_back(line.str());
   }
  };
  compare(base.at("source_settings"),preview.at("source_settings"),"");
 }
 if(result.size()==1&&std::abs(old_hp-new_hp)<.01)result.push_back("No numeric change in parsed native stats");
 return result;
}
std::vector<std::string> Frontend::car_stats_lines()const{
 std::vector<std::string> result;
 for(int cls=1;cls<=4;cls++){
  std::set<int> authored,completed;
  for(const auto& e:career_rules.events)if(e.car_class==cls){authored.insert(e.level);if(career_progress.position(e.level)>0)completed.insert(e.level);}
  result.push_back("CLASS "+std::string(cls==1?"C":cls==2?"B":cls==3?"A":"BONUS")+": "+std::to_string(completed.size())+" / "+std::to_string(authored.size()));
 }
 if(cars.empty())return result;
 const auto& car=cars[car_index];
 result.push_back("CAR: "+car.name);
 const auto config=tuning_profile();
 std::string engine_type="NOT DECLARED";
 if(config.contains("source_settings")&&config["source_settings"].is_object()){
  auto settings=config["source_settings"].value("engine",Json::object());
  for(auto key:{"EngineType","Type","Configuration","CylinderLayout","Name"})if(settings.contains(key)&&settings[key].is_string()){
   engine_type=settings[key].get<std::string>();break;
  }
  if(engine_type=="NOT DECLARED")for(auto key:{"NumCylinders","Cylinders"})if(settings.contains(key)&&settings[key].is_number_integer()){
   engine_type=std::to_string(settings[key].get<int>())+" CYLINDER";break;
  }
 }
 result.push_back("ENGINE: "+engine_type);
 result.push_back("POWER: "+whole(tuned_horsepower())+" HP");
 result.push_back("TORQUE: "+whole(numeric_setting(config,"ce.PeakTorque",car.stats.torque))+" Nm");
 result.push_back("WEIGHT: "+whole(numeric_setting(config,"cd.Mass",car.stats.weight))+" kg");
 result.push_back("REDLINE: "+whole(numeric_setting(config,"ce.RedLineRpm"))+" RPM");
 result.push_back("DRIVETRAIN: "+std::string(numeric_setting(config,"cb.FrontTraction")>.5?"FWD":numeric_setting(config,"cb.FrontTraction")<.5?"RWD":"AWD/UNKNOWN"));
 return result;
}
void Frontend::save_profile(){profile_save_ok=false;if(profile_path.empty()){profile_save_ok=true;return;}try{save_settings();Json j=Json::object();{if(!profile_id.empty())j=ProfileStore(profile_root/"players").load(profile_id);else{std::ifstream existing(profile_path);if(existing){try{existing>>j;}catch(const std::exception&){message="Cannot read profile.json; save cancelled";return;}}}}if(!j.is_object())return;Json fields=Json{{"version",1},{"language",ui_language_code(language)},{"car",cars.empty()?3:cars[car_index].number},{"skin",choice.skin},{"money",money},{"upgrades",owned},{"imperial",imperial},{"map",map_visible},{"arrow",arrow_visible},{"bloom",bloom},{"bloom_intensity",bloom_intensity},{"ambient_occlusion",ambient_occlusion},{"ssao",ambient_occlusion},{"water_ssr",water_ssr},{"pbr",pbr},{"lod",lod_enabled},{"auto_performance",auto_performance},{"detail_level",detail_level},{"music_volume",music_volume},{"race_music_volume",race_music_volume},{"effects_volume",effects_volume}};for(auto it=fields.begin();it!=fields.end();++it)j[it.key()]=it.value();j["version"]=3;j["career"]=career_progress.json();j["automatic"]=choice.automatic;j["driver_gender"]=choice.driver_gender;j["handling_difficulty"]=choice.handling_difficulty;if(!profile_id.empty()){ProfileStore(profile_root/"players").save(profile_id,j);}else write_file_atomic(profile_path,j.dump(2));profile_save_ok=true;
 }catch(const std::exception&){message="Cannot save profile.json";}}
void Frontend::load_profile(){if(cars.empty())return;try{std::ifstream file(profile_path);Json j;if(!profile_id.empty())j=ProfileStore(profile_root/"players").load(profile_id);else file>>j;language=ui_language_from_code(j.value("language",std::string(ui_language_code(language))));choice.automatic=j.value("automatic",true);choice.driver_gender=int(driver_model_from_id(j.value("driver_gender",0)));choice.handling_difficulty=std::clamp(j.value("handling_difficulty",1),1,2);if(j.contains("career"))career_progress.load(j.at("career"));money=std::clamp(j.value("money",700),0,100000000);owned=j.value("upgrades",Json::object());if(!owned.is_object())owned=Json::object();imperial=j.value("imperial",true);map_visible=j.value("map",true);arrow_visible=j.value("arrow",true);pbr=false;bloom_intensity=j.contains("bloom_intensity")&&j["bloom_intensity"].is_number()?j["bloom_intensity"].get<float>():1.f;bloom_intensity=std::isfinite(bloom_intensity)?std::clamp(bloom_intensity,0.f,2.f):1.f;bloom=bloom_intensity>0;ambient_occlusion=j.value("ssao",true);water_ssr=j.value("water_ssr",true);lod_enabled=j.value("lod",true);auto_performance=j.value("auto_performance",true);detail_level=std::clamp(j.value("detail_level",2),0,3);music_volume=std::clamp(j.value("music_volume",80),0,100);race_music_volume=std::clamp(j.value("race_music_volume",80),0,100);effects_volume=std::clamp(j.value("effects_volume",100),0,100);int n=j.value("car",3);for(size_t i=0;i<cars.size();++i)if(cars[i].number==n){select_car(i);break;}int skin=j.value("skin",1);if(std::any_of(cars[car_index].skins.begin(),cars[car_index].skins.end(),[&](auto&s){return s.number==skin;}))choice.skin=skin;}catch(const std::exception&){} }

void Frontend::restore_career_car(){for(size_t i=0;i<cars.size();++i)if(cars[i].number==career_progress.car){select_car(i);choice.skin=career_progress.skin;return;}}
void Frontend::enter_career(){choice.bonus_free_roam=false;career=true;if(car_index>=car_selection_count())select_car(0);choice.career_race=true;choice.time_trial=false;message.clear();if(!career_progress.initialized){career_progress.initialize();money=4000;owned=Json::object();save_profile();}restore_career_car();page=career_progress.car?MenuPage::Garage:MenuPage::CarSelect;row=0;}
bool Frontend::career_car_unlocked(int car_class)const{
 if(car_class<1||car_class>3)return false;
 for(int previous=1;previous<car_class;++previous){
  bool has_finals=false;
  for(const auto&event:career_rules.events){
   if(event.car_class!=previous||event.stage!=3)continue;
   has_finals=true;
   const int place=career_progress.position(event.level);
   if(place<1||place>3)return false;
  }
  if(!has_finals)return false;
 }
 return true;
}
bool Frontend::buy_career_car(){if(cars.empty())return false;const auto&c=cars[car_index];if(!career_car_unlocked(c.car_class)&&c.number!=career_progress.car){message="This car class is locked";return false;}if(c.number==career_progress.car){career_progress.skin=choice.skin;save_profile();return true;}int price=c.stats.price,trade=career_progress.car?career_progress.car_price/2:0;if(price<=0){message="Car is not available for purchase";return false;}if(money+trade<price){message="Not enough money: $"+std::to_string(price-trade)+" after trade";return false;}if(career_progress.car)owned.erase(std::to_string(career_progress.car));money+=trade-price;career_progress.car=c.number;career_progress.car_price=price;career_progress.skin=choice.skin;save_profile();message="Car purchased";return true;}
bool Frontend::begin_career_race(){if(!choice.career_race)return true;if(!career_progress.car||!career_event_unlocked(choice.level)){message="Career event is locked or no car purchased";return false;}if(career_progress.pending&&career_progress.pending_level==choice.level)return true;last_career_result=Json::object();career_progress.pending=true;career_progress.pending_level=choice.level;++career_progress.attempt;save_profile();if(!profile_save_ok){career_progress.pending=false;message="Cannot save career; choose a writable game folder";return false;}return true;}
void Frontend::cancel_career_race(){if(!career_progress.pending)return;career_progress.pending=false;save_profile();}
void Frontend::complete_career_race(const Json&standings,int prop_hits,int prop_bonus,const std::array<int,10>&hit_types,const std::array<int,10>&cents_types){if(!choice.career_race||!career_progress.pending)return;const Json*player=nullptr;if(standings.is_array()){for(auto&v:standings)if(v.is_object()&&v.value("car",-1)==0){player=&v;break;}}else if(standings.is_object())player=&standings;if(!player)return;try{auto before_progress=career_progress;auto before_result=last_career_result;int before_money=money;int place=player->value("rank",player->value("place",0));double seconds=player->value("time",player->value("total_time",0.));bool finished=player->value("finished",false),dnf=player->value("dnf",false);if(!finished&&!dnf)return;int old_class=career_progress.highest_class;std::vector<int> before;for(const auto&e:career_rules.events)if(career_event_unlocked(e.level))before.push_back(e.level);int prize=career_progress.settle(career_rules,place,seconds,finished,dnf);
 const bool paid=finished&&!dnf&&place>=1&&place<=8&&std::isfinite(seconds)&&seconds>=0;
 prop_bonus=paid?std::clamp(prop_bonus,0,100000000-prize):0;int award=prize+prop_bonus;Json unlocked=Json::array();for(const auto&e:career_rules.events)if(career_event_unlocked(e.level)&&std::find(before.begin(),before.end(),e.level)==before.end())unlocked.push_back(e.level);last_career_result={{"level",choice.level},{"place",place},{"time",seconds},{"dnf",dnf},{"award",award},{"race_prize",prize},{"prop_hits",std::max(0,prop_hits)},{"prop_bonus",prop_bonus},{"prop_hits_by_type",hit_types},{"prop_cents_by_type",paid?cents_types:std::array<int,10>{}},{"medal",!dnf&&place>=1&&place<=3?(place==1?"GOLD":place==2?"SILVER":"BRONZE"):""},{"unlocked_levels",unlocked},{"class_unlocked",career_progress.highest_class>old_class?career_progress.highest_class:0},{"campaign_complete",career_progress.classes_paid[2]}};money=std::min(100000000,money+award);message=dnf?"DNF - no award":"Career result: "+std::to_string(place)+"  Award $"+std::to_string(award);save_profile();if(!profile_save_ok){career_progress=std::move(before_progress);last_career_result=std::move(before_result);money=before_money;message="Result not saved: check folder access; it will retry";}}catch(const std::exception&){} }

void Frontend::complete_career_stunt(int medal,int award,double score){
 if(!choice.career_race||!stunt_mode()||!career_progress.pending||career_progress.pending_level!=choice.level)return;
 const auto before=career_progress;
 const int balance=money;
 career_progress.pending=false;
 if(medal>=1&&medal<=3){
  const int previous=career_progress.position(choice.level);
  if(!previous||medal<previous)career_progress.positions[choice.level]=medal;
 }
 money=std::clamp(money,0,100000000);
 award=std::clamp(award,0,100000000-money);
 money+=award;
 last_career_result={{"level",choice.level},{"place",medal},{"stunt_score",score},{"award",award},{"race_prize",award}};
 save_profile();
 if(!profile_save_ok){
  career_progress=before;
  money=balance;
  last_career_result=Json::object();
 }
}

}
