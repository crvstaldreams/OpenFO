#include "vehicle_camera_ini.hpp"
#include <cassert>
#include <cmath>
int main(){
 using namespace ofo;
 VehicleCameraINI c;
 c.index=2;
 c.position={-.3f,.7f,.2f};
 c.target={0.f,.9f,3.f};
 c.fov=74.f;
 const CameraPose p=ini_camera_pose(c,identity());
 assert(std::abs(p.eye.x+.3f)<1e-5f);
 assert(std::abs(p.eye.y-.7f)<1e-5f);
 assert(p.target.z>p.eye.z+2.f);
 assert(p.fov==74.f);
 c.target=c.position;
 const CameraPose fallback=ini_camera_pose(c,identity());
 assert(fallback.target.z>fallback.eye.z+.5f);
 return 0;
}
