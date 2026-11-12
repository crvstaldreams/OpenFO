#include "simulation.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool v,const char*m){if(!v)throw std::runtime_error(m);}
Geometry floor(){Geometry g;g.materials.push_back({"road"});Mesh m;for(Vec p:std::vector<Vec>{{-100,0,-100},{100,0,-100},{100,0,100},{-100,0,100}})m.vertices.push_back({p});m.indices={0,2,1,0,3,2};g.meshes.push_back(m);return g;}
VehicleConfig synthetic(){VehicleConfig c;c.mass=1000;c.inertia={1000,1000,1000};c.lo={-.3f,0,-.5f};c.hi={.3f,.4f,.5f};c.ratios={3,2,1};c.torque_curve={{950,1},{7000,1}};for(int i=0;i<4;i++){auto&w=c.wheels[i];w.mount={i%2?.85f:-.85f,0,i<2?1.f:-1.f};w.radius=.3f;w.width=.4f;w.rest=.3f;w.min=.05f;w.max=.6f;}return c;}
void patch(Geometry&g,float x0,float x1,float y){Mesh m;m.vertices={{{x0,y,-5}},{{x1,y,-5}},{{x1,y,5}},{{x0,y,5}}};m.indices={0,2,1,0,3,2};g.meshes.push_back(m);}
void smooth_support(){
 auto c=synthetic();Geometry ramp;ramp.materials.push_back({"road"});
 Mesh m;m.vertices={{{-20,0,-5}},{{20,0,-5}},{{20,0,1}},{{-20,0,1}},{{20,.56f,5}},{{-20,.56f,5}}};m.indices={0,2,1,0,3,2,3,4,2,3,5,4};ramp.meshes.push_back(m);
 Simulation before(ramp,c,translate({0,.2f,-.02f})),after(ramp,c,translate({0,.2f,.02f}));before.step({});after.step({});
 auto a=before.state().wheel_contacts[1],b=after.state().wheel_contacts[1];std::cout<<"ramp seam normals "<<a.normal.z<<" "<<b.normal.z<<'\n';
 need(a.grounded&&b.grounded,"both sides of connected shallow ramp support wheel");
 need(a.normal.z<-.015f&&a.normal.z>-.12f&&b.normal.z<-.015f&&length(a.normal-b.normal)<.04f,"wheel footprint interpolates support across shallow connected ramp seam");
 Simulation distant(ramp,c,translate({0,.2f,-2}));distant.step({});need(std::abs(distant.state().wheel_contacts[1].normal.z)<.001f,"large triangles do not spread seam influence beyond tire footprint");
 // A wall shares the road edge but cannot become a shallow support normal.
 Mesh wall;wall.vertices={{{-20,0,1}},{{20,0,1}},{{20,3,1}},{{-20,3,1}}};wall.indices={0,2,1,0,3,2};ramp.meshes.push_back(wall);
 Simulation protected_wall(ramp,c,translate({0,.2f,-.02f}));protected_wall.step({});auto n=protected_wall.state().wheel_contacts[1].normal;
 need(std::abs(n.z-a.normal.z)<.01f,"vertical wall does not contaminate road support interpolation");
}
void portable(){smooth_support();auto c=synthetic();Geometry rail;rail.materials.push_back({"road"});patch(rail,-20,20,-10);patch(rail,.96f,1.02f,0);Simulation edge(rail,c,translate({0,.2f,0}));edge.step({});need(edge.state().wheel_load[1]>100,"tire width must support on narrow edge outside center rays");need(edge.state().wheel_load[0]==0,"opposite wheel must not hit distant edge or own chassis");
 Geometry gap;gap.materials.push_back({"road"});patch(gap,-20,20,-10);patch(gap,1.15f,1.3f,0);Simulation air(gap,c,translate({0,.2f,0}));air.step({});for(auto v:air.state().wheel_load)need(v==0,"no phantom support beyond sidewall");
 auto ground=floor();Simulation settled(ground,c,translate({0,.6f,0}));for(int k=0;k<400;k++)settled.step({});float load=0;for(auto v:settled.state().wheel_load)load+=v;need(load>6000&&load<14000,"spring support must carry weight rather than rigid wheels at full droop");need(settled.state().damage==0,"settling must not cause damage");
 PropDefinition prop;prop.id=1;prop.mass=20;prop.inertia={1,1,1};prop.half={.1f,.3f,.15f};prop.transform=translate({1.08f,.25f,1});ground.props.push_back(prop);Simulation side(ground,c,translate({0,.25f,0}));side.step({});need(side.diagnostics()["props"]["released"]==0,"stationary sidewall contact must not release held prop without an impulse");ground.props[0].transform=translate({1.04f,.25f,3});Simulation impact(ground,c,translate({0,.6f,0}));for(int i=0;i<100;i++)impact.step({});Input gas;gas.throttle=1;for(int i=0;i<400&&impact.diagnostics()["props"]["released"]==0;i++)impact.step(gas);need(impact.diagnostics()["props"]["released"]==1,"moving wheel impact outside chassis releases held prop");
 ground.props[0].transform=translate({2,.25f,1});Simulation untouched(ground,c,translate({0,.25f,0}));untouched.step({});need(untouched.diagnostics()["props"]["released"]==0,"wheel sweep must not release untouched props");
 std::cout<<"width/gap/self-filter/spring support/sidewall prop contacts passed\n";}
