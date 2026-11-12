#include "captured_scene.hpp"
#include "content.hpp"
#include "course_ui.hpp"
#include "frame_clock.hpp"
#include "frontend_hud.hpp"
#include "race_battle.hpp"
#include "race_loading.hpp"
#include "recovery_flow.hpp"
#include "legacy_ui/race_ui.hpp"
#include <iostream>
#include <stdexcept>

using namespace ofo;
namespace {
void require(bool condition,const std::string& what){
 if(!condition)throw std::runtime_error(what);
}
void verify_launch_controls(){
 StuntSetup setup;
 setup.kind=StuntKind::Longjump;
 StuntSession event(setup);
 require(event.display().angle==0&&event.time_scale()==1,"first attempt starts at zero and full speed");
 Input input;
 event.aim(true,.01);
 event.aim(false,.01);
 event.launch_input(input);
 require(!input.eject,"holding gamepad A through the briefing cannot eject");
 event.aim(true,.01);
 require(event.display().angle==0&&event.display().charging,"a fresh aim begins at zero");
 FrameClock clock;
 int steps=0;
 for(int i=0;i<50;++i){
  event.aim(true,.01);
  steps+=clock.advance(.01,true,false,event.time_scale());
 }
 input.nitro=input.eject=true;
 event.launch_input(input);
 require(!input.eject&&!input.nitro&&event.display().charging,"charging suppresses boost and direct eject");
 require(steps==12&&std::abs(event.display().angle-35)<1e-6,"physics slows while the angle rises with real time");
 event.aim(false,.01);
 event.aim(false,.001);
 require(event.time_scale()==1,"release restores full simulation speed");
 event.launch_input(input);
 require(input.eject&&std::abs(input.stunt_angle-35)<1e-6,"release waits for a physics tick and uses the measured angle");
 event.launch_input(input);
 require(!input.eject,"release is one-shot");
 event.aim(true,.01);
 event.aim(true,.04);
 event.cancel_aim();
 event.aim(false,.01);
 event.launch_input(input);
 require(!input.eject&&!event.display().charging&&event.display().angle==0,"pausing a charge cannot launch on resume");
 event.aim(true,.01);
 event.aim(false,.01);
 event.launch_input(input);
 require(input.eject&&input.stunt_angle==0,"a tap launches at zero degrees");
 State state;
 state.driver_ejected=true;
 event.update(state,.01);
 state.stunt_grounded=true;
 event.update(state,.25);event.update(state,.25);
 require(event.next_attempt()&&event.display().angle==0&&event.time_scale()==1,"the next attempt starts at zero and full speed");
}
void verify_driver_rest_finish(){
 StuntSetup setup;setup.kind=StuntKind::Longjump;setup.forward={0,0,1};
 StuntSession event(setup);
 State state;state.driver_ejected=true;state.driver=translate({0,2,0});
 event.update(state,.01);
 state.driver=translate({0,15,20});
 state.driver_velocity={0,0,0};state.driver_acceleration={0,-9.81f,0};
 event.update(state,.1);
 require(!event.display().attempt_done,"velocity zero at flight apex is not final rest");
 state.driver=translate({0,0,12});state.stunt_grounded=true;
 state.driver_velocity={};state.driver_acceleration={};
 for(int i=0;i<52&&!event.display().attempt_done;i++)event.update(state,.01);
 require(event.display().attempt_done,"driver must finish after prolonged near-zero acceleration in contact");
 require(event.display().final_position_valid&&std::abs(event.display().final_position.z-12)<.001f,"stunt records driver final position");
 require(std::abs(event.display().score-12)<.001,"longjump uses final landing position, not maximum distance in flight");
}
void verify_configurable_aim(){
 StuntSession event(StuntSetup{},StuntAimSettings{.5,40});
 event.aim(false,.01);
 event.aim(true,.01);
 event.aim(true,.05);
 require(event.time_scale()==.5&&event.display().angle==2,"configured aim scale and angle rate drive the stunt session");
}
void verify_briefing(const Resources& r,const RaceLoadingPresentation& load,RaceMode mode,StuntKind kind){
 FrontendHud hud(r);
 for(auto lang:{UiLanguage::Ukrainian,UiLanguage::English}){
  RaceOverlayState overlay;
  overlay.event_intro=true;
  overlay.battle=mode==RaceMode::Battle;
  overlay.countdown=3;
  overlay.stunt.kind=kind;
  overlay.event_name=load.name;
  overlay.event_hint=event_intro_hint(r,mode,kind,lang);
  overlay.language=lang;
  const auto commands=hud.commands(1280,720,overlay);
  require(overlay.event_hint.size()>80,"localized briefing exists");
  require(std::any_of(commands.begin(),commands.end(),[&](const FrontendDraw& cmd){
   return cmd.kind==FrontendDraw::Text&&cmd.value==load.name;
  }),"briefing precedes countdown and shows selected event");
 }
}
void verify_battle(const Resources& r,const MenuTrack& slot,const RaceChoice& choice,const VehicleConfig& cfg){
 // The race loader has no checkpoint course for a last-man-standing battle.
 auto route=race_ai_route(Json{});
 route.derby=true;
 auto roster=load_race_opponents(r,choice.ai_profile_class,choice.ai_handicap_level,
                                 choice.ai_upgrade_level,choice.bots,slot.stage);
 require(roster.size()==size_t(choice.bots),slot.name+" loads its native starting grid");
 const auto track=load_geometry(r,stage_geometry(slot.stage),slot.stage);
 Simulation sim(track,cfg,load_spawn(r,slot.stage));
 for(int i=0;i<200;++i)sim.step({});
 sim.set_opponents(std::move(roster),std::move(route));
 RaceBattle battle(1+size_t(choice.bots));
 battle.update(.01,sim.state());
 require(!battle.finished()&&battle.survivors()==1+size_t(choice.bots),
         slot.name+" begins with the full field of cars alive");
 float max_bot_speed=0;
 for(int i=0;i<500&&!battle.finished();++i){
  sim.step({});
  const auto state=sim.state();
  for(const auto& bot:*state.opponents)max_bot_speed=std::max(max_bot_speed,bot.speed);
  battle.update(.01,state);
 }
 require(max_bot_speed>2.f,slot.name+" bots move without a circuit route");
}
void verify_stunt(const Resources& r,const Geometry& track,const MenuTrack& slot,const VehicleConfig& cfg,
                  const Geometry& body,std::shared_ptr<const NativeDriver> driver){
 const auto setup=load_stunt_setup(r,track,slot);
 Simulation sim(track,cfg,setup.spawn,load_vehicle_panels(r,body),driver,load_driver_placement(r,cfg.id,body,driver.get()));
 StuntSession event(setup);
 const double angle=setup.kind==StuntKind::Highjump?73:
                    setup.kind==StuntKind::Longjump?36:
                    setup.kind==StuntKind::Bullseye?22:
                    setup.kind==StuntKind::Clown?34:32;
 const int attempts=setup.kind==StuntKind::Bowling?3:1;
 for(int run=0;run<attempts;++run){
  sim.set_prop_rewards_enabled(false);
  for(int i=0;i<200;++i)sim.step({});
  const float lane=setup.spawn[12],sign=setup.forward.z;
  const auto generation=sim.state().recovery_generation;
  FrameClock clock;
  RecoveryFlow recovery;
  bool launched=false;
  for(int frame=0;frame<5100&&!event.display().attempt_done;++frame){
   const auto state=sim.state();
   const float heading=std::atan2(state.car[8]*sign,state.car[10]*sign);
   const double distance=(setup.launch_z-state.car[14])*sign;
   const bool aim=!launched&&event.display().angle<angle&&
    (event.display().charging||distance<=state.speed*angle/280.);
   event.aim(aim,.01);
   Input input;
   input.throttle=1;
   if(!launched)input.steer=std::clamp(-(state.car[12]-lane)*sign*.032f-heading*.68f,-.24f,.24f);
   const int steps=clock.advance(.01,true,false,event.time_scale());
   for(int i=0;i<steps&&!event.display().attempt_done;++i){
    event.launch_input(input);
    sim.step(input);
    const auto current=sim.state();
    require(!recovery.step(.01,current.driver_ejected,false,false)&&!recovery.active(),"stunt ejection never starts the return countdown");
    event.update(current,.01);
    launched=launched||current.driver_ejected;
   }
  }
  const auto result=event.display();
  std::cout<<slot.name<<" attempt "<<run+1<<" scored "<<result.score<<" after "<<result.elapsed<<" s"<<std::endl;
  require(launched&&result.attempt_done&&result.score>0,slot.name+" must score using the real car, driver and track");
  require(sim.state().driver_ejected&&sim.state().recovery_generation==generation,"the driver remains on scene until the player advances");
  if(run+1<attempts){
   require(event.next_attempt(),"another attempt starts");
   sim.reset_stunt_attempt(event.initial_props());
   const auto reset=sim.state();
   require(!reset.driver_ejected&&reset.damage==0&&(!reset.props||reset.props->empty()),"stunt reset restores the car and targets");
  }
 }
 if(attempts==3)require(event.display().finished&&event.display().award>0&&!event.next_attempt(),"three scoring attempts award the authored prize and end the event");
}
}

