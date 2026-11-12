#pragma once
#include "assets.hpp"
#include "race_ai.hpp"
#include "wheel_effects.hpp"
#include "vehicle_damage.hpp"
#include "native_driver.hpp"
#include "collision_audio.hpp"
#include "vehicle_panel_physics.hpp"
#include <memory>
namespace ofo {
struct Input {float throttle=0,brake=0,steer=0,handbrake=0;bool nitro=false,automatic=true,eject=false;int shift=0;float clutch=0;float stunt_angle=-1;};
struct State {bool stunt_grounded=false;std::shared_ptr<const std::vector<size_t>> stunt_hits;std::array<float,2>headlight_health{{1,1}};std::shared_ptr<const std::vector<OpponentState>> opponents;std::shared_ptr<const std::vector<CollisionEvent>>collision_events;std::shared_ptr<const std::array<Mat,11>>driver_bones;std::shared_ptr<const std::vector<VehiclePanelPose>> panels;std::shared_ptr<const std::vector<VehicleDamageContact>>impacts,collision_contacts;std::array<WheelContact,4>wheel_contacts{};uint64_t recovery_generation=0;float damage=0;bool nitro_active=false;Vec velocity{};std::shared_ptr<const std::vector<PropPose>>props;Mat driver=identity();bool driver_ejected=false;Vec driver_velocity{},driver_acceleration{};float driver_segment_speed=0;Mat car=identity();std::array<Mat,4>wheels{},wheel_mounts{};std::array<float,4>slip{},wheel_load{},suspension{};float rpm=950,speed=0,nitro=1,steer=0;int gear=1,prop_hits=0,prop_bonus=0;std::array<int,10>prop_hits_by_type{},prop_bonus_cents_by_type{};double time=0;};
struct RemoteCollider {int id;Mat car;Vec velocity,lo,hi;float mass;};
class Simulation {struct Impl;std::unique_ptr<Impl>p_;public:Simulation(const Geometry&,VehicleConfig,Mat spawn,std::vector<VehiclePanelDefinition> panels={},std::shared_ptr<const NativeDriver> driver_model={},DriverPlacement driver_placement={});~Simulation();Simulation(const Simulation&)=delete;void step(Input,float dt=.01f);void set_cpu_lanes(unsigned lanes);unsigned cpu_lanes()const;State state()const;void recover();
// Route XZ/yaw is grounded against track geometry. False leaves the car untouched.
bool recover(const Mat& route_target,bool anchor_height=false);void set_opponents(std::vector<RaceOpponentConfig>,RaceRoute);void set_opponent_finished(size_t index,bool finished=true);// Friction-only weather adapter, shared by player and opponents. 1=dry.
void set_remote_colliders(const std::vector<RemoteCollider>&);
void reset_stunt_attempt(const std::vector<PropDefinition>&);
void set_prop_rewards_enabled(bool enabled);void set_grip_scale(float scale);Json diagnostics()const;};
}
