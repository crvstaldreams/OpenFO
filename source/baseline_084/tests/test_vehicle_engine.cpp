#include "vehicle_engine.hpp"
#include "simulation.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 VehicleConfig c;c.mass=1000;c.inertia={1100,1200,250};c.lo={-.65f,.1f,-1.5f};c.hi={.65f,.7f,1.5f};c.center_mass={0,.28f,0};c.ratios={3,2,1.5f,1};c.torque_curve={{500,.2f},{950,.6f},{3500,1},{5500,.8f},{7000,0}};
 for(int i=0;i<4;i++){auto&w=c.wheels[i];w.mount={i%2?.75f:-.75f,.15f,i<2?1.15f:-1.15f};w.k=22000;w.bump=1500;w.rebound=1800;}
 std::array<float,4>w{};VehicleEngine e;e.reset(c);
 for(int i=0;i<300;i++)e.step(c,w,0,0,0,0,true,0,0,.01f);need(std::abs(e.rpm-c.idle_rpm)<1,"stationary idle stable");need(e.transmitted==0,"idle does not propel parked car");
 for(int i=0;i<500;i++)e.step(c,w,0,1,0,0,false,0,1,.01f);std::cout<<"clutch free rpm="<<e.rpm<<'\n';need(e.rpm>c.idle_rpm*3&&e.rpm<c.max_rpm*1.08f+1,"clutch free rev reaches curve redline independently of wheels");need(e.transmitted==0,"released clutch transmits no torque");
 for(int i=0;i<500;i++)e.step(c,w,0,0,0,0,false,0,1,.01f);need(e.rpm<c.idle_rpm*1.5f,"free rotor falls toward idle under native friction");
 e.reset(c);float pull=e.step(c,w,0,1,0,0,true,0,0,.01f);need(pull>0,"idle launch produces wheel torque");
 e.reset(c);e.step(c,w,12,-1,0,0,true,0,0,.01f);need(e.gear==1&&e.direction_brake>0&&e.transmitted==0,"reverse request at speed brakes without reverse torque");e.step(c,w,0,-1,0,0,true,0,0,.01f);need(e.gear==-1,"reverse engages at standstill");float reverse=0;for(int i=0;i<100;i++)reverse=e.step(c,w,0,-1,0,0,true,0,0,.01f);need(reverse<0,"native reverse ratio drives backward");
 e.reset(c);e.step(c,w,0,0,0,0,false,-1,0,.01f);need(e.gear==0,"manual downshift selects neutral");for(int i=0;i<100;i++)e.step(c,w,0,1,0,0,false,0,0,.01f);need(e.gear==0&&e.transmitted==0&&e.rpm>2000,"neutral permits free rev");

 for(int i=0;i<100;i++)e.step(c,w,0,1,0,0,true,0,0,.01f);need(e.gear>=1&&e.transmitted>0,"switching manual neutral to automatic restores forward propulsion");
 auto no_forward=c;no_forward.ratios.clear();VehicleEngine empty;empty.reset(no_forward);empty.step(no_forward,w,0,-1,0,0,true,0,0,.01f);float safe=empty.step(no_forward,w,0,1,0,0,true,0,0,.01f);need(empty.gear==0&&safe==0&&std::isfinite(empty.rpm),"missing forward ratios safely selects neutral after reverse");for(int i=0;i<100;i++)safe=empty.step(no_forward,w,0,1,0,0,true,0,0,.01f);need(empty.gear==0&&safe==0,"automatic cannot engage a nonexistent forward gear");

 e.reset(c);w.fill(180);for(int i=0;i<120;i++)e.step(c,w,50,1,0,0,true,0,0,.01f);need(e.gear>1,"automatic shifts respond to real loaded rotor rpm");need(std::isfinite(e.rpm)&&e.rpm<=c.max_rpm*1.08f+1,"wheel-driven overrun stays finite and bounded");
 e.reset(c);w.fill(0);e.step(c,w,0,0,0,0,false,1,0,.01f);for(int i=0;i<150;i++)e.step(c,w,0,0,0,0,false,1,0,.01f);need(e.gear==2,"held manual shift is edge-triggered");
 Geometry road;road.materials.push_back({"road"});Mesh floor;floor.vertices={{{-100,0,-100}},{{-100,0,500}},{{100,0,500}},{{100,0,-100}}};floor.indices={0,1,2,0,2,3};road.meshes.push_back(floor);Simulation sim(road,c,translate({0,1,0}));for(int i=0;i<300;i++)sim.step({});Input input;input.throttle=1;for(int i=0;i<500;i++)sim.step(input);float speed=sim.state().speed;need(speed>4,"engine drives physical car from rest");input={};input.clutch=1;for(int i=0;i<100;i++)sim.step(input);need(sim.state().speed>speed*.65f,"released clutch permits physical coasting");input.brake=1;for(int i=0;i<300;i++)sim.step(input);need(std::abs(sim.state().speed)<1,"physical brakes stop clutch-free car without reverse drive");input={};input.throttle=-1;for(int i=0;i<250;i++)sim.step(input);need(sim.state().gear==-1&&sim.state().speed<-.5f,"native reverse drives physical car backward after stop");
 if(argc>1){Resources r(argv[1]);for(int id=1;id<=16;id++){auto v=load_vehicle(r,id);auto n=std::to_string(id);auto raw=bed_file(r,"equipmnt/custom/engine_"+n+".bed").at("Car.Engine.Car"+n+"_1").at("Settings");auto gear=bed_file(r,"equipmnt/custom/gearbox_"+n+".bed").at("Car.Gearbox.Car"+n+"_1").at("Settings");need(v.torque_curve.size()==raw.at("TorqueCurveParams").get<size_t>(),"only authored active torque samples loaded");need(v.max_rpm==v.torque_curve.back().first,"redline follows active curve endpoint");need(v.reverse_ratio==gear.at("GearRRatio").at("2").get<float>(),"reverse ratio native");need(v.engine_inertia==raw.at("InertiaEngine").get<float>(),"engine rotor inertia native");need(r.exists(v.provenance.at("engine_audio").get<std::string>()),"per-car native engine audio exists");VehicleEngine rotor;rotor.reset(v);for(int i=0;i<300;i++)rotor.step(v,w,0,1,0,0,false,0,1,.01f);need(rotor.rpm>v.idle_rpm*2&&std::isfinite(rotor.rpm),"all native engines free rev");std::cout<<id<<" "<<v.name<<" mass="<<v.mass<<" torque="<<v.engine_torque<<" redline="<<v.max_rpm<<" reverse="<<v.reverse_ratio<<'\n';}}
 std::cout<<"native vehicle engine passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
