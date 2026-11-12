#include "simulation.hpp"
#include <stdexcept>
#include <iostream>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Geometry road(bool wall,bool prop){Geometry g;g.materials.push_back({"road"});Mesh m;for(Vec v:std::vector<Vec>{{-100,0,-100},{-100,0,500},{100,0,500},{100,0,-100}})m.vertices.push_back({v});m.indices={0,1,2,0,2,3};g.meshes.push_back(m);if(wall){Mesh w;for(Vec v:std::vector<Vec>{{-100,0,35},{100,0,35},{100,10,35},{-100,10,35}})w.vertices.push_back({v});w.indices=m.indices;g.meshes.push_back(w);}if(prop){PropDefinition p;p.mass=20;p.bonus={1,160,.022f};p.half={1,.8f,.5f};p.inertia={10,10,10};p.transform=translate({0,.8f,20});g.props.push_back(p);}return g;}
VehicleConfig car(){VehicleConfig c;c.mass=1000;c.inertia={1100,1200,250};c.lo={-.65f,.1f,-1.5f};c.hi={.65f,.7f,1.5f};c.center_mass={0,.28f,0};c.engine_torque=300;c.ratios={3,2,1.5f,1};c.torque_curve={{950,1},{7000,1}};c.nitro_capacity=.25f;for(int i=0;i<4;i++){auto&w=c.wheels[i];w.mount={i%2?.75f:-.75f,.15f,i<2?1.15f:-1.15f};w.rest=.32f;w.min=.05f;w.max=.55f;w.radius=.305f;w.k=22000;w.bump=1500;w.rebound=1800;}return c;}
void drain(Simulation&s){for(int i=0;i<300;i++)s.step({});Input boost;boost.throttle=1;boost.nitro=true;for(int i=0;i<35;i++)s.step(boost);need(s.state().nitro<.001f,"test drains nitro before contact");}
int main(){try{
 auto two_props=road(false,true);auto second=two_props.props.front();second.transform=translate({20,.8f,20});two_props.props.push_back(second);
 Simulation prop(two_props,car(),translate({0,1,0}));drain(prop);Input gas;gas.throttle=1;bool hit=false;for(int i=0;i<900;i++){prop.step(gas);auto s=prop.state();if(s.props&&!s.props->empty()){hit=true;break;}}need(hit,"car hits and releases actual light prop");need(std::abs(prop.state().nitro-.022f)<.0001f,"light prop grants authored mayhem, not impulse-derived nitro");need(prop.state().prop_hits==1&&prop.state().prop_bonus==2&&prop.state().prop_bonus_cents_by_type[1]==160,"authored cash is accumulated in cents and rounded at payout");
 auto before_recovery=prop.state().prop_hits;prop.recover();need(prop.state().prop_hits==before_recovery,"driver recovery does not reset the smash ledger");
 need(prop.recover(translate({20,1,0})),"second approach lane has a safe recovery");
 for(int i=0;i<900&&prop.state().prop_hits<2;i++)prop.step(gas);
 need(prop.state().prop_hits==2&&prop.state().prop_bonus==3&&prop.state().prop_bonus_cents_by_type[1]==320,"distinct instances pay separately and fractional dollars round once");need(prop.state().nitro<=1,"nitro clamps to capacity");
 auto gentle=car();gentle.engine_torque=30;Simulation wall(road(true,false),gentle,translate({0,1,0}));drain(wall);for(int i=0;i<1800&&wall.state().nitro==0;i++)wall.step(gas);need(wall.state().nitro>0,"solved wall collision refills nitro");need(!wall.state().driver_ejected,"wall pressure test keeps driver seated");float awarded=wall.state().nitro;for(int i=0;i<500;i++)wall.step(gas);need(wall.state().nitro<=awarded+.0001f,"continuous wall pressure cannot farm nitro");
 wall.recover();for(int i=0;i<2400&&wall.state().nitro<=awarded+.0001f;i++)wall.step(gas);need(wall.state().nitro>awarded+.01f,"a separated new wall collision earns another reward");
 Simulation full(road(false,true),car(),translate({0,1,0}));for(int i=0;i<300;i++)full.step({});for(int i=0;i<1000&&(!full.state().props||full.state().props->empty());i++)full.step(gas);need(full.state().props&&!full.state().props->empty(),"full tank test reaches actual prop contact");need(full.state().nitro==1&&full.state().prop_hits==1,"impact at full capacity still consumes the cash/nitro claim");
 auto disabled_road=road(false,true);Simulation disabled(disabled_road,car(),translate({0,1,0}));disabled.set_prop_rewards_enabled(false);drain(disabled);for(int i=0;i<1000&&(!disabled.state().props||disabled.state().props->empty());i++)disabled.step(gas);need(disabled.state().props&&!disabled.state().props->empty()&&disabled.state().prop_hits==0&&disabled.state().nitro==0,"coasting after finish still moves objects without earning rewards");
 auto rock_road=road(false,true);rock_road.props[0].bonus={9,0,.18f};Simulation rock(rock_road,car(),translate({0,1,0}));drain(rock);for(int i=0;i<1000&&!rock.state().prop_hits;i++)rock.step(gas);need(rock.state().prop_hits==1&&rock.state().prop_bonus==0&&std::abs(rock.state().nitro-.18f)<.0001f,"native nitro-only objects do not add money");
 Simulation ai_hit(road(false,true),car(),translate({20,1,0}));
 RaceOpponentConfig bot;bot.spawn=translate({0,1,0});bot.vehicle=car();RaceRoute route;route.half_width=1;
 for(int i=0;i<50;i++)route.points.push_back({0,0,float(i*5)});
 for(int i=49;i>=0;i--)route.points.push_back({50,0,float(i*5)});
 ai_hit.set_opponents({bot},route);
 for(int i=0;i<1200&&(!ai_hit.state().props||ai_hit.state().props->empty());i++)ai_hit.step({});
 need(ai_hit.state().props&&!ai_hit.state().props->empty(),"opponent reaches and moves the actual prop");
 need(ai_hit.state().prop_hits==0&&ai_hit.state().prop_bonus==0,"opponent collision cannot earn cash or claim the player reward");
 Simulation idle(road(false,false),car(),translate({0,1,0}));drain(idle);for(int i=0;i<1000;i++)idle.step({});need(idle.state().nitro<.001f,"ordinary tire support never refills nitro");
 auto arena=road(true,true);arena.source="tracks/derby/test";Simulation derby(arena,car(),translate({0,1,0}));for(int i=0;i<300;i++)derby.step({});auto start=derby.state();Input boost=gas;boost.nitro=true;derby.step(boost);need(!derby.state().nitro_active,"derby nitro remains disabled");for(int i=0;i<1000&&(!derby.state().props||derby.state().props->empty());i++)derby.step(boost);need(derby.state().props&&!derby.state().props->empty(),"derby collision actually occurs");need(derby.state().prop_hits==1&&derby.state().prop_bonus==2,"derby tracks cash without enabling nitro");need(derby.diagnostics()["collision_nitro"]["tracked_pairs"]==0,"derby contacts do not enter nitro reward tracking");
 {
  const auto config=car();
  const auto track=road(false,false);
  Simulation drive(track,config,translate({0,1,0}));
  for(int i=0;i<300;++i)drive.step({});
  drive.set_remote_colliders({{2,translate({0,.1f,15}),{},config.lo,config.hi,config.mass}});
  bool collided=false;
  for(int i=0;i<900&&!collided;++i){
   drive.step(gas);
   const auto state=drive.state();
   collided=state.collision_contacts&&!state.collision_contacts->empty();
  }
  need(collided&&drive.state().car[14]<15,"remote chassis participates in solved contact");
  drive.set_remote_colliders({});
  for(int i=0;i<1300&&drive.state().car[14]<19;++i)drive.step(gas);
  need(drive.state().car[14]>=19,"departed peer no longer blocks the track");
 }
 std::cout<<"collision nitro prop="<<prop.state().nitro<<" wall="<<awarded<<" passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
