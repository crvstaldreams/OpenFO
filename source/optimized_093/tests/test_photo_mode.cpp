#include "photo_mode.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ofo;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static bool close(float a,float b){return std::abs(a-b)<1e-5f;}
int main(){try{
 auto c=photo_camera_begin({2,3,4},{2,3,5},72);need(close(c.fov,72)&&length(photo_forward(c)-Vec{0,0,1})<1e-5f,"begin preserves view");
 auto m=photo_view(c);need(length(point(m,c.eye))<1e-5f,"view origin is camera eye");
 PhotoInput i;i.move_z=1;photo_camera_step(c,i,1);need(close(c.eye.z,5.2f),"elapsed clamp bounds camera movement");
 auto fast=photo_camera_begin({0,0,0},{0,0,1});i.fast=true;photo_camera_step(fast,i,.1);need(close(fast.eye.z,4.8f),"shift multiplies movement speed");
 i={};i.pitch=1000;i.roll=1000;i.zoom=1000;photo_camera_step(c,i,.1);need(c.pitch<1.56f&&c.pitch>1.55f&&c.roll>=-3.142f&&c.roll<=3.142f&&c.fov==110,"pitch roll and FOV are bounded");
 c.roll=1.57079632679f;need(std::abs(dot(photo_right(c),photo_up(c)))<1e-5f&&close(length(photo_right(c)),1),"rolled camera orthonormal");
 auto v=photo_view(c);need(length(direction(v,photo_right(c))-Vec{1,0,0})<1e-5f,"view honors camera roll");
 PhotoSettings s;Vec color{.2f,.4f,.7f};need(length(photo_grade(color,s)-color)<1e-6f,"native default preserves color");
 s.preset=3;auto bw=photo_grade(color,s);need(close(bw.x,bw.y)&&close(bw.y,bw.z),"B&W has equal channels");
 s={};s.saturation=0;auto grey=photo_grade(color,s);need(close(grey.x,grey.z),"zero saturation produces gray");
 s={};s.exposure=1;need(close(photo_grade({.1f,.2f,.3f},s).z,.6f),"exposure uses stops");
 s={};s.vignette=1;need(length(photo_grade(color,s,0,0))<1e-6f&&length(photo_grade(color,s)-color)<1e-6f,"vignette darkens corners only");
 for(int p=0;p<6;++p){s={};s.preset=p;auto out=photo_grade(color,s);need(out.x>=0&&out.x<=1&&out.y>=0&&out.y<=1&&out.z>=0&&out.z<=1,"preset output bounded");need(photo_preset_name(p)[0]!=0,"preset label exists");}
 s={9,50,8,-3,-1};photo_sanitize(s);need(s.preset==5&&s.exposure==3&&s.contrast==2&&s.saturation==0&&s.vignette==0,"settings clamp");
 std::cout<<"photo camera and color tests passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
