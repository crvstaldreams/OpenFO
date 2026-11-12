#pragma once
#include "assets.hpp"

namespace ofo {
struct SteeringMount {
 size_t mesh=0;
 Mat frame=identity();
 float radius=0,rim_z=0;
 Vec local_center{};
};
std::optional<SteeringMount> steering_mount(const Geometry&);

struct MenuSteeringWheel {
 Mesh rim;
 std::string texture;
 float radius=0;
 bool apply(Geometry&) const;
};

MenuSteeringWheel menu_steering_from_track(const Resources&,const Geometry&);
MenuSteeringWheel load_menu_steering(const Resources&);
}
