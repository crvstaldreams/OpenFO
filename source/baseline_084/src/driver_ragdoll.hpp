#pragma once
#include "native_driver.hpp"
#include <memory>
#include <optional>
class btDynamicsWorld;class btRigidBody;
namespace ofo {
struct DriverJointDefinition {size_t parent=0,child=0,anchor_bone=0;bool hinge=false;Vec axis1{},axis2{};std::array<float,2>limit1{},limit2{};Vec max_force{};};
std::vector<DriverJointDefinition> native_driver_joints(const NativeDriver&);
// Authored boxes + relative segment masses (normalized to configured total mass), 10 native
// joints mapped to Bullet. Constraint rest frames use launch pose to avoid snaps.
// Linear velocity is measured at velocity_reference_world (defaults to car root).
// Optional owner must outlive this ragdoll; only driver bodies store its ignore link.
class NativeDriverRagdoll {
 struct Impl;std::unique_ptr<Impl>impl_;
public:
 NativeDriverRagdoll(btDynamicsWorld&,const NativeDriver&,const DriverPose&car_local,const Mat&car_world,Vec linear_velocity,Vec angular_velocity,float total_mass_kg=80.f,std::optional<Vec> velocity_reference_world={},btRigidBody* owner=nullptr);
 ~NativeDriverRagdoll();
 NativeDriverRagdoll(const NativeDriverRagdoll&)=delete;NativeDriverRagdoll&operator=(const NativeDriverRagdoll&)=delete;
 std::array<Mat,driver_bone_count> snapshot()const;
 const std::vector<btRigidBody*>& bodies()const;
 void set_velocity(Vec);void apply_impulse(Vec);
};
}
