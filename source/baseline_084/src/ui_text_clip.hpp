#pragma once
#include <algorithm>
#include <cmath>
namespace ofo {
struct UiTextClip {float x=0,width=0,u=0,uw=1;bool visible=false;};
inline UiTextClip ui_text_clip(float x,float width,float left=-1,float right=-1){
 UiTextClip c;c.x=x;c.width=width;if(!std::isfinite(x)||!std::isfinite(width)||width<=0)return c;
 float a=left>=0?std::max(x,left):x,b=right>=0?std::min(x+width,right):x+width;if(b<=a)return c;
 c.x=a;c.width=b-a;c.u=(a-x)/width;c.uw=c.width/width;c.visible=true;return c;
}
}