int main(int argc,char** argv){
 try{
  require(argc==2,"usage: test_event_modes078 retail-data");
  verify_launch_controls();verify_driver_rest_finish();verify_configurable_aim();
  if(std::string(argv[1])=="--controls")return 0;
  Resources resources(argv[1]);
  Frontend menu(resources);
  const auto cfg=load_vehicle(resources,3);
  const auto driver=std::make_shared<const NativeDriver>(load_native_driver(resources,driver_model_from_id(0)));
  const auto body=load_geometry(resources,cfg.body);
  std::map<std::string,Geometry> stunt_tracks;
  int derbies=0,arenas=0,stunts=0;
  for(size_t i=0;i<menu.track_slots.size();++i){
   const auto& slot=menu.track_slots[i];
   if(slot.menu_class!=4)continue;
   menu.select_track(i);
   const auto loading=race_loading_presentation(menu,slot.stage);
   require(loading.name==slot.name,"loading label must identify the event even on a shared stage");
   switch(menu.mode()){
   case RaceMode::Circuit:{
    require(arena_circuit(slot.stage)&&loading.laps==slot.laps,"arena is a circuit with its native lap count");
    auto route=race_course(resources,slot.stage);
    CourseDirector director(route,1,loading.laps,0);
    require(director.available(),"arena has native race checkpoints");
    verify_briefing(resources,loading,RaceMode::Circuit,StuntKind::None);
    ++arenas;
    break;
   }
   case RaceMode::Battle:
    require(slot.event=="DERBY"&&loading.laps==0&&menu.choice.bots>=1,"derby is a car battle without laps");
    verify_briefing(resources,loading,RaceMode::Battle,StuntKind::None);
    verify_battle(resources,slot,menu.choice,cfg);
    ++derbies;
    break;
   case RaceMode::Stunt:{
    require(loading.laps==0&&menu.choice.bots==0,"stunts are solo attempts");
    verify_briefing(resources,loading,RaceMode::Stunt,stunt_kind(slot.subevent));
    if(stunt_tracks.count(slot.stage)==0){
     auto track=load_geometry(resources,stage_geometry(slot.stage),slot.stage);
     append_captured_scene(track,resources);
     stunt_tracks.emplace(slot.stage,std::move(track));
    }
    verify_stunt(resources,stunt_tracks.at(slot.stage),slot,cfg,body,driver);
    ++stunts;
    break;
   }
   default:throw std::runtime_error("unexpected free-roam bonus event");
   }
  }
  require(derbies==3&&arenas==3&&stunts==6,"complete bonus catalog");
  menu.choice.time_trial=true;menu.selected_class=4;
  require(menu.bonus_class_visible()&&menu.class_tracks().size()==3,"time trial keeps only the three arena circuits");
  menu.page=MenuPage::Main;
  menu.row=0;
  const auto menu_commands=menu.draw_commands(1280,720,0);
  const MenuViewport viewport(1280,720);
  const auto label=ui_localize(menu.labels()[0],menu.language);
  require(std::any_of(menu_commands.begin(),menu_commands.end(),[&](const FrontendDraw& cmd){
   return cmd.kind==FrontendDraw::Text&&cmd.value==label&&std::abs(cmd.y-(viewport.map_y(404)-3))<.01;
  }),"main carousel text is exactly three pixels higher");
  require(std::none_of(menu_commands.begin(),menu_commands.end(),[&](const FrontendDraw& cmd){
   return cmd.kind==FrontendDraw::Rect&&cmd.y>=viewport.map_y(393)&&cmd.y<=viewport.map_y(429);
  }),"main carousel text has no selection plate");
 }catch(const std::exception& e){
  std::cerr<<e.what()<<'\n';
  return 1;
 }
}
