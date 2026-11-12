#include "meter_artwork.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ofo;
void check(bool value, const char* message) {
 if (!value) throw std::runtime_error(message);
}
bool close(float a, float b) { return std::abs(a-b)<.001f; }
int main() {
 try {
  constexpr float rad=3.14159265f/180;
  check(close(muscle_meter_angle(0,240),135*rad),"speed scale starts at the lower left");
  check(close(muscle_meter_angle(120,240),270*rad),"120 km/h aligns with the center marking");
  check(close(muscle_meter_angle(75,150),270*rad),"75 mph aligns with the center marking");
  check(close(muscle_meter_angle(10000,10000),405*rad),"tachometer ends at the 10 marking");
  check(close(muscle_meter_angle(-9,150),135*rad),"negative readings cannot underflow the scale");
  check(close(muscle_meter_angle(160,150),405*rad),"above-range needle saturates");
  check(close(muscle_meter_angle(std::numeric_limits<float>::quiet_NaN(),240),135*rad),"nonfinite readings remain drawable");
  auto metric=muscle_meter_readings(false,false,false,-25,3400,4,.57f,.9f);
  check(metric.speed==90&&metric.gear==4&&metric.segments==6,"race reads speed magnitude, gear and nitro");
  auto derby=muscle_meter_readings(true,true,false,25,3400,-1,.05f,.8f);
  check(derby.speed==55&&derby.gear==10&&derby.segments==8,"derby uses mph, reverse and damage");
  auto stunt=muscle_meter_readings(false,false,true,25,3400,0,1,1);
  check(stunt.gear==11&&stunt.segments==0,"stunt omits meter and shows neutral");
  auto invalid=muscle_meter_readings(false,false,false,std::numeric_limits<float>::infinity(),3400,60,
                                     std::numeric_limits<float>::quiet_NaN(),1);
  check(invalid.speed==0&&invalid.gear==9&&invalid.segments==0,"corrupt readings are bounded");
  std::cout<<"Muscle meter geometry, speed/RPM, units, gears and mode feedback passed\n";
 } catch(const std::exception& error) {
  std::cerr<<error.what()<<'\n';
  return 1;
 }
}
