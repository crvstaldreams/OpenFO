#pragma once
#include "frontend.hpp"
#include "stunt_event.hpp"
namespace ofo {
// Source: v0.23 race_overlay_gl.hpp. Values must come from gameplay; the UI
// never invents placements, laps, countdowns, scoring or a result table.
struct RaceOverlayEntry {int rank=0;std::string name;int lap=0,laps=0;double time=0;bool finished=false,player=false;std::string status;double best=0;std::string car_name;};
struct MapMarker {double x=0,z=0,yaw=0;bool player=false,eliminated=false;};
struct RaceOverlayState {
 UiLanguage language=UiLanguage::Ukrainian;
 float hud_opacity=1;
 double recovery_countdown=0,countdown=0,elapsed=0,lap_time=0,finish_elapsed=0;
 int position=1,participants=1,lap=1,laps=3;
 double off_route_distance=-1;
 int prop_hits=0,prop_bonus=0;
 bool show_positions=false,results=true;
 bool replay_paused=false;double replay_rate=1,replay_position=0,replay_duration=0;int replay_selected=0;
 bool finished=false,replay=false;
 std::vector<RaceOverlayEntry> leaderboard;
 std::string result_detail,finish_prompt="ENTER / A: replay   ESC: pause menu";
 std::string music_title,music_artist;double music_age=-1;
 StuntDisplay stunt;
 bool event_intro=false;std::string event_name,event_hint;
 bool battle=false;double health=1;size_t survivors=0;
 std::vector<MapMarker>markers;
 bool guidance_available=false;double guidance_yaw=0,guidance_distance=0;
};
class FrontendHud {
 const Resources&r_;mutable std::map<std::string,MenuArtwork>art_;
public:
 explicit FrontendHud(const Resources&r):r_(r){}
 std::vector<FrontendDraw> commands(int width,int height,const RaceOverlayState&)const;
};
}
