#pragma once
#include "assets.hpp"
#include <btBulletDynamicsCommon.h>
#include <functional>
#include <memory>
#include <optional>
namespace ofo {
class ContactWorld:public btDiscreteDynamicsWorld {public:using btDiscreteDynamicsWorld::btDiscreteDynamicsWorld;std::function<bool()>contacts;std::function<void()>before_solve,after_solve;void solveConstraints(btContactSolverInfo&info)override{if(before_solve)before_solve();btDiscreteDynamicsWorld::solveConstraints(info);if(after_solve)after_solve();}void performDiscreteCollisionDetection()override{btDiscreteDynamicsWorld::performDiscreteCollisionDetection();if(contacts&&contacts())btDiscreteDynamicsWorld::performDiscreteCollisionDetection();}};
class PropPhysics {struct Impl;std::unique_ptr<Impl>p;public:PropPhysics(btDiscreteDynamicsWorld&,const std::vector<PropDefinition>&);~PropPhysics();bool activate_contacts(const btCollisionObject*car,const btCollisionObject*driver);void update_rest(float dt);void set_contact_step(float dt);size_t released()const;bool is_prop(const btCollisionObject*)const;std::optional<size_t> index(const btCollisionObject*)const;float pending_bonus_impulse(const btCollisionObject*)const;std::optional<PropBonus> claim_bonus(const btCollisionObject*);int collision_sound(const btCollisionObject*)const;bool is_wood_log(const btCollisionObject*)const;Mat pose(size_t)const;Vec velocity(size_t)const;std::shared_ptr<const std::vector<PropPose>>snapshot()const;Json diagnostics()const;};
}
