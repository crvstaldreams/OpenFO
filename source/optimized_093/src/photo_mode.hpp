#pragma once
#include "math.hpp"
namespace ofo {
// Angles are radians; field of view is degrees, as in the gameplay camera.
struct PhotoCamera {Vec eye;float yaw=0,pitch=0,roll=0,fov=60;};
struct PhotoInput {
 float move_x=0,move_y=0,move_z=0;
 float yaw=0,pitch=0,roll=0,zoom=0;
 bool fast=false;
 float mouse_x=0,mouse_y=0;
};
PhotoCamera photo_camera_begin(Vec eye,Vec target,float fov=60);
Vec photo_forward(const PhotoCamera&);
Vec photo_right(const PhotoCamera&);
Vec photo_up(const PhotoCamera&);
Mat photo_view(const PhotoCamera&);
void photo_camera_step(PhotoCamera&,const PhotoInput&,double dt);
struct PhotoSettings {int preset=0;float exposure=0,contrast=1,saturation=1,vignette=0;};
void photo_sanitize(PhotoSettings&);
const char* photo_preset_name(int preset);
// Reference for the native photo shader. Coordinates are normalized image UVs.
Vec photo_grade(Vec rgb,PhotoSettings,float u=.5f,float v=.5f);
}
