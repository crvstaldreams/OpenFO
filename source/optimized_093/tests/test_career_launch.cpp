#include "frontend.hpp"
#include "race_ai.hpp"
#include "course_ui.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const std::string&m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{if(argc<2)return 1;Resources r(argv[1]);Frontend f(r);f.career=true;f.career_progress.initialize();f.career_progress.highest_class=3;int circuits=0,derbies=0,arenas=0,stunts=0;for(size_t i=0;i<f.track_slots.size();++i){f.select_track(i);auto&c=f.choice;auto stage=f.selected_stage();if(f.stunt_mode()){need(f.supported_stunt()&&c.bots==0,"stunt launch");++stunts;continue;}need(r.exists(stage+"/data/startpoints.bed"),"missing grid "+stage);auto roster=load_race_opponents(r,c.ai_profile_class,c.ai_handicap_level,c.ai_upgrade_level,c.bots,stage);need(roster.size()==size_t(c.bots),"incomplete opponent roster "+stage);for(const auto&b:roster){need(r.exists(b.vehicle.body)&&r.exists(b.vehicle.tire),"missing AI model "+stage);need(r.exists("cars/car_"+std::to_string(b.car_id)+"/skinai"+std::to_string(b.skin)+".dds"),"missing native AI skin "+stage);}if(f.mode()==RaceMode::Battle){++derbies;continue;}if(arena_circuit(stage))++arenas;auto route=race_course(r,stage);need(route.value("available",false),"career circuit has no native route "+stage);CourseDirector director(route,1+roster.size(),c.laps,int(roster.size()));need(director.available(),"unusable checkpoint route "+stage);need(c.bots==7,"career circuit must contain seven original opponents");++circuits;}
 need(circuits==39&&derbies==3&&arenas==3&&stunts==6,"full native career mode catalog");std::cout<<"39 native circuits including 3 arenas, 3 derby grids, 6 stunts and all opponent models/skins resolved\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
