#pragma once
#include "assets.hpp"
#include "driver_model.hpp"
namespace ofo {
constexpr size_t driver_bone_count=11;
// Road yaw is positive toward +X. A rim rotating about local +Z must turn
// negative so its top moves toward +X, matching the physical road wheels.
inline float steering_wheel_angle(float road_angle){return std::isfinite(road_angle)?-road_angle:0.f;}
struct DriverSkinWeight {uint8_t bone[4]{},weight[4]{};};
struct DriverKey {uint32_t time=0;Vec position;std::array<float,4> rotation{};};
struct DriverTrack {int parent=-1;Mat inverse_bind=identity();std::vector<DriverKey> keys;};
struct DriverSegment {std::string name;int parent=-1;Vec center,dimension,joint;Mat orientation=identity();float mass=0;};
struct DriverHandBind {
 Vec wrist{},along{},palm{};
 bool valid=false;
};
struct NativeDriver {
 Geometry geometry; // Bind mesh vertices, UVs, topology and material. Never rebuilt per frame.
 std::vector<std::vector<DriverSkinWeight>> weights;
 std::array<DriverTrack,driver_bone_count> tracks;
 std::array<DriverSegment,driver_bone_count> segments;
 std::array<DriverHandBind,2> hands{};
 uint32_t start=0,duration=0,rate=0;size_t blended_vertices=0;
 Json native_bones,native_eject_poses;std::string native_behavior; // Original constraint/behavior script, never executed.
};
struct DriverPlacement {Vec seat;Mat steering=identity();float rim_radius=0,rim_z=0;bool has_steering=false;std::string source;std::array<Vec,2> grip{};};
struct DriverPose {std::array<Mat,driver_bone_count> bones,skin,parts;std::array<Vec,2> hand_target{},wrist_target{};std::array<Mat,2> hand_skin{};std::array<bool,2> posed_hands{};bool hands_locked=false;};
NativeDriver load_native_driver(const Resources&,bool female=false);
NativeDriver load_native_driver(const Resources&,DriverModel);
DriverPlacement load_driver_placement(const Resources&,int car_id,const Geometry&body,const NativeDriver* driver=nullptr);
// phase in [0,1] maps directly to original BSA key times; .5 is centered steering.
// Optional road wheel angle applies the existing direct wheel-radian adapter and arm IK.
DriverPose sample_driver_pose(const NativeDriver&,const DriverPlacement&,float phase=.5f,float steering_radians=0,bool lock_hands=true);
// Allocate once, then reuse deform_driver output. Only vertex position/normal and bounds change.
// Gameplay uses the authored neutral seated frame; steering only moves the arms.
// This matches the earlier OpenGL rig and keeps torso/head/legs stable.
DriverPose sample_seated_driver_pose(const NativeDriver&,const DriverPlacement&,float steering_radians=0);
Geometry make_driver_geometry(const NativeDriver&);
void deform_driver(const NativeDriver&,const DriverPose&,Geometry&output);
// Rebuild skin palette from simulation's world-space segment center transforms after ejection.
DriverPose driver_ragdoll_pose(const NativeDriver&,const std::array<Mat,driver_bone_count>&segment_world);
}
