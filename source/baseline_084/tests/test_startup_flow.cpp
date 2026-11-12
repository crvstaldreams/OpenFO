#undef NDEBUG
#include "startup_flow.hpp"
#include <cassert>
int main(){
 ofo::StartupPaintPolicy paint;assert(paint.redraw(false,false));assert(!paint.redraw(false,false));assert(paint.redraw(true,false));assert(paint.redraw(false,true));assert(!paint.redraw(false,false));
 ofo::StartupEnter edge(true);assert(!edge.press(true));assert(!edge.press(false));assert(edge.press(true));assert(!edge.press(true));assert(!edge.press(false));assert(edge.press(true));
 ofo::StartupFlow f;assert(f.phase==ofo::StartupFlow::Movies);f.movies_finished();assert(f.phase==ofo::StartupFlow::Copyright);assert(!f.advance(3.99));assert(f.advance(.01));assert(f.phase==ofo::StartupFlow::Loading);assert(!f.advance(50));
 auto r=ofo::startup_fit(1280,720,640,480);assert(r.x==160&&r.y==0&&r.w==960&&r.h==720);
 auto p=ofo::startup_fit(800,600,1920,1080);assert(p.x==0&&p.y==75&&p.w==800&&p.h==450);
}