int main(int argc,char**argv){try{portable();if(argc<2)return 0;Resources r(argv[1]);for(int id=1;id<=16;id++){auto c=load_vehicle(r,id);Simulation s(floor(),c,translate({0,1,0}));// A stationary native hull must not alternate between floor and droop.
// This caught the large-triangle GJK sweep failure hidden by speed-only tests.
Simulation resting(floor(),c,translate({0,1,0}));std::array<double,4> sum{},squared{};int samples=0;float settled_damage=0;
for(int step=0;step<600;++step){resting.step({});if(step==399)settled_damage=resting.state().damage;if(step>=400){++samples;auto state=resting.state();for(int wheel=0;wheel<4;++wheel){double load=state.wheel_load[wheel];sum[wheel]+=load;squared[wheel]+=load*load;need(state.wheel_contacts[wheel].grounded,"native tire cannot miss a stationary flat road");}}}
for(int wheel=0;wheel<4;++wheel){double mean=sum[wheel]/samples,cv=std::sqrt(std::max(0.,squared[wheel]/samples-mean*mean))/std::max(1.,mean);need(mean>100&&cv<.15,"stationary native tire load must remain continuous on a large road triangle");}
need(resting.state().damage==settled_damage,"stationary flat-road support must not add damage after initial drop");
auto d=s.diagnostics();need(d.contains("wheel_collision"),"missing model-derived wheel collision diagnostics");need(d["wheel_collision"]["native_hulls"]==4,"four original tire mesh hulls required");need(d["chassis_collision"]["boxes"]==2,"original top and bottom chassis boxes required");float spacer=d.at("handling").at("suspension_spacer_m");Input turn;turn.steer=.7f;turn.throttle=.5f;for(int i=0;i<80;i++)s.step(turn);auto pose=s.state();for(int wheel=0;wheel<4;wheel++){auto expected=point(pose.car,c.wheels[wheel].mount+Vec{0,c.wheels[wheel].rest-pose.suspension[wheel]-spacer,0});need(length(point(pose.wheels[wheel],{})-expected)<1e-5f,"native wheel marker follows actual suspension displacement");need(length(point(pose.wheel_mounts[wheel],{})-expected)<1e-5f,"hub and wheel share suspension position");auto axis=unit(direction(pose.wheel_mounts[wheel],{1,0,0}));auto tire_axis=unit(direction(pose.wheels[wheel],{1,0,0}));need(length(axis-tire_axis)<1e-5f,"hub and spinning tire retain identical steering axle");}}std::cout<<"16 original car wheel collision profiles passed\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
