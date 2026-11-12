#include "frontend.hpp"
#include "content.hpp"

namespace ofo {
void Frontend::refresh_track_projects(){
 if(!assets)return;
 auto stages=stage_catalog(*assets);
 for(const auto& alpha:alpha_tracks)stages.push_back(alpha.stage);
 if(assets->exists("tracks/menu/menu1/geometry/track_geom.w32"))
  stages.push_back("tracks/menu/menu1/a");
 track_projects=list_track_projects(project_directory,stages);
}

void Frontend::select_track_project(const TrackProject& project){
 career=false;
 choice.career_race=false;
 choice.track_project=project.path;
 choice.stage=project.stage;
 choice.track=stage_geometry(project.stage);
 choice.time_trial=project.circuit;
 choice.bonus_free_roam=!project.circuit;
 choice.event=project.circuit?"RACE":"FREEROAM";
 choice.subevent.clear();
 choice.level=0;
 choice.laps=project.circuit?3:1;
 choice.bots=0;
 choice.ai_profile_class=choice.ai_handicap_level=choice.ai_upgrade_level=1;
 apply_track_settings();
 message.clear();
}
}
