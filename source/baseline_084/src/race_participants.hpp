#pragma once
#include "simulation.hpp"
#include "course_ui.hpp"
#include "frontend_hud.hpp"
namespace ofo {
inline std::vector<CoursePose> participant_poses(const State&s){
 std::vector<CoursePose> out;CoursePose p;p.chassis=s.car;p.speed_kmh=s.speed*3.6;p.driver_ejected=s.driver_ejected;out.push_back(p);
 if(s.opponents)for(const auto&b:*s.opponents){p.chassis=b.car;p.speed_kmh=b.speed*3.6;p.driver_ejected=b.driver_ejected;out.push_back(p);}return out;
}
inline Json name_standings(Json rows,const State&s){
 for(auto&row:rows){const int index=row.value("car",0);row["player"]=index==0;row["name"]="PLAYER";
  if(index>0&&s.opponents&&size_t(index-1)<s.opponents->size()){const auto&b=(*s.opponents)[size_t(index-1)];row["name"]=b.name;row["car_id"]=b.car_id;row["skin"]=b.skin;}
 }return rows;
}
inline std::vector<RaceOverlayEntry> race_overlay_rows(const Json&rows){std::vector<RaceOverlayEntry> out;if(!rows.is_array())return out;
 for(const auto&j:rows){RaceOverlayEntry e;e.player=j.value("player",j.value("car",0)==0);e.rank=j.value("rank",1);e.name=j.value("name",e.player?"PLAYER":"AI");e.car_name=j.value("car_name",std::string{});e.lap=j.value("lap",1);e.laps=j.value("laps",1);e.time=j.value("time",0.);e.best=j.value("best",0.);e.finished=j.value("finished",false);e.status=j.value("dnf",false)?"DNF":j.value("status","");out.push_back(std::move(e));}return out;
}
inline int race_player_rank(const Json&rows){for(const auto&j:rows)if(j.value("player",j.value("car",0)==0))return j.value("rank",1);return 1;}
}
