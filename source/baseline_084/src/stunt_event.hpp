#pragma once
#include "race_event.hpp"
#include "openfo_bed_settings.hpp"
#include "localization.hpp"
#include "simulation.hpp"
#include "legacy_ui/menu_assets.hpp"

namespace ofo {
struct StuntTarget {size_t prop=0;int value=0;Vec up{0,1,0};};
struct StuntSetup {
 StuntKind kind=StuntKind::None;
 Mat spawn=identity();
 Vec target{},forward{};
 std::vector<StuntTarget> objects;
 std::vector<PropDefinition> initial_props;
 std::array<double,3> thresholds{};
 std::array<int,3> prizes{};
 std::string name;
 float reference_height=0,launch_z=0;
};
struct StuntDisplay {
 StuntKind kind=StuntKind::None;
 std::string name;
 std::array<double,3> scores{},thresholds{};
 std::array<int,3> prizes{};
 int attempt=0,award=0,medal=0;
 double angle=0,score=0,total=0,elapsed=0;
 float launch_distance=0;
 bool charging=false,airborne=false,attempt_done=false,finished=false;
};

StuntSetup load_stunt_setup(const Resources&,const Geometry&,const MenuTrack&);
std::string event_intro_hint(const Resources&,RaceMode,StuntKind,UiLanguage);
const char* stunt_score_unit(StuntKind);

class StuntSession {
 StuntSetup setup_;
 StuntAimSettings aim_settings_;
 StuntDisplay display_;
 double elapsed_=0,flight_=0,quiet_=0;
 bool armed_=false,launch_pending_=false,scored_contact_=false;
 Vec start_{},previous_{};
 std::vector<size_t> hits_;
 void settle();
public:
 explicit StuntSession(StuntSetup,StuntAimSettings={});
 const StuntDisplay& display()const{return display_;}
 const std::vector<PropDefinition>& initial_props()const{return setup_.initial_props;}
 bool next_attempt();
 void aim(bool nitro,double wall_seconds);
 void cancel_aim();
 double time_scale()const{return display_.charging?aim_settings_.time_scale:1.;}
 void launch_input(Input&);
 void update(const State&,double dt);
};
}
