#include "presentation_camera.hpp"
#include "frontend.hpp"
#include "frontend_hud.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
// Independent test oracle: sample the swept lens sphere against expanded OBBs.
bool lens_clear(Vec eye,const std::vector<CameraObstacle>& obstacles,float radius){
 for(const auto&o:obstacles){Vec delta=eye-Vec{o.transform[12],o.transform[13],o.transform[14]};
  float x=delta.x*o.transform[0]+delta.y*o.transform[1]+delta.z*o.transform[2];
  float y=delta.x*o.transform[4]+delta.y*o.transform[5]+delta.z*o.transform[6];
  float z=delta.x*o.transform[8]+delta.y*o.transform[9]+delta.z*o.transform[10];
  if(x>=o.lo.x-radius&&x<=o.hi.x+radius&&y>=o.lo.y-radius&&y<=o.hi.y+radius&&z>=o.lo.z-radius&&z<=o.hi.z+radius)return false;
 }return true;
}
int main(int argc,char**argv){try{
 RaceStartClock clock;double active=0;for(int i=0;i<29;i++)active+=clock.advance(.1,true);need(active<1e-6&&clock.remaining()>.09,"physics held through3-2-1");double before=clock.remaining();clock.advance(1,false);need(clock.remaining()==before,"pause/focus loss freezes cinematic");auto extra=clock.advance(.15,true);need(std::abs(extra-.05)<1e-6,"only post-GO fraction goes to physics");need(clock.remaining()==0,"GO exactly at three seconds");clock.reset();need(clock.remaining()==3,"restart resets3-2-1");RaceStartClock fast;for(int i=0;i<360;i++)fast.advance(1./120,true);need(fast.remaining()<1e-8,"countdown not frame-count based");
 CameraPose gameplay{{0,2,-5},{0,0,0},80};Vec lo{-.8f,.1f,-1.8f},hi{.8f,1.2f,1.8f};
 for(int i=0;i<=300;i++){double remaining=3-i*.01;auto c=countdown_camera(remaining,lo,hi,gameplay);need(std::isfinite(c.pose.eye.x)&&std::isfinite(c.pose.eye.y)&&std::isfinite(c.pose.fov),"finite cinematic pose");need(length(c.pose.eye-c.pose.target)>.5f,"nondegenerate view");need(c.fade>=0&&c.fade<=1&&c.letterbox>=0&&c.letterbox<=1,"bounded transition opacity");if(remaining>0)need(c.pose.eye.x<lo.x||c.pose.eye.x>hi.x||c.pose.eye.z<lo.z||c.pose.eye.z>hi.z,"camera stays outside car bounds");}
 need(countdown_camera(2.5,lo,hi,gameplay).shot==0&&countdown_camera(1.5,lo,hi,gameplay).shot==1&&countdown_camera(.5,lo,hi,gameplay).shot==2,"three car flyby shots");auto end=countdown_camera(0,lo,hi,gameplay);need(length(end.pose.eye-gameplay.eye)<1e-5&&length(end.pose.target-gameplay.target)<1e-5&&end.pose.fov==gameplay.fov,"seamless handoff to selected gameplay camera");auto almost=countdown_camera(.00001,lo,hi,gameplay);need(length(almost.pose.eye-end.pose.eye)<.001f,"no camera jump at GO");need(race_hud_opacity(1,0)==0&&race_hud_opacity(0,.1)>0&&race_hud_opacity(0,1)==1,"HUD appears only after GO");
 // Native-sized dense grids, rotated as a whole and with individually rotated cars.
 for(float aspect:{4.f/3,16.f/9,32.f/9})for(int heading=0;heading<8;heading++){
  Mat headingPose=multiply(translate({12,1,-18}),rotate_y(heading*3.14159265f/4));std::vector<CameraObstacle> grid;
  for(int row=-2;row<=2;row++)for(int column=-2;column<=2;column++){
   auto pose=multiply(headingPose,multiply(translate({column*2.5f,0,row*5.f}),rotate_y((row+column)*.07f)));
   grid.push_back({pose,{-.95f,0,-2.1f},{.95f,1.65f,2.1f}});
  }
  CameraPose desired{point(headingPose,{0,2,-6}),point(headingPose,{0,.8f,0}),80};auto safe=camera_safe_pose(desired,grid,aspect);
  CameraPose previous;bool havePrevious=false;float clearance=camera_clearance(80,aspect);
  for(int frame=0;frame<=720;frame++){
   auto c=countdown_camera(3-frame/240.,lo,hi,safe,headingPose,grid,aspect);
   need(lens_clear(c.pose.eye,grid,clearance),"cinematic eye and near plane clear dense grid");
   if(havePrevious)for(int sample=0;sample<=16;sample++)need(lens_clear(previous.eye+(c.pose.eye-previous.eye)*(sample/16.f),grid,clearance),"entire camera movement segment clears grid");
   previous=c.pose;havePrevious=true;
  }
  need(length(previous.eye-safe.eye)<1e-5f,"collision-cleared seamless gameplay handoff");
 }
 auto empty=fs::temp_directory_path()/"ofo_cinematic_ui";fs::create_directories(empty);Resources r(argc>1?fs::path(argv[1]):empty);FrontendHud hud(r);RaceOverlayState state;state.countdown=2.8;state.music_title="TRACK";state.music_age=2;state.show_positions=true;state.leaderboard.push_back({1,"PLAYER",1,3,0,false,true,""});for(auto&c:hud.commands(1280,720,state))need(c.kind!=FrontendDraw::Rect&&c.value!="TOTAL TIME"&&c.value!="RACE POSITIONS"&&c.value.find("musicbar")==std::string::npos,"countdown has no race HUD");
 if(argc>1){Frontend menu(r);for(auto page:{MenuPage::Main,MenuPage::Garage,MenuPage::CarSelect,MenuPage::Tuning}){menu.page=page;auto a=menu.showroom(0),b=menu.showroom(3);need(a.enabled&&b.enabled,"menu showroom enabled");if(page==MenuPage::CarSelect)need(length(a.eye-b.eye)<1e-5f&&length(a.target-b.target)<1e-5f&&a.fov==b.fov,"car select composition stays static");else need(length(a.eye-b.eye)>.01f,"other menu scenes retain camera motion");}menu.open_pause();need(!menu.showroom(3).enabled,"pause remains2D over frozen world");}
 std::cout<<"camera presentation: menu motion,3shots,handoff,countdown timing and HUD gating passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
