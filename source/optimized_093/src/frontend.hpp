#pragma once
#include "race_event.hpp"
#include "track_projects.hpp"
#include "assets.hpp"
#include "driver_model.hpp"
#include "lan_session.hpp"
#include "openfo_bed_settings.hpp"
#include "localization.hpp"
#include "display_resolution_policy.hpp"
#include "profile_store.hpp"
#include "track_environment.hpp"
#include "career.hpp"
#include "legacy_ui/menu_assets.hpp"
#include "legacy_ui/font_assets.hpp"
#include "legacy_ui/control_settings.hpp"

#include <functional>
#include <future>
#include "ui_layout.hpp"
namespace ofo {
class EditorSceneQuery;
struct RaceChoice {TrackEnvironment environment;std::string track;size_t car=0;int laps=3,bots=0,skin=1;bool time_trial=false,automatic=true,career_race=false,bonus_free_roam=false;int driver_gender=0,handling_difficulty=1;int ai_handicap_level=1,ai_upgrade_level=1,ai_profile_class=1;std::string event,subevent;std::string stage;int level=0;fs::path track_project;};
enum class MenuPage { Start,Profile,CareerAward,Garage,Tuning,Upgrade,Stats,GameOptions, Main,SinglePlayer,RaceSetup,ClassSelect,CarSelect,Transmission,Pause,Confirm,Options,Controls,Credits,SmashBonus,AlphaTracks,TrackProjects,LanStart,LanLobby,LanRooms };
// Coordinates in physical pixels; content uses centered 640x480 source canvas.
struct FrontendDraw {
 enum Kind {Rect,Image,Text} kind=Rect;
 float x=0,y=0,w=0,h=0,u=0,v=0,uw=1,vh=1,scale=1;
 float clip_left=-1,clip_right=-1; // optional physical horizontal text clipping
 uint32_t color=0xffffffff;bool heading=false;
 std::string value;
};
struct FrontendScene {
 bool enabled=false,show_car=false,title_smoke=false;
 std::string geometry="tracks/menu/menu1/geometry/track_geom.w32",car_geometry;
 std::string config_base="tracks/menu/menu1/a";
 Mat car_transform=identity(); Vec eye{},target{};float fov=70,fade_alpha=0;int slot=20,skin=1;
};
struct XboxPreview {
 bool connected=false;
 uint16_t buttons=0;
 float left_x=0,left_y=0,right_x=0,right_y=0,left_trigger=0,right_trigger=0,steering=0;
};
class Frontend {
public:
 std::vector<std::string> tracks;std::map<std::string,std::string> names,previews;std::map<std::string,int> default_laps;
 std::vector<MenuTrack> track_slots,alpha_tracks;size_t track_slot=0;
 fs::path project_directory;std::vector<TrackProject> track_projects;
 std::vector<MenuCar> cars;RaceChoice choice;size_t track_index=0,car_index=0;int selected_class=1,row=0;
 MenuPage page=MenuPage::Main;bool pause_context=false,start=false,resume=false,recover=false,restart=false,retire=false,replay=false,photo=false,quit=false;
 const Resources* assets=nullptr;
 mutable Json showroom_points,showroom_cameras,showroom_dummies;
 mutable std::map<std::pair<int,int>,float> showroom_lift;
 mutable std::shared_ptr<EditorSceneQuery> showroom_ground;
 mutable std::map<FontStyle,BitmapFont> fonts;
 mutable std::map<std::string,MenuArtwork> artwork_cache;
 CareerRules career_rules;CareerProgress career_progress;Json last_career_result=Json::object();
 UiLanguage language=UiLanguage::Ukrainian;
 bool career=false,imperial=true,map_visible=true,arrow_visible=true;
 bool lod_enabled=true,auto_performance=true;int detail_level=2;int fps_limit=60;bool vsync=false,mirror_enabled=true;int render_scale_percent=100;unsigned active_cpu_lanes=1;
 float bloom_intensity=1.f;bool bloom=true,ambient_occlusion=true,pbr=false,water_ssr=true;
 mutable double camera_epoch=-1;
 double animation_time=-1; // negative uses the monotonic UI clock; previews may set a fixed time
 int tuning_category=0;size_t upgrade_index=0;int upgrade_detail_scroll=0;int stats_scroll=0,money=700;
 std::vector<MenuUpgrade> upgrades;Json owned=Json::object();
 mutable std::map<std::string,Json> tuning_profile_cache;
 fs::path profile_path,profile_root,settings_path,track_settings_path;
 Json track_settings=Json::object();
 void load_track_settings();void save_track_settings();void apply_track_settings();void remember_track_settings(bool ai_changed=false);
 bool profile_save_ok=true;
 std::string profile_id,profile_name="PLAYER",profile_draft;bool profile_editing=false;int profile_draft_gender=0,profile_draft_difficulty=1;
 std::vector<PlayerProfile> player_profiles;
 std::string profile_delete_id;
 bool profile_delete_yes=false;
 void confirm_profile_delete();
 int screen_width=1280,screen_height=720;bool fullscreen=false;
 std::vector<DisplayResolution> display_resolutions;
 void set_display_resolutions(std::vector<DisplayResolution> modes){display_resolutions=normalize_display_resolutions(std::move(modes));if(!display_resolutions.empty()){const auto selected=closest_display_resolution(display_resolutions,screen_width,screen_height);screen_width=selected.first;screen_height=selected.second;}}
 void init_profiles(const fs::path&);void refresh_profiles();bool create_profile(std::string);bool activate_profile(const std::string&);void profile_character(uint32_t);void save_settings();void load_settings();
 uint64_t navigation_serial=0; // host consumes a changed serial as one native navigation sound
 mutable UiMotion menu_scroll,car_scroll,part_scroll;
 mutable bool camera_blend_valid=false;mutable double camera_blend_time=-1;
 mutable int showroom_car=-1;mutable double showroom_transition=-1;mutable FrontendScene showroom_previous;
 mutable Vec camera_blend_eye{},camera_blend_target{};mutable float camera_blend_fov=70;
 std::vector<std::pair<float,float>> horizontal_positions(double clock=-1,float viewport_scale=1)const;
 std::vector<size_t> category_upgrades()const;bool upgrade_available(size_t)const;
 bool begin_career_race();void cancel_career_race();void complete_career_race(const Json& standings,int prop_hits=0,int prop_bonus=0,
                          const std::array<int,10>& hit_types={},const std::array<int,10>& cents_types={});
 bool career_event_unlocked(int level)const{return career_progress.unlocked(career_rules,level);}
 bool career_car_unlocked(int car_class)const;
 void enter_career();bool buy_career_car();void restore_career_car();
 void save_profile();void load_profile();void buy_upgrade();
 std::vector<std::string> car_upgrades(int number)const;
 Json tuning_profile(bool preview_selected=false)const;
 Json stock_tuning_profile()const;
 double tuned_horsepower(bool preview_selected=false)const;
 std::vector<std::string> upgrade_effect_lines(size_t index)const;
 std::vector<std::string> car_stats_lines()const;
 RaceMode mode()const{
  if(!choice.track_project.empty())return choice.bonus_free_roam?RaceMode::FreeRoam:RaceMode::Circuit;
  return race_mode(choice.stage.empty()?choice.track:choice.stage,choice.event,choice.bonus_free_roam);
 }
 bool stunt_mode()const{return mode()==RaceMode::Stunt;}
 bool supported_stunt()const{return stunt_kind(choice.subevent)!=StuntKind::None;}
 bool bonus_class_visible()const;
 void complete_career_stunt(int medal,int award,double score);
 XboxPreview xbox;
 ControlSettings controls;fs::path controls_path;int binding=-1;
 void save_controls();
 std::string message,controller_hint;
 int music_volume=80,race_music_volume=80,effects_volume=100,credits_index=1;MenuPage options_return=MenuPage::Main;int pending_action=0;
 size_t car_selection_count()const;
 void select_car(size_t);void select_track(size_t);
 void refresh_track_projects();void select_track_project(const TrackProject&);
 void select_alpha_track(size_t);std::vector<size_t> class_tracks()const;void cycle_skin(int);
 explicit Frontend(const Resources&,const fs::path& profile_dir={},const std::string& requested="");
 std::shared_ptr<LanSession> lan;
 bool lan_talking=false;
 std::string lan_address,master_address,room_draft;
 fs::path master_settings;
 OpenFOBedSettings bed_settings;
 std::vector<LanRoomInfo> online_rooms;
 std::future<std::vector<LanRoomInfo>> room_lookup;
 std::future<uint16_t> room_creation;
 void setup_master_server(const fs::path&);
 void poll_room_directory();
 bool lan_page()const{return page==MenuPage::LanStart||page==MenuPage::LanLobby||page==MenuPage::LanRooms;}
 const MenuTrack* lan_track()const;
 std::array<std::string,5> lan_setting_values(const MenuTrack*)const;
 std::vector<std::string> lan_labels()const;
 void lan_key(int);
 void lan_character(uint32_t);
 void lan_leave();
 bool apply_lan_match();
 void key(int);void back();void open_pause();void click(int,int,int,int);
 std::vector<size_t> class_cars()const;std::vector<std::string> labels()const;std::string title()const;
 void click_normalized(float x,float y,int width,int height){click(int(x*width),int(y*height),width,height);}
 void clear_actions(){start=resume=recover=restart=retire=replay=photo=quit=false;}
 int selected_car_id()const{return cars.empty()?3:cars[car_index].number;}
 std::string selected_stage()const;
 std::vector<FrontendDraw> draw_commands(int width,int height,double clock=0)const;
 FrontendScene showroom(double seconds=0)const;
 std::vector<FrontendDraw> replay_commands(int width,int height,double position,double duration,double rate,bool paused,int selected)const;
};
}
