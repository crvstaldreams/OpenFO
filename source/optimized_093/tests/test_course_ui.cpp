#undef NDEBUG
#include "course_ui.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace ofo;
CoursePose pose(double x,double z){CoursePose p;p.chassis={1,0,0,0,0,1,0,0,0,0,1,0,float(x),0,float(z),1};return p;}
int main(int argc,char**argv){
 Json course={{"points",{{0,0,0},{0,0,10},{10,0,10},{10,0,0}}},{"gates",{{{"position",{0,0,5}},{"left",{-5,0,5}},{"right",{5,0,5}}},{{"position",{10,0,5}},{"left",{5,0,5}},{"right",{15,0,5}}}}}};
 CourseDirector r(course,2,1,1);assert(r.hud()[0].contains("lap_time"));r.update(.1,{pose(0,0),pose(0,0)});r.update(.1,{pose(0,7),pose(0,7)});assert(r.hud()[0]["next_gate"]==1);r.reset(0);assert(r.hud()[0]["next_gate"]==0);r.update(.1,{pose(10,7),pose(10,7)});r.update(.1,{pose(10,0),pose(10,0)});assert(!r.hud()[0]["finished"].get<bool>());assert(r.hud()[1]["finished"].get<bool>());assert(!r.finished()&&r.finished(1)&&!r.finished(99));
 assert(std::abs(r.hud()[1]["lap_time"].get<double>()-.4)<1e-9);assert(std::abs(r.hud()[1]["time"].get<double>()-.4)<1e-9);
 auto table=r.leaderboard({pose(0,0),pose(10,0)});assert(table[0]["car"]==1&&table[0]["rank"]==1);
 auto inputs=r.controls({pose(0,0),pose(0,0)});assert(inputs.size()==2&&std::isfinite(inputs[1].steer));
 // Nearest route projection must use segments and retain checkpoint/timing state.
 auto off=pose(3,6);auto recovery=r.recovery_transform(off);assert(recovery);
 assert(std::abs((*recovery)[12])<1e-6&&std::abs((*recovery)[14]-6)<1e-6);
 assert(std::abs((*recovery)[8])<1e-6&&std::abs((*recovery)[10]-1)<1e-6);
 auto before=r.hud()[0];auto relocated=pose(0,6);r.relocated(0,relocated);assert(r.hud()[0]==before);
 r.update(.1,{pose(0,6),pose(10,0)});assert(r.hud()[0]["next_gate"]==before["next_gate"]);
 CourseDirector missing(Json::object(),1,1,0);assert(!missing.recovery_transform(off));
 assert(!missing.guidance(0,off).available);assert(!r.guidance(9,off).available);
 Json navigation={{"points",{{0,0,0},{0,0,200},{200,0,200},{200,0,0}}}};
 CourseDirector nav(navigation,1,2,0);
 auto straight=nav.guidance(0,pose(0,80));assert(straight.available&&std::abs(straight.yaw)<1e-6&&std::abs(straight.distance-20)<1e-6);
 auto right=nav.guidance(0,pose(0,190));assert(right.available&&right.yaw>0&&right.target[0]>0&&std::abs(right.target[2]-200)<1e-6);
 navigation["points"]={{0,0,0},{0,0,200},{-200,0,200},{-200,0,0}};
 CourseDirector left_route(navigation,1,2,0);assert(left_route.guidance(0,pose(0,190)).yaw<0);
 auto reverse=pose(200,100);reverse.chassis[0]=-1;reverse.chassis[10]=-1;
 assert(std::abs(nav.guidance(0,reverse).yaw)<1e-6);
 auto seam=pose(10,0);seam.speed_kmh=80;seam.chassis[0]=0;seam.chassis[2]=1;seam.chassis[8]=-1;seam.chassis[10]=0;
 auto wrap=nav.guidance(0,seam);assert(wrap.available&&wrap.target[0]==0&&std::abs(wrap.target[2]-30)<1e-6&&wrap.yaw>0);
 auto recover=nav.guidance(0,pose(50,80));assert(recover.available&&recover.yaw<0);
 auto fast=pose(0,80);fast.speed_kmh=300;assert(std::abs(nav.guidance(0,fast).distance-40)<1e-6);
 auto invalid=pose(0,80);invalid.chassis[12]=NAN;assert(!nav.guidance(0,invalid).available);
 assert(!r.guidance(1,pose(10,0)).available); // A finished driver's arrow disappears.
 Json levels={{"points",{{0,0,0},{0,0,10},{0,20,10},{0,20,0}}}};
 CourseDirector elevated(levels,1,1,0);auto upper=off;upper.chassis[12]=1;upper.chassis[13]=20;
 auto high=elevated.recovery_transform(upper);assert(high&&std::abs((*high)[13]-20)<1e-6);
 // AIRaceLine vertical control handles must not pull XZ projection away from road.
 Json handles={{"points",{{0,0,0},{0,20,100},{100,20,100},{100,0,0}}}};
 CourseDirector handle_route(handles,1,1,0);auto low_car=pose(2,50);auto projected=handle_route.recovery_transform(low_car);assert(projected&&std::abs((*projected)[14]-50)<1e-4);
 Json duplicate={{"points",{{0,0,0},{0,0,0},{0,0,10},{10,0,10}}}};
 CourseDirector repeated(duplicate,1,1,0);assert(repeated.recovery_transform(off));
 CourseDirector jam(navigation,2,3,1);std::vector<CoursePose> parked{pose(0,0),pose(0,0)};bool backed=false;for(int i=0;i<500;++i){auto c=jam.controls(parked);backed|=c[1].throttle<0;jam.update(.01,parked);}assert(backed);parked[1].driver_ejected=true;for(int i=0;i<410;++i)jam.update(.01,parked);assert(jam.recovery_requested(1));jam.relocated(1,parked[1]);assert(!jam.recovery_requested(1));
 if(argc>1){Resources res(argv[1]);auto c=race_course(res,"data/tracks/arena/arena1/a/");assert(c["available"]==true);assert(c["points"].size()>30);assert(c["gates"].size()==6);assert(std::abs(c["points"][0][0].get<double>()-7.829942)<.001);
 auto equivalent=race_course(res,"tracks/arena/arena1/a");assert(equivalent["points"]==c["points"]);CourseDirector real(c,1,3,0);assert(real.available());auto start=pose(c["points"][0][0],c["points"][0][2]);assert(real.guidance(0,start).available);
 auto forest=race_course(res,"tracks/forest/forest1/a");assert(forest["available"]==true);assert(forest["points"].size()>30);
 std::cout<<"course_ui: synthetic gates, lap timing, finish, leaderboard, recovery, steering guidance, real arena and forest route tests passed\n";}
}
