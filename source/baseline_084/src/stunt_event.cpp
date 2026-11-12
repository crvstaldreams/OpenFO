#include "stunt_event.hpp"
#include "editor_scene_query.hpp"
#include "content.hpp"
#include <stdexcept>

namespace ofo {
namespace {
int dart_value(const std::string& type){
 if(type=="wood_dartboard_bullseye")return 50;
 if(type.rfind("wood_dartboard_",0)!=0)return 0;
 const auto suffix=type.substr(type.rfind('_')+1);
 if(suffix.empty()||suffix.find_first_not_of("0123456789")!=suffix.npos)return 0;
 const auto points=std::clamp(std::stoi(suffix),0,20);
 return points*(type.find("_double_")!=type.npos?2:type.find("_triple_")!=type.npos?3:1);
}
int clown_value(const std::string& name){
 // Target order in the native placement table corresponds to levels.bed winnings.
 constexpr std::pair<const char*,int> prizes[]={{"mouth",500},{"hat",500},{"nose",500},
  {"cup",1000},{"mug",3000},{"pocket",3000},{"flower",1000},{"candy",1000},{"trumpet",1000}};
 if(name.rfind("dyn_fence_clown",0)==0)
  for(auto [target,value]:prizes)if(name.find(target)!=name.npos)return value;
 return 0;
}
}

std::string event_intro_hint(const Resources&r,RaceMode mode,StuntKind kind,UiLanguage language){
 if(mode==RaceMode::Circuit)
  return language==UiLanguage::Ukrainian?
   "Пройди всі контрольні точки за порядком і заверши зазначену кількість кіл. Використовуй нітро на прямих. Червона дуга біля спідометра показує накопичені пошкодження.":
   "Pass every checkpoint in order and complete the required laps. Use nitro on the straights. The red arc beside the speedometer shows accumulated damage.";

 const char* key=nullptr;
 const char* ukrainian=nullptr;
 const char* english=nullptr;
 switch(kind){
 case StuntKind::Longjump:
  key="STUNTS_LONG";ukrainian="Розженися, утримуй нітро і відпусти його біля краю трампліна, щоб катапультувати водія. Тривалість натискання визначає кут. Дистанції трьох стрибків додаються.";break;
 case StuntKind::Highjump:
  key="STUNTS_HIGH";ukrainian="Розженися і катапультуй водія з трампліна якомога вище. Що довше тримаєш нітро перед відпусканням, то більший кут. Висоти трьох стрибків додаються.";break;
 case StuntKind::Darts:
  key="STUNTS_DARTS";ukrainian="Розженися і запусти водія в мішень, утримавши й відпустивши нітро. Подвійні та потрійні сектори множать очки. Очки трьох спроб додаються.";break;
 case StuntKind::Bowling:
  key="STUNTS_BOWLING";ukrainian="Розженися і запусти водія в доріжку, утримавши й відпустивши нітро. Збий якомога більше кеглів. Перед кожною спробою кеглі виставляються знову.";break;
 case StuntKind::Clown:
  key="STUNTS_CLOWN";ukrainian="Розженися і запусти водія в клоуна, утримавши й відпустивши нітро. Цілься в окремі мішені. Кожна має свою винагороду.";break;
 case StuntKind::Bullseye:
  key="STUNTS_BULLSEYE";ukrainian="Розженися і запусти водія в мішень, утримавши й відпустивши нітро. Що ближче до центра мішені торкнеться водій, то більше очок. Очки трьох спроб додаються.";break;
 default:
  ukrainian="Знищ усі інші автомобілі й залишся останнім у дербі. Червона дуга показує пошкодження. Під час зіткнення швидший автомобіль отримує десяту частину шкоди від удару.";
  english="Wreck every other car and be the last driver standing. The red arc shows damage. In a collision, the faster car takes one tenth of the impact damage.";break;
 }
 if(language==UiLanguage::Ukrainian)return ukrainian;
 if(key){
  const auto strings=menu_strings(r);
  if(auto found=strings.find(key);found!=strings.end())return found->second;
  return "Build speed, hold Nitro to set the launch angle and release it to eject the driver. Scores from three attempts are added together.";
 }
 return english;
}

StuntSetup load_stunt_setup(const Resources& r,const Geometry& track,const MenuTrack& slot){
 StuntSetup setup;
 setup.kind=stunt_kind(slot.subevent);
 setup.name=slot.name;
 setup.thresholds=slot.scores;
 setup.prizes=slot.prizes;
 // Release windows are measured on each original approach, before its barrier.
 int start=0;
 switch(setup.kind){
 case StuntKind::Longjump:
  start=1;setup.launch_z=3;
  setup.target=vector3(bed_file(r,slot.stage+"/data/splines.ai").at("Splines").at("AIRaceLine").at("ControlPoints").at("1"));
  break;
 case StuntKind::Highjump:start=2;setup.launch_z=-64;setup.target=point(track.markers.at("dummy_hijump_01"),{});break;
 case StuntKind::Bullseye:start=3;setup.launch_z=65;setup.target=point(track.markers.at("curling_center"),{});break;
 case StuntKind::Bowling:start=4;setup.launch_z=-64;break;
 case StuntKind::Darts:start=5;setup.launch_z=68;break;
 case StuntKind::Clown:start=6;setup.launch_z=-36;break;
 default:throw std::runtime_error("Unknown stunt event");
 }
 if(start>=4){
  std::map<std::string,size_t> bodies;
  for(const auto& mesh:track.meshes)
   if(mesh.prop_index>=0&&size_t(mesh.prop_index)<track.props.size())
    bodies.emplace(lower(mesh.name),size_t(mesh.prop_index));
  bool found_center=false;
  for(const auto& prop:track.native_props){
   const auto type=lower(prop.type),name=lower(prop.name);
   int value=0;
   bool center=false;
   switch(setup.kind){
   case StuntKind::Bowling:value=type.rfind("bowling_pin",0)==0?1:0;center=type=="bowling_pina";break;
   case StuntKind::Darts:value=dart_value(type);center=type=="wood_dartboard_bullseye";break;
   case StuntKind::Clown:value=clown_value(name);center=type=="clownnose";break;
   default:break;
   }
   if(!value)continue;
   auto body=bodies.find(name);
   if(body==bodies.end())continue;
   if(center){setup.target=point(prop.transform,prop.center);found_center=true;}
   setup.objects.push_back({body->second,value,unit(direction(prop.transform,{0,1,0}))});
  }
  if(!found_center||setup.objects.empty())throw std::runtime_error("Stunt target bodies are missing: "+slot.name);
 }
 const Mat native=load_spawn(r,slot.stage,start);
 auto position=point(native,{});
 position.x=setup.target.x;
 setup.forward={0,0,native[10]<0?-1.f:1.f};
 EditorSceneQuery ground(track);
 if(auto floor=ground.ray(position+Vec{0,12,0},{0,-1,0},24)){
  position.y=floor->y+.55f;
  setup.reference_height=floor->y;
 }else setup.reference_height=position.y;
 if(setup.kind==StuntKind::Highjump){
  // The native height signs start at 16 m, measured from the arena floor.
  setup.reference_height=setup.target.y-16.f;
 }
 setup.spawn=multiply(translate(position),rotate_y(setup.forward.z<0?3.14159265f:0.f));
 setup.initial_props=track.props;
 return setup;
}

const char* stunt_score_unit(StuntKind kind){
 if(kind==StuntKind::Longjump||kind==StuntKind::Highjump)return "m";
 if(kind==StuntKind::Bowling)return "pins";
 if(kind==StuntKind::Clown)return "$";
 return "pts";
}

StuntSession::StuntSession(StuntSetup setup,StuntAimSettings aim_settings)
 :setup_(std::move(setup)),aim_settings_(aim_settings){
 display_.kind=setup_.kind;
 display_.name=setup_.name;
 display_.thresholds=setup_.thresholds;
 display_.prizes=setup_.prizes;
 display_.launch_distance=(setup_.launch_z-setup_.spawn[14])*setup_.forward.z;
}
bool StuntSession::next_attempt(){
 if(!display_.attempt_done||display_.finished)return false;
 ++display_.attempt;
 display_.attempt_done=display_.airborne=display_.charging=false;
 display_.score=display_.elapsed=display_.angle=0;
 display_.launch_distance=(setup_.launch_z-setup_.spawn[14])*setup_.forward.z;
 elapsed_=flight_=quiet_=0;
 armed_=launch_pending_=scored_contact_=false;
 hits_.clear();
 return true;
}
void StuntSession::aim(bool nitro,double wall_seconds){
 if(display_.attempt_done||display_.airborne||launch_pending_)return;
 if(!armed_){
  armed_=!nitro;
  return;
 }
 if(nitro){
  if(!display_.charging)display_.angle=0;
  else if(std::isfinite(wall_seconds)&&wall_seconds>0)
   display_.angle=std::min(double(stunt_max_launch_angle),display_.angle+std::min(wall_seconds,.05)*aim_settings_.degrees_per_second);
  display_.charging=true;
 }else if(display_.charging){
  display_.charging=false;
  launch_pending_=true;
 }
}
void StuntSession::cancel_aim(){
 if(display_.charging||launch_pending_)display_.angle=0;
 display_.charging=armed_=launch_pending_=false;
}
void StuntSession::launch_input(Input& input){
 input.nitro=input.eject=false;
 if(launch_pending_){
  input.stunt_angle=float(display_.angle);
  input.eject=true;
  launch_pending_=false;
 }
}
void StuntSession::settle(){
 display_.charging=display_.airborne=false;
 launch_pending_=false;
 display_.attempt_done=true;
 display_.scores[size_t(display_.attempt)]=display_.score;
 display_.total=0;
 for(auto score:display_.scores)display_.total+=score;
 display_.finished=display_.attempt==2;
 if(!display_.finished)return;
 if(setup_.kind==StuntKind::Clown){
  display_.award=int(display_.total);
  display_.medal=display_.award>0?1:0;
  return;
 }
 for(size_t i=0;i<setup_.thresholds.size();++i)
  if(setup_.thresholds[i]>0&&display_.total>=setup_.thresholds[i]){
   display_.award=setup_.prizes[i];
   display_.medal=int(i)+1;
   break;
  }
}
void StuntSession::update(const State& state,double dt){
 if(display_.attempt_done)return;
 elapsed_+=dt;
 display_.elapsed=elapsed_;
 if(!state.driver_ejected){
  display_.launch_distance=(setup_.launch_z-state.car[14])*setup_.forward.z;
  if(elapsed_>=35)settle();
  return;
 }
 const auto position=point(state.driver,{});
 if(!display_.airborne){
  display_.airborne=true;
  start_=previous_=position;
 }
 flight_+=dt;
 const auto motion=length(position-previous_);
 previous_=position;
 quiet_=(motion<dt*.65)?quiet_+dt:0;
 if(setup_.kind==StuntKind::Highjump)
  display_.score=std::max(display_.score,double(position.y-setup_.reference_height));
 if(setup_.kind==StuntKind::Longjump)
  display_.score=std::max(display_.score,double(dot(position-start_,setup_.forward)));
 if((setup_.kind==StuntKind::Darts||setup_.kind==StuntKind::Clown)&&state.stunt_hits){
  int impact=0;
  for(auto index:*state.stunt_hits){
   if(std::find(hits_.begin(),hits_.end(),index)!=hits_.end())continue;
   const auto target=std::find_if(setup_.objects.begin(),setup_.objects.end(),[&](const StuntTarget& t){return t.prop==index;});
   if(target==setup_.objects.end())continue;
   hits_.push_back(index);
   if(setup_.kind==StuntKind::Clown)display_.score+=target->value;
   else impact=std::max(impact,target->value);
  }
  if(impact){display_.score=impact;settle();return;}
 }
 if(setup_.kind==StuntKind::Bowling&&state.props){
  for(const auto& target:setup_.objects){
   if(std::find(hits_.begin(),hits_.end(),target.prop)!=hits_.end())continue;
   const auto pose=std::find_if(state.props->begin(),state.props->end(),
    [&](const PropPose& p){return p.index==target.prop;});
   if(pose!=state.props->end()&&dot(unit(direction(pose->transform,{0,1,0})),target.up)<.65f){
    hits_.push_back(target.prop);
    ++display_.score;
   }
  }
 }
 const bool landed=flight_>.25&&state.stunt_grounded;
 if(landed&&!scored_contact_){scored_contact_=true;quiet_=0;}
 if((setup_.kind==StuntKind::Longjump||setup_.kind==StuntKind::Highjump)&&landed){settle();return;}
 if(setup_.kind==StuntKind::Bullseye&&landed){
  const auto offset=position-setup_.target;
  if(std::abs(offset.y)<2.5f){
   const double rings=std::floor(std::hypot(offset.x,offset.z)/1.5);
   display_.score=std::max(display_.score,100.-10.*rings);
  }
 }
 if(flight_>=14||(flight_>1&&quiet_>=.8))settle();
}
}
