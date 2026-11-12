#include "headlight_damage.hpp"
#include <stdexcept>
static void need(bool b){if(!b)throw std::runtime_error("localized headlight damage regression");}
int main(){using namespace ofo;VehicleConfig c;c.lo={-1,0,-2};c.hi={1,1.4f,2};c.mass=1000;
 VehicleHeadlightDamage lights;VehicleDamageContact h;h.position={-.65f,.6f,2};h.normal={0,0,-1};h.closing_speed=8;h.impulse=5000;h.radius=.4f;
 lights.hit(c,h);need(lights.health()[0]==0&&lights.health()[1]==1);
 VehicleHeadlightDamage rear;h.position.z=-2;h.normal.z=1;rear.hit(c,h);need(rear.health()[0]==1&&rear.health()[1]==1);
 VehicleHeadlightDamage soft;h.position.z=2;h.normal.z=-1;h.closing_speed=.1f;soft.hit(c,h);need(soft.health()[0]==1&&soft.health()[1]==1);
 VehicleHeadlightDamage support;h.closing_speed=8;h.normal={0,1,0};support.hit(c,h);need(support.health()[0]==1&&support.health()[1]==1);
 h.normal={0,0,-1};h.position={0,.6f,2};h.radius=1.2f;support.hit(c,h);need(support.health()[0]==0&&support.health()[1]==0);
 // Damage persists across copies/replay and is not replenished by a later weak hit.
 auto snapshot=lights.health();h.impulse=1;lights.hit(c,h);need(lights.health()==snapshot);
}
