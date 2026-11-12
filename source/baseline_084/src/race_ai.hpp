#pragma once
#include "assets.hpp"
#include "vehicle_panel_physics.hpp"
#include <algorithm>
#include <cmath>
namespace ofo {
struct NativeAIBehavior {
 float aggression=.5f,bump=.5f,block=.3f,overtake=.6f,throttle=1,handicap=.5f,catch_up=.8f;
 float look_min=10,look_max=60,look_modifier=1.2f,brake_time=1,brake_friction=1.2f,friction_adjust=.2f;
 float throttle_min=.8f,throttle_max=.93f,throttle_reduction=.2f,turn_scale=6;
 Json native;
};
struct RaceRoute {bool derby=false;std::vector<Vec>points;float half_width=8;};
struct RaceOpponentConfig {int id=1,car_id=1,skin=1;std::string name;VehicleConfig vehicle;Mat spawn=identity();NativeAIBehavior behavior;Json provenance;std::vector<VehiclePanelDefinition> panels;};
struct OpponentState {std::shared_ptr<const std::vector<VehiclePanelPose>> panels;std::array<float,2>headlight_health{{1,1}};int id=1,car_id=1,skin=1;std::string name;Mat car=identity();std::array<Mat,4>wheels{};Vec velocity{};float speed=0,damage=0;uint64_t recovery_generation=0;bool lan_remote=false,connected=true,driver_ejected=false;double stunt_score=0;};
struct AIVehiclePose {Mat car=identity();Vec velocity{};float damage=0;};
struct AICommand {float throttle=0,brake=0,steer=0,target_speed=0,lane_offset=0;bool recover=false;Mat recovery=identity();};
NativeAIBehavior decode_ai_behavior(const Json&profile,const Json&handicap=Json::object());
RaceRoute race_ai_route(const Json&course);
std::vector<RaceOpponentConfig> load_race_opponents(const Resources&,int car_class,int handicap_level,int upgrade_level,int count,const std::string&stage);
// Route index is a navigation cursor only. RaceDirector owns ordered gate/lap progress.
class RaceAIController {
 RaceRoute route_;NativeAIBehavior behavior_;VehicleConfig vehicle_;float grip_scale_=1,lane_=0;size_t seed_=0,nearest_=0;bool ready_=false;float stuck_=0,failed_=0,misaligned_=0,steer_=0;
 public:RaceAIController(RaceRoute,NativeAIBehavior,size_t seed=0,const VehicleConfig& vehicle=VehicleConfig{});void set_grip_scale(float scale){grip_scale_=std::isfinite(scale)?std::clamp(scale,.5f,1.f):1.f;}AICommand update(const AIVehiclePose&,const std::vector<AIVehiclePose>&traffic,float dt);
};
}
