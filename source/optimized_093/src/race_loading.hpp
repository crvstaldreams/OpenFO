#pragma once
#include "frontend.hpp"

namespace ofo {
struct LoadingUV {float u=0,v=0,w=1,h=1;};
inline LoadingUV loading_cover_uv(int width,int height,int image_width,int image_height){
 if(width<=0||height<=0||image_width<=0||image_height<=0)return {};
 const float screen=float(width)/height,image=float(image_width)/image_height;
 if(image>screen){
  const float visible=screen/image;
  return {(1-visible)*.5f,0,visible,1};
 }
 const float visible=image/screen;
 return {0,(1-visible)*.5f,1,visible};
}
inline float loading_progress(unsigned percent){return std::min(percent,100u)*.01f;}
inline bool loading_race_has_laps(RaceMode mode){return mode==RaceMode::Circuit;}

struct RaceLoadingPresentation {
 std::string name,preview;
 MenuArtwork logo;
 int laps=0;
 unsigned progress=0;
};

inline RaceLoadingPresentation race_loading_presentation(const Frontend& menu,const std::string& stage){
 RaceLoadingPresentation load;
 if(loading_race_has_laps(menu.mode()))
  load.laps=std::clamp(menu.choice.laps,1,9);
 const auto stage_id=clean_path(stage);
 auto matches=[&](const MenuTrack& track){return !track.stage.empty()&&clean_path(track.stage)==stage_id&&(!menu.stunt_mode()||track.level==menu.choice.level);};
 const MenuTrack* selected=nullptr;
 if(menu.track_slot<menu.track_slots.size()&&matches(menu.track_slots[menu.track_slot]))
  selected=&menu.track_slots[menu.track_slot];
 if(!selected){
  for(const auto* catalog:{&menu.track_slots,&menu.alpha_tracks}){
   auto track=std::find_if(catalog->begin(),catalog->end(),matches);
   if(track!=catalog->end()){
    selected=&*track;
    break;
   }
  }
 }
 if(selected){
  load.name=selected->name;
  load.preview=selected->preview;
  if(menu.assets&&!selected->name_icon.empty())
   load.logo=menu_artwork(*menu.assets,"track_name_icons",selected->name_icon);
 }
 if(!menu.choice.track_project.empty()){
  load.name=menu.choice.track_project.stem().u8string();
  load.logo={};
 }
 if(load.name.empty()){
  if(auto it=menu.names.find(menu.choice.track);it!=menu.names.end())load.name=it->second;
  if(load.name.empty())load.name=stage;
 }
 if(load.preview.empty())
  if(auto it=menu.previews.find(menu.choice.track);it!=menu.previews.end())load.preview=it->second;
 return load;
}
}
