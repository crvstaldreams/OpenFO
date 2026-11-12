#include "course_ui.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ofo;
namespace {
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
CoursePose pose(double x,double z,double y=0){
 CoursePose p;p.chassis[12]=float(x);p.chassis[13]=float(y);p.chassis[14]=float(z);return p;
}
}
int main(int argc,char**argv) try {
 Json route={
  {"points",{{0,20,0},{0,20,100},{100,20,100},{100,20,0}}},
  {"gates",{
   {{"position",{0,0,20}},{"left",{-40,0,20}},{"right",{40,0,20}}},
   {{"position",{0,0,80}},{"left",{-40,0,80}},{"right",{40,0,80}}}
  }}
 };
 // Asymmetric mask in original top-to-bottom world orientation. Use a 0/1
 // palette on one branch and 0/255 on the other, as original stages do.
 Bytes bytes(140*140);
 for(size_t row=0;row<140;row++)for(size_t col=0;col<140;col++){
  const double z=119.5-double(row),x=double(col)-19.5;
  if(z<0||z>100)continue;
  if(std::abs(x)<(z<50?7.:4.))bytes[row*140+col]=1;
  if(std::abs(x-100)<7)bytes[row*140+col]=255;
 }
 route["reset_map"]={{"side",140},{"origin",{-20,120}},{"span",{140,-140}},{"mask",Json::binary(std::move(bytes))}};
 CourseDirector c(route,1,2,0);
 check(c.recovery_candidate_valid(pose(5,25).chassis)&&!c.recovery_candidate_valid(pose(5,75).chassis),"reset map reversed Z or ignored its asymmetric authored boundary");
 c.update(.01,{pose(0,15,13)});c.update(.01,{pose(0,25,13)});
 check(!c.off_route()&&c.hud()[0]["next_gate"]==1,"on-route checkpoint crossing failed");
 c.update(.01,{pose(10,25,15)});
 check(c.off_route()&&c.off_route_distance(pose(10,25,42))==0,"departure position or XZ distance not registered");
 auto anchor=c.departure_transform();
 check(anchor&&(*anchor)[12]==0&&(*anchor)[13]==13&&(*anchor)[14]==25&&(*anchor)[10]==1,"return anchor does not retain last valid height and route heading");
 check(c.recovery_candidate_valid(*anchor),"return anchor outside native route mask");
 c.update(.01,{pose(13,29,-20)});
 check(std::abs(c.off_route_distance(pose(13,29,500))-5.)<1e-5&&c.departure_transform()==anchor,"departure moved while off route");
 c.update(.01,{pose(13,70)});c.update(.01,{pose(13,85)});
 check(c.hud()[0]["next_gate"]==1&&c.hud()[0]["lap"]==1,"off-route path advanced checkpoints");
 c.update(.01,{pose(100,25)});
 check(c.off_route(),"rejoining a different part of the route cleared departure");
 c.update(.01,{pose(0,25)});
 check(!c.off_route()&&!c.departure_transform()&&c.off_route_distance(pose(0,25))<0,"legitimate local re-entry did not clear departure");
 c.update(.01,{pose(15,25)});
 const auto timing=c.hud()[0];
 c.relocated(0,pose(0,25));
 check(!c.off_route()&&c.hud()[0]==timing,"recovery altered time, lap or gate");

 route.erase("reset_map");
 CourseDirector fallback(route,1,2,0);
 fallback.update(.01,{pose(0,25,13)});fallback.update(.01,{pose(25,25)});
 check(fallback.off_route(),"fallback corridor did not detect exit");
 fallback.update(.01,{pose(23,25)});
 check(fallback.off_route(),"fallback re-entry hysteresis was lost");
 fallback.update(.01,{pose(20,25)});
 check(!fallback.off_route(),"fallback corridor did not clear local re-entry");

 if(argc>1){
  Resources r(argv[1]);
  auto forest=race_course(r,"tracks/forest/forest1/a");
  check(forest.contains("reset_map"),"original Forest1 reset map did not load");
  CourseDirector original(forest,1,3,0);
  auto spawn=pose(-17.595631,206.036591,-17.992197);
  original.update(.01,{spawn});
  check(!original.off_route(),"original Forest1 starting grid marked off course");
  original.update(.01,{pose(0,0)});
  check(original.off_route()&&original.recovery_candidate_valid(*original.departure_transform()),"original Forest1 infield and return anchor classified incorrectly");
  auto town=race_course(r,"tracks/town/town2/a");
  check(town.contains("reset_map"),"original Town2 0/1 reset map did not load");
  CourseDirector town_course(town,1,3,0);
  auto first=town["points"][0];
  town_course.update(.01,{pose(first[0].get<double>(),first[2].get<double>())});
  check(!town_course.off_route(),"original Town2 valid mask value 1 rejected");
 }
 std::cout<<"off route: native and fallback bounds, fixed XZ departure, route height, anti-cutting and relocation passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
