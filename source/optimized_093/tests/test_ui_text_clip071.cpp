#include "ui_text_clip.hpp"
#include <cassert>
int main(){using namespace ofo;auto a=ui_text_clip(-50,200,0,100);assert(a.visible&&a.x==0&&a.width==100&&a.u==.25f&&a.uw==.5f);auto b=ui_text_clip(120,20,0,100);assert(!b.visible);auto c=ui_text_clip(10,20);assert(c.visible&&c.x==10&&c.width==20&&c.u==0&&c.uw==1);assert(!ui_text_clip(1,0).visible);}
