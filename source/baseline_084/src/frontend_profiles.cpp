#include "frontend.hpp"
#include "file_io.hpp"
#include <algorithm>
#include <fstream>
namespace ofo {
void Frontend::refresh_profiles(){player_profiles.clear();if(profile_root.empty())return;try{player_profiles=ProfileStore(profile_root/"players").list();}catch(const std::exception&e){message=e.what();}}
void Frontend::init_profiles(const fs::path&root){profile_root=root;fs::create_directories(root);profile_path=root/"profile.json";controls_path=root/"controls.json";settings_path=root/"settings.json";track_settings_path=root/"track_settings.json";ProfileStore store(root/"players");refresh_profiles();
 // Import the previous single-player save once, leaving its source untouched.
 if(player_profiles.empty()&&!fs::exists(store.root()/"index.json")&&fs::exists(profile_path)){try{auto b=read_file(profile_path,4*1024*1024);auto j=Json::parse(b.begin(),b.end());if(j.is_object()){auto player=store.create("PLAYER",j);store.activate(player.id);fs::path old_controls=root/"controls.json";if(fs::exists(old_controls))fs::copy_file(old_controls,store.folder(player.id)/"controls.json",fs::copy_options::overwrite_existing);if(fs::exists(settings_path))fs::copy_file(settings_path,store.folder(player.id)/"settings.json",fs::copy_options::overwrite_existing);refresh_profiles();}}catch(const std::exception&e){message=std::string("Legacy save preserved: ")+e.what();}}
 auto active=store.active();if(active.empty()&&!player_profiles.empty())active=player_profiles.front().id;if(!active.empty())activate_profile(active);else if(fs::exists(store.root()/"index.json")){
  profile_path.clear();controls_path.clear();settings_path.clear();track_settings_path.clear();
 }else{controls.load(controls_path);load_profile();load_settings();load_track_settings();}}
bool Frontend::activate_profile(const std::string&id){if(profile_root.empty())return false;try{ProfileStore store(profile_root/"players");auto data=store.load(id);auto name=data.at("profile").at("name").get<std::string>();store.activate(id);profile_id=id;profile_name=name;auto folder=store.folder(id);profile_path=folder/"savegame.json";controls_path=folder/"controls.json";settings_path=folder/"settings.json";track_settings_path=folder/"track_settings.json";
 language=UiLanguage::Ukrainian;career=false;choice.career_race=false;choice.bonus_free_roam=false;choice.time_trial=false;career_progress=CareerProgress{};last_career_result=Json::object();owned=Json::object();money=700;imperial=true;map_visible=arrow_visible=lod_enabled=true;detail_level=2;bloom_intensity=1.f;bloom=true;ambient_occlusion=water_ssr=true;music_volume=race_music_volume=80;effects_volume=100;screen_width=1280;screen_height=720;fullscreen=false;choice.automatic=true;choice.ai_handicap_level=1;controls=ControlSettings{};if(!cars.empty())select_car(0);load_profile();load_settings();load_track_settings();controls.load(controls_path);refresh_profiles();message="Profile: "+profile_name;return true;
 }catch(const std::exception&e){message=std::string("Cannot load profile: ")+e.what();return false;}}
bool Frontend::create_profile(std::string name){if(profile_root.empty())return false;try{ProfileStore store(profile_root/"players");auto p=store.create(std::move(name),{{"money",700},{"upgrades",Json::object()},{"career",CareerProgress{}.json()},{"automatic",true},{"driver_gender",profile_draft_gender},{"handling_difficulty",profile_draft_difficulty}});if(!activate_profile(p.id))return false;save_profile();return true;}catch(const std::exception&e){message=e.what();return false;}}
void Frontend::confirm_profile_delete(){
 if(profile_delete_id.empty())return;
 const auto id=std::exchange(profile_delete_id,{});
 if(!std::exchange(profile_delete_yes,false))return;
 try{
  auto next=ProfileStore(profile_root/"players").remove(id);
  if(id==profile_id){
   profile_id.clear();profile_name="PLAYER";
   profile_path.clear();settings_path.clear();controls_path.clear();track_settings_path.clear();
   career_progress={};owned=Json::object();money=700;career=false;
   if(!next.empty()&&!activate_profile(next)){refresh_profiles();return;}
  }
  refresh_profiles();
  row=std::clamp(row,0,int(player_profiles.size()));
  message="Profile deleted";
 }catch(const std::exception&e){message=e.what();}
}
void Frontend::profile_character(uint32_t c){if(page!=MenuPage::Profile||!profile_editing)return;if(c==8){if(!profile_draft.empty()){size_t i=profile_draft.size()-1;while(i&&(static_cast<unsigned char>(profile_draft[i])&0xc0)==0x80)--i;profile_draft.erase(i);}return;}if(c<32||c==127||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return;size_t count=0;for(unsigned char b:profile_draft)if((b&0xc0)!=0x80)++count;if(count>=32)return;if(c<128)profile_draft+=char(c);else if(c<2048){profile_draft+=char(0xc0|(c>>6));profile_draft+=char(0x80|(c&63));}else if(c<65536){profile_draft+=char(0xe0|(c>>12));profile_draft+=char(0x80|((c>>6)&63));profile_draft+=char(0x80|(c&63));}else{profile_draft+=char(0xf0|(c>>18));profile_draft+=char(0x80|((c>>12)&63));profile_draft+=char(0x80|((c>>6)&63));profile_draft+=char(0x80|(c&63));}}
void Frontend::save_settings(){bloom_intensity=std::isfinite(bloom_intensity)?std::clamp(bloom_intensity,0.f,2.f):1.f;bloom=bloom_intensity>0;if(settings_path.empty())return;Json j={{"version",1},{"language",ui_language_code(language)},{"imperial",imperial},{"map",map_visible},{"arrow",arrow_visible},{"lod",lod_enabled},{"detail_level",detail_level},{"bloom_intensity",bloom_intensity},{"ssao",ambient_occlusion},{"water_ssr",water_ssr},{"music_volume",music_volume},{"race_music_volume",race_music_volume},{"effects_volume",effects_volume},{"screen_width",screen_width},{"screen_height",screen_height},{"fullscreen",fullscreen},{"automatic",choice.automatic}};write_file_atomic(settings_path,j.dump(2));}
void Frontend::load_settings(){if(settings_path.empty()||!fs::exists(settings_path))return;try{auto b=read_file(settings_path,1024*1024);auto j=Json::parse(b.begin(),b.end());language=ui_language_from_code(j.value("language",std::string(ui_language_code(language))));imperial=j.value("imperial",imperial);map_visible=j.value("map",map_visible);arrow_visible=j.value("arrow",arrow_visible);lod_enabled=j.value("lod",lod_enabled);ambient_occlusion=j.value("ssao",true);water_ssr=j.value("water_ssr",true);bloom_intensity=j.contains("bloom_intensity")&&j["bloom_intensity"].is_number()?j["bloom_intensity"].get<float>():1.f;bloom_intensity=std::isfinite(bloom_intensity)?std::clamp(bloom_intensity,0.f,2.f):1.f;bloom=bloom_intensity>0;detail_level=std::clamp(j.value("detail_level",detail_level),0,3);music_volume=std::clamp(j.value("music_volume",music_volume),0,100);race_music_volume=std::clamp(j.value("race_music_volume",race_music_volume),0,100);effects_volume=std::clamp(j.value("effects_volume",effects_volume),0,100);screen_width=std::clamp(j.value("screen_width",1280),800,3840);screen_height=std::clamp(j.value("screen_height",720),600,2160);fullscreen=j.value("fullscreen",false);choice.automatic=j.value("automatic",choice.automatic);}catch(const std::exception&e){message=std::string("Settings ignored: ")+e.what();}}
void Frontend::load_track_settings(){track_settings=Json::object();choice.environment=TrackEnvironment{};
 if(!track_settings_path.empty()&&fs::exists(track_settings_path))try{auto b=read_file(track_settings_path,1024*1024);auto j=Json::parse(b.begin(),b.end());if(j.is_object()&&j.contains("stages")&&j["stages"].is_object())track_settings=j["stages"];}catch(const std::exception&){message="Track settings ignored: invalid file";}
 apply_track_settings();
}
void Frontend::save_track_settings(){if(track_settings_path.empty())return;try{write_file_atomic(track_settings_path,Json{{"version",1},{"stages",track_settings}}.dump(2));}catch(const std::exception&){message="Cannot save track settings";}}
void Frontend::apply_track_settings(){choice.environment=TrackEnvironment{};auto key=selected_stage();if(!track_settings.is_object()||!track_settings.contains(key))return;auto s=TrackRaceSettings::from_json(track_settings[key]);choice.environment=s.environment;if(s.ai_override)choice.ai_handicap_level=s.ai_handicap_level;}
void Frontend::remember_track_settings(bool ai_changed){auto key=selected_stage();TrackRaceSettings s;if(track_settings.contains(key))s=TrackRaceSettings::from_json(track_settings[key]);s.environment=choice.environment;s.ai_handicap_level=choice.ai_handicap_level;s.ai_override=s.ai_override||ai_changed;track_settings[key]=s.json();save_track_settings();}

}
