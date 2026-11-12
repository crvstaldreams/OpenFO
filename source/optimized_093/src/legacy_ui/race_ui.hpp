#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace ofo {
// Event semantics match the source game mode branches; split_meters is for
// split-screen, never a substitute for the stunt face.
inline std::string meter_face(bool battle,bool stunt){return battle?"meters_based.dds":stunt?"meters_base.dds":"meters_basen.dds";}
inline bool meter_nitro_visible(bool battle,bool stunt){return !battle&&!stunt;}
// Smooth one-second selection pulse. Timing is a compatibility setting:
// the supplied HUD-boundary hooks do not capture the native UI pulse clock.
inline float selection_brightness(double clock){
 if(!std::isfinite(clock))return 1;
 return float(.7+.3*std::cos(6.283185307179586*clock));
}
// Visual cadence is a prototype setting, not recovered original timing.
inline float nitro_lamp(double charge,bool active,double clock){
 if(!std::isfinite(charge)||charge<=0)return 0;
 if(active)return 1;
 return std::fmod(std::max(0.,clock),.6)<.3?1.f:.15f;
}
struct ReplayTransport {
 double position=0,rate=1;bool paused=false;int selected=0;
 void start(double first){position=first;rate=1;paused=false;selected=0;}
 void restart(double first){position=first;paused=false;}
 void advance(double dt,double first,double last){
  if(!std::isfinite(first)||!std::isfinite(last)||last<first)return;
  position=std::clamp(position,first,last);
  if(!paused&&std::isfinite(dt)&&dt>0){position=std::min(last,position+dt*rate);if(position>=last)paused=true;}
 }
 void slower(){rate=std::max(.25,rate*.5);paused=false;}
 void faster(){rate=std::min(4.,rate*2);paused=false;}
 // Five source transport commands: exit/restart/pause/fast/slow.
 bool activate(double first){switch(selected){case 0:return true;case 1:restart(first);break;case 2:paused=!paused;break;case 3:faster();break;case 4:slower();break;}return false;}
};
enum class RaceEndStage { Racing,Finish,Results };
struct RaceEndFlow {
 RaceEndStage stage=RaceEndStage::Racing;double age=0;
 void reset(){stage=RaceEndStage::Racing;age=0;}
 void update(bool finished,double dt){
  if(!finished){reset();return;}
  if(stage==RaceEndStage::Racing){stage=RaceEndStage::Finish;age=0;}
  if(std::isfinite(dt)&&dt>0)age+=dt;
  if(stage==RaceEndStage::Finish&&age>=1.5)stage=RaceEndStage::Results;
 }
 void show_results(){if(stage==RaceEndStage::Finish)stage=RaceEndStage::Results;}
};
// Fixed 4:3 source menu canvas; both draw and hit testing use this rectangle.
struct MenuViewport {
 double x=0,y=0,width=0,height=0,screen_height=0;
 MenuViewport(int w,int h){if(w<=0||h<=0)return;double s=std::min(w/640.,h/480.);width=640*s;height=480*s;screen_height=h;x=(w-width)*.5;y=(h-height)*.5;}
 double map_y(double sy)const{double k=width/640.,extra=screen_height-height;return sy*k+(sy<63?0:sy>=365?extra:extra*.5);}
 double source_y(double py)const{double k=width/640.;if(k<=0)return -1;double extra=screen_height-height;if(py<63*k)return py/k;if(py>=map_y(365))return (py-extra)/k;double sy=(py-extra*.5)/k;return sy>=63&&sy<365?sy:-1;}
 bool contains(double px,double py)const{return width>0&&height>0&&px>=x&&px<x+width&&py>=0&&py<screen_height&&source_y(py)>=0;}
};
}
