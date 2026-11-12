#pragma once
#include "vehicle_damage.hpp"
#include <memory>
class btDiscreteDynamicsWorld;class btRigidBody;
namespace ofo {
struct VehiclePanelPose {size_t panel=0;Mat transform=identity();float damage=0;bool detached=false,unlocked=false;};
class VehiclePanelPhysics {struct Impl;std::unique_ptr<Impl>p_;public:
 VehiclePanelPhysics(btDiscreteDynamicsWorld&,btRigidBody&,Vec center_mass,std::vector<VehiclePanelDefinition>);
 ~VehiclePanelPhysics();void update(const std::vector<VehicleDamageContact>&,float dt);void clear();
 void recover_attached(const Mat& previous_car);
 std::shared_ptr<const std::vector<VehiclePanelPose>> snapshot()const;size_t detached()const;
};
}
