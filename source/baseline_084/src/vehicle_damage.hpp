#pragma once
#include "assets.hpp"
namespace ofo {
// Chassis-root local coordinates (BGM instance transforms are applied once).
struct VehicleDamageContact { Vec position{},normal{}; float radius=0,depth=0,impulse=0,closing_speed=0; };
struct CrashProfile {
 float onset=1,full=25,nominal=3.5f,radius_min=.5f,radius_max=.6f,radius_at=25;
};
struct CrashTarget {
 size_t mesh=0;
 std::vector<Vec> positions,normals;
 size_t moved=0;
 std::optional<CrashProfile> prototype_profile;
};
struct CrashTargets { std::vector<CrashTarget> surfaces; size_t records=0,vertices=0,moved=0; };
// Validates names, ordered native surfaces, bind positions/normals/UVs and budgets.
// The returned targets retain Mesh vertex order, indices, materials and transforms.
CrashTargets parse_vehicle_crash(const Bytes&,const Geometry&);
CrashTargets load_vehicle_crash(const Resources&,const Geometry&);
int vehicle_window_pane(const std::string&); // front, rear, left, right; -1 otherwise
struct VehiclePanelDefinition {
 std::string name,marker; std::vector<size_t> meshes;
 Vec center{},lo{},hi{},pivot{},axis{0,1,0}; bool hinge=false;
 bool has_joint_limits=false;
 Mat joint_frame=identity();
 Vec joint_min{},joint_max{},joint_rest{},joint_stiffness{};
 float mass=0,unlock_impulse=50,break_impulse=360,lower_limit=0,upper_limit=0;
 CrashProfile crash;
 bool prototype_momentum=false;
};
std::vector<VehiclePanelDefinition> vehicle_panel_definitions(const Geometry&,const Json& panels);
std::vector<VehiclePanelDefinition> load_vehicle_panels(const Resources&,const Geometry&);
struct VehiclePanelDamage { float damage=0,attachment_momentum=0,cooldown=0,unlock_age=0; bool unlocked=false,detached=false; };
// Pure local-contact rules ported from the OpenGL implementation. Feed only new
// external collision contacts, never resting constraint loads or the dent history.
void tick_vehicle_panel(VehiclePanelDamage&,const VehiclePanelDefinition&,float dt);
void hit_vehicle_panel(VehiclePanelDamage&,const VehiclePanelDefinition&,const VehicleDamageContact&,float car_mass,bool direct=false,float durability=.5f);
Mat vehicle_panel_hinge(const VehiclePanelDefinition&,float angle);
// Copies source geometry, interpolates only authored crash targets near contacts,
// switches existing damaged paint/glass textures, and can remove the front glass.
// No procedural deformation is generated when an authored target is absent.
Geometry vehicle_damage_geometry(const Geometry&,const CrashTargets&,float damage,const std::vector<VehicleDamageContact>&,const Resources* resources=nullptr,const std::array<float,4>&glass={},bool front_glass_removed=false);
std::array<float,4> vehicle_glass_damage(const Geometry&,const std::vector<VehicleDamageContact>&,float car_mass);
}
