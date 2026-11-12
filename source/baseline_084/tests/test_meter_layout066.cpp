#include "meter_artwork.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ofo;
int main(){try{
 auto inside=[](MeterRect r,float l,float t,float right,float bottom){return r.x>=l&&r.y>=t&&r.x+r.w<=right&&r.y+r.h<=bottom;};
 auto layout=meter_instruments();
 if(!inside(layout.speed,16,108,56,122))throw std::runtime_error("speed stays in left rectangle");
 if(!inside(layout.unit,61,108,86,122))throw std::runtime_error("unit stays in middle rectangle");
 if(!inside(layout.gear,105,104,122,121))throw std::runtime_error("gear occupies separate right circle");
 auto needle=meter_needle(0);if(std::abs(needle.pivot_x-72)>1e-4f||std::abs(needle.pivot_y-62)>1e-4f)throw std::runtime_error("needle pivot aligns with tachometer hub");
 if(std::abs(needle.angle-135*3.14159265f/180)>1e-4f||std::abs(meter_needle(10000).angle-405*3.14159265f/180)>1e-4f)throw std::runtime_error("needle sweeps native 0 to 10 tachometer");
 auto bottom=meter_arc_point(113,108,116,0),mid=meter_arc_point(113,108,116,.5f),top=meter_arc_point(113,108,116,1);
 if(bottom.x>=113||mid.x>=113||top.x>=113||bottom.y<=mid.y||mid.y<=top.y)
  throw std::runtime_error("marker travels on the left from bottom to top");
 if(meter_arc_fraction(0,0,85)!=0||meter_arc_fraction(42.5f,0,85)!=.5f||meter_arc_fraction(85,0,85)!=1)
  throw std::runtime_error("stunt launch range fills the same arc as the other meters");
 std::cout<<"meter: speed, unit, gear cells and tachometer pivot/sweep passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
