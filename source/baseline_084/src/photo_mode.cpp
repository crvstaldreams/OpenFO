#include "photo_mode.hpp"
namespace ofo {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr float pitch_limit=89*pi/180;
float finite(float n,float fallback=0){return std::isfinite(n)?n:fallback;}
float bounded(float n,float lo,float hi,float fallback=0){return std::clamp(finite(n,fallback),lo,hi);}
float wrapped(float n){return std::remainder(finite(n),2*pi);}
float luminance(Vec c){return .2126f*c.x+.7152f*c.y+.0722f*c.z;}
Vec saturation(Vec c,float s){float l=luminance(c);return Vec{l,l,l}+(c-Vec{l,l,l})*s;}
}
PhotoCamera photo_camera_begin(Vec eye,Vec target,float fov){
 Vec f=unit(target-eye,{0,0,1});PhotoCamera c;c.eye=eye;c.yaw=std::atan2(f.x,f.z);c.pitch=std::clamp(std::asin(std::clamp(f.y,-1.f,1.f)),-pitch_limit,pitch_limit);c.fov=bounded(fov,15,110,60);return c;
}
Vec photo_forward(const PhotoCamera& c){float cp=std::cos(c.pitch);return {std::sin(c.yaw)*cp,std::sin(c.pitch),std::cos(c.yaw)*cp};}
Vec photo_right(const PhotoCamera& c){Vec f=photo_forward(c),r=unit(cross({0,1,0},f),{1,0,0}),u=cross(f,r);return r*std::cos(c.roll)+u*std::sin(c.roll);}
Vec photo_up(const PhotoCamera& c){return cross(photo_forward(c),photo_right(c));}
Mat photo_view(const PhotoCamera& c){Vec f=photo_forward(c),r=photo_right(c),u=photo_up(c);return{r.x,u.x,f.x,0,r.y,u.y,f.y,0,r.z,u.z,f.z,0,-dot(r,c.eye),-dot(u,c.eye),-dot(f,c.eye),1};}
void photo_camera_step(PhotoCamera& c,const PhotoInput& i,double dt){
 float t=std::isfinite(dt)?float(std::clamp(dt,0.,.1)):0;
 c.yaw=wrapped(c.yaw+finite(i.yaw)*t+finite(i.mouse_x)*.003f);
 c.pitch=bounded(c.pitch+finite(i.pitch)*t-finite(i.mouse_y)*.003f,-pitch_limit,pitch_limit);
 c.roll=wrapped(c.roll+finite(i.roll)*t);
 c.fov=bounded(c.fov+finite(i.zoom)*t,15,110,60);
 Vec local{finite(i.move_x),finite(i.move_y),finite(i.move_z)};float n=length(local);if(n>1)local=local/n;
 c.eye+=(photo_right(c)*local.x+photo_up(c)*local.y+photo_forward(c)*local.z)*(t*(i.fast?48.f:12.f));
}
void photo_sanitize(PhotoSettings& s){s.preset=std::clamp(s.preset,0,5);s.exposure=bounded(s.exposure,-3,3);s.contrast=bounded(s.contrast,.5f,2,1);s.saturation=bounded(s.saturation,0,2,1);s.vignette=bounded(s.vignette,0,1);}
const char* photo_preset_name(int preset){static const char* const names[]={"Native","Vivid","Film","B&W","Sepia","Cool"};return names[std::clamp(preset,0,5)];}
Vec photo_grade(Vec rgb,PhotoSettings s,float u,float v){
 photo_sanitize(s);
 switch(s.preset){
 case 1:rgb=(saturation(rgb,1.25f)-Vec{.5f,.5f,.5f})*1.1f+Vec{.5f,.5f,.5f};break;
 case 2:rgb=rgb*.9f+Vec{.05f,.05f,.05f};rgb={rgb.x*1.04f,rgb.y,rgb.z*.94f};break;
 case 3:{float l=luminance(rgb);rgb={l,l,l};break;}
 case 4:{float l=luminance(rgb);rgb={l*1.12f,l,l*.8f};break;}
 case 5:rgb={rgb.x*.9f,rgb.y*1.02f,rgb.z*1.12f};break;
 default:break;
 }
 rgb=rgb*std::exp2(s.exposure);
 rgb=(rgb-Vec{.5f,.5f,.5f})*s.contrast+Vec{.5f,.5f,.5f};
 rgb=saturation(rgb,s.saturation);
 float x=finite(u,.5f)-.5f,y=finite(v,.5f)-.5f;
 rgb=rgb*(1-s.vignette*std::clamp(2*(x*x+y*y),0.f,1.f));
 return {std::clamp(rgb.x,0.f,1.f),std::clamp(rgb.y,0.f,1.f),std::clamp(rgb.z,0.f,1.f)};
}
}
