#include "race_camera.hpp"
#include "content.hpp"
#include <stdexcept>
#include <iostream>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 Resources r(argc>1?argv[1]:"/tmp/ofo068-data");auto d=load_native_driver(r);
 need(race_camera_count==3&&next_race_camera(0)==1&&next_race_camera(1)==2&&next_race_camera(2)==0,"exactly follow bumper cockpit");
 for(int id=1;id<=16;id++){
 auto cfg=load_vehicle(r,id);auto p=load_driver_placement(r,id,load_geometry(r,cfg.body),&d);auto seated=sample_seated_driver_pose(d,p,.4f);
 auto bumper=race_camera_pose(1,identity(),cfg.lo,cfg.hi,{0,2,-6},{0,1,1},80,&seated);
 need(bumper.eye.z>cfg.hi.z+.1f,"bumper lens clears every native front body bound");need(bumper.eye.y>cfg.lo.y&&bumper.eye.y<cfg.hi.y,"bumper lies within body height");
 auto cockpit=race_camera_pose(2,identity(),cfg.lo,cfg.hi,{0,2,-6},{0,1,1},80,&seated);
 need(dot(unit(cockpit.target-cockpit.eye),unit({0,-.045f,1}))>.999f,"Cockpit aims through the windscreen");
 need(length(cockpit.eye-(point(seated.parts[6],{})+Vec{0,.018f,.157f}))<1e-5f,"cockpit uses seated native head");
 Mat roll=identity();roll[0]=roll[5]=std::cos(2.8f);roll[1]=std::sin(2.8f);roll[4]=-roll[1];
 for(const Mat&inverted:{multiply(translate({3,4,5}),multiply(rotate_y(.7f),rotate_x(3.14159265f))),multiply(translate({1,2,3}),multiply(rotate_x(.8f),roll))}){
 auto flipped=race_camera_pose(2,inverted,cfg.lo,cfg.hi,{0,2,-6},{0,1,1},80,&seated);
 need(length(flipped.eye-point(inverted,cockpit.eye))<1e-5f,"upside down eye follows chassis");
 need(dot(unit(flipped.target-flipped.eye),unit(direction(inverted,cockpit.target-cockpit.eye)))>.9999f,"upside down head forward follows chassis");
 need(dot(flipped.up,unit(direction(inverted,cockpit.up)))>.9999f,"upside down head up follows roll and pitch");
 auto view=race_camera_view(flipped);need(direction(view,flipped.up).y>.999f,"view matrix retains inverted up");
 }
 }
 std::cout<<"three camera modes, all 16 bumper bounds/native head placements, inverted eye/forward/up passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
