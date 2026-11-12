#pragma once
#include "math.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
namespace ofo {
namespace fs=std::filesystem;using Json=nlohmann::json;using Bytes=std::vector<uint8_t>;
std::string lower(std::string);std::string clean_path(std::string);
class Resources {fs::path root_;std::map<std::string,fs::path> index_;public:explicit Resources(fs::path);void mount(fs::path,std::string);size_t overlay_textures(fs::path);bool exists(std::string)const;Bytes read(std::string)const;std::string text(std::string)const;std::vector<std::string> paths()const;std::string texture(std::string geometry,std::string reference)const;};
struct Vertex {Vec position,normal;uint32_t color=0xffffffff;float u=0,v=0,lu=0,lv=0;};
struct Material {std::string name,texture[3];uint32_t shader=0,alpha=0;};
struct Mesh {int prop_index=-1;int source_surface=-1;bool captured=false;bool solid=true;bool billboard=false;std::string name;size_t material=0;Mat transform=identity();Vec center{};float radius=0;std::vector<Vertex> vertices;std::vector<uint32_t> indices;};
struct PropJointDefinition {enum Kind {None,Hinge,Pole};Kind kind=None;Vec pivot{},axis{0,1,0};std::array<float,2> x_limits{},z_limits{};float max_force=0,velocity=0,break_force=0,break_torque=0;};
struct PropBonus {int type=0,cents=0;float nitro=0;};
struct LogContact {float wakeup_velocity=.25f,reactivation_velocity=.25f;};
struct PropDefinition {uint64_t id=0;float mass=1,restitution=.1f;Vec inertia{},center{},half{.5f,.5f,.5f};Mat transform=identity();int collision_sound=0;PropJointDefinition joint;PropBonus bonus;std::optional<LogContact> log_contact;};
struct PropPose {size_t index=0;Mat transform=identity();};
// Native compact-object identity is track-local; surface IDs cannot be shared across tracks.
struct NativePropPlacement {std::string name,type,model_name;uint32_t model_flags=0,placement_flags=0,group_index=0xffffffffu;size_t native_index=0;Mat transform=identity();Vec center{},half{};std::vector<uint32_t> surfaces;};
struct TrackAssetId { std::string source_geometry; uint64_t source_id=0; };
struct Geometry {std::vector<NativePropPlacement> native_props;std::vector<PropDefinition>props;std::string source,lighting_source,stage_source;size_t filtered_compact_instances=0;size_t native_static_batches=0,native_tree_instances=0,native_tree_batches=0,native_compact_instances=0,native_compact_batches=0;std::vector<Material> materials;std::vector<Mesh> meshes;std::map<std::string,Mat> markers;size_t triangles=0;bool track=false;std::vector<Mesh> collision_meshes;std::map<size_t,TrackAssetId> editor_template_props;};
Json geometry_summary(const Geometry&);
Geometry load_geometry(const Resources&,const std::string&,const std::string& stage="");
// Compact prop geometry from all stage variants of one source location.
Geometry load_track_prop_geometry(const Resources&,const std::string&);
Json parse_bed(const std::string&);Json bed_file(const Resources&,const std::string&);Vec vector3(const Json&);
struct WheelConfig {std::string mesh_name;std::vector<Vec> collision_vertices;Vec mesh_scale{1,1,1};Vec mount,arch_offset;float width=.185f;float mass=35,inertia_factor=1,radius=.305f,rest=.32f,min=.05f,max=.55f,k=18000,bump=1200,rebound=1400;};
struct TireSurfaceDynamics {float longitudinal=1,lateral=1,rolling_resistance=0;};
struct TireDynamics {float longitudinal=1,lateral=1,optimal_slip_ratio=.1f,optimal_slip_angle=28,optimal_slip_load=1.5f,optimal_load_factor=1.2f,rolling_resistance=1;bool authored=false;};
struct VehicleConfig {TireDynamics tire_dynamics;std::map<int,TireSurfaceDynamics> tire_surfaces;std::array<float,2>damping{{.01f,.05f}};Vec aero_location{},downforce_location{};int id=3;std::string name,body,tire,profile;float mass=0,authored_mass=0;Vec inertia,center_mass,lo,hi,top_lo{},top_hi{};bool has_top_collision=false;std::array<WheelConfig,4>wheels;float engine_torque=125,idle_rpm=950,max_rpm=7000,final_ratio=3.8f,nitro_g=1.2f,nitro_capacity=5,brake_torque=8500,handbrake=5000,steering_limit=45,steering_reduction=.4f;float steering_angle=35,downforce=2;Vec drag{1.2f,1.6f,.4f};float front_drive=0,mu=1.05f;float engine_inertia=.12f,shaft_inertia=.06f,engine_friction=.055f,reverse_ratio=-6.45f,clutch_torque=400,clutch_release=.1f,gear_shift=.08f,clutch_engage=.15f,brake_front=.55f;std::vector<float> ratios;std::vector<std::pair<float,float>>torque_curve;Json provenance;};
struct CarDisplayInfo {std::string name,mass_text;};
VehicleConfig load_vehicle(const Resources&,int,const fs::path&reference_profiles={});
Mat load_spawn(const Resources&,const std::string&stage,int index=1);std::vector<std::string> track_catalog(const Resources&);std::vector<std::string> first_build_stages(const Resources&);std::string stage_geometry(const std::string&);
}
