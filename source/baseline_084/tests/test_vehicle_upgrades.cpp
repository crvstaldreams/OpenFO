#include "vehicle_upgrades.hpp"
#include "legacy_ui/car_config.hpp"
#include "legacy_ui/menu_assets.hpp"
#include <iostream>
#include <stdexcept>
int main(int n,char**v){if(n<3)return 1;ofo::Resources r(v[1]);auto base=ofo::load_vehicle(r,3,v[2]);auto all=ofo::menu_upgrades(r);bool found=false;for(const auto&part:all){auto expected=ofo::car_configuration(r,"cars/car_3",{part.id});float torque=expected.value("ce.TorqueMax",base.engine_torque);if(torque<=base.engine_torque)continue;auto c=base;ofo::apply_selected_upgrades(c,r,{part.id});if(c.engine_torque!=torque||c.engine_torque<=base.engine_torque)throw std::runtime_error("selected part torque not applied");if(c.provenance.at("selected_upgrades").empty())throw std::runtime_error("upgrade provenance");if(c.provenance.at("upgrade_mass_delta")==0&&c.mass!=base.mass)throw std::runtime_error("menu mass must not replace physical measured mass");std::cout<<part.id<<" torque="<<c.engine_torque<<" physical_mass="<<c.mass<<'\n';found=true;break;}if(!found)throw std::runtime_error("native engine upgrade fixture missing");
 auto shocks=ofo::car_configuration(r,"cars/car_3",{"Upgrades.SHOCKS1"});auto upgraded=base;ofo::apply_selected_upgrades(upgraded,r,{"Upgrades.SHOCKS1"});
 auto near=[](float a,float b,float tolerance,const char*message){if(std::abs(a-b)>tolerance)throw std::runtime_error(message);};
 near(shocks.at("cs.FrontDefaultCompression"),.10f,.000001f,"SHOCKS1 recursively interpolates front compression");near(shocks.at("cs.RearDefaultCompression"),.115f,.000001f,"SHOCKS1 recursively interpolates rear compression");
 near(upgraded.wheels[0].k,17903.25f,.01f,"SHOCKS1 source compression changes runtime front spring");near(upgraded.wheels[0].rest,.31f,.000001f,"SHOCKS1 source rest length changes runtime front suspension");near(upgraded.wheels[2].rest,.325f,.000001f,"SHOCKS1 source rest length changes runtime rear suspension");
 near(upgraded.provenance.at("handling").at("rollbar").at(0),.125f,.000001f,"SHOCKS1 rollbar reaches runtime handling");
 auto stacked=ofo::car_configuration(r,"cars/car_3",{"Upgrades.SHOCKS1","Upgrades.SHOCKS2","Upgrades.SHOCKS3"});near(stacked.at("cs.FrontDefaultCompression"),.072f,.000001f,"nested installed effects accumulate before interpolation");
 std::cout<<"SHOCKS1 front compression="<<shocks.at("cs.FrontDefaultCompression")<<" spring="<<upgraded.wheels[0].k<<" rest="<<upgraded.wheels[0].rest<<'\n';}

