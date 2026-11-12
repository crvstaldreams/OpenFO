#pragma once
#include "math.hpp"
#include <vector>
namespace ofo {
struct CameraPose {Vec eye{},target{};float fov=80;Vec up{0,1,0};};
struct CinematicCamera {CameraPose pose;int shot=-1;float fade=0,letterbox=0;bool active=false;};
class RaceStartClock {double remaining_=3;public:
 void reset(){remaining_=3;}double remaining()const{return remaining_;}
 double advance(double seconds,bool active){if(!active||!std::isfinite(seconds)||seconds<=0)return 0;double held=std::min(remaining_,seconds);remaining_=std::max(0.,remaining_-held);if(remaining_<1e-9)remaining_=0;return std::max(0.,seconds-held);}
};
struct CameraObstacle {Mat transform=identity();Vec lo{},hi{};};
// Conservative sphere enclosing the near plane plus lens clearance.
float camera_clearance(float fov,float aspect=16.f/9);
bool camera_segment_clear(Vec a,Vec b,const std::vector<CameraObstacle>& obstacles,float clearance);
// Gameplay lens/near-plane clearance; target-to-eye visibility is not a motion sweep.
CameraPose camera_safe_pose(CameraPose pose,const std::vector<CameraObstacle>& obstacles,float aspect=16.f/9);
Vec animated_menu_eye(Vec authored,Vec target,double clock,bool tuning);
CinematicCamera countdown_camera(double remaining,Vec lo,Vec hi,const CameraPose&gameplay,const Mat&car=identity(),const std::vector<CameraObstacle>& obstacles={},float aspect=16.f/9);
float race_hud_opacity(double remaining,double elapsed);
}
