#include "simulation.hpp"
#include "course_ui.hpp"
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace ofo;
int main(int argc,char**argv){try{if(argc<2)throw std::runtime_error("original data argument required");Resources r(argv[1]);const std::string stage="tracks/forest/forest1/a";auto native_course=race_course(r,stage);auto route=race_ai_route(native_course);auto roster=load_race_opponents(r,1,1,1,7,stage);auto geometry=load_geometry(r,stage_geometry(stage),stage);auto cfg=load_vehicle(r,3,argc>2?argv[2]:"");Simulation sim(geometry,cfg,load_spawn(r,stage));for(int i=0;i<200;i++)sim.step({});sim.set_opponents(roster,route);CourseDirector course(native_course,8,1,7);std::array<uint64_t,7>generations{};bool finished=false;int steps=0;auto start=std::chrono::steady_clock::now();
 for(;steps<60000&&!finished;++steps){sim.step({});auto state=sim.state();std::vector<CoursePose>poses(8);poses[0].chassis=state.car;poses[0].speed_kmh=state.speed*3.6;for(size_t i=0;i<7;i++){const auto&o=state.opponents->at(i);poses[i+1].chassis=o.car;poses[i+1].speed_kmh=o.speed*3.6;if(generations[i]!=o.recovery_generation){if(generations[i])course.relocated(i+1,poses[i+1]);generations[i]=o.recovery_generation;}}course.update(.01,poses);for(size_t i=0;i<7;i++)if(course.finished(i+1)){sim.set_opponent_finished(i);finished=true;}if(steps%6000==5999){auto table=course.leaderboard(poses);std::cout<<"simulation_seconds="<<(steps+1)*.01<<" progress="<<table.dump()<<std::endl;}}
 auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<"simulation_seconds="<<steps*.01<<" wall_seconds="<<elapsed<<" hud="<<course.hud().dump()<<std::endl;if(!finished)throw std::runtime_error("No opponent completed one lap through all ordered native gates in 600 simulated seconds");std::cout<<"native shared seven-AI ordered-gate lap finish passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
