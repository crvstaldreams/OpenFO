#pragma once
#include <algorithm>
#include <cmath>
namespace ofo {
// A held skip key cannot become an activation in the next presentation phase.
struct StartupEnter {bool held=false;explicit StartupEnter(bool initial=false):held(initial){}bool press(bool now){bool edge=now&&!held;held=now;return edge;}};
struct StartupFlow {enum Phase{Movies,Copyright,Loading};Phase phase=Movies;double age=0;void movies_finished(){phase=Copyright;age=0;}bool advance(double seconds){if(phase!=Copyright)return false;if(std::isfinite(seconds)&&seconds>0)age+=seconds;if(age+1e-9<4)return false;phase=Loading;return true;}};
// Presentation changes and Windows exposure/resize are the only repaint causes.
struct StartupPaintPolicy {bool first=true;bool redraw(bool changed,bool exposed){bool draw=first||changed||exposed;first=false;return draw;}};
enum class AudioScene { Silent, Menu, Race };
class PresentedAudioScene {
 AudioScene shown_=AudioScene::Silent;
public:
 void loading(){shown_=AudioScene::Silent;}
 void presented(AudioScene scene){shown_=scene;}
 bool ready(AudioScene scene)const{return scene!=AudioScene::Silent&&shown_==scene;}
};
struct StartupRect {int x=0,y=0,w=0,h=0;};
inline StartupRect startup_fit(int w,int h,int source_w,int source_h){if(w<1||h<1||source_w<1||source_h<1)return {};double scale=std::min(double(w)/source_w,double(h)/source_h);int rw=int(source_w*scale),rh=int(source_h*scale);return {(w-rw)/2,(h-rh)/2,rw,rh};}
}
