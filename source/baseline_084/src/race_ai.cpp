#include "race_ai.hpp"
#include "legacy_ui/compat.hpp"
#include "vehicle_upgrades.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ofo {
namespace {
float value(const Json&j,const char*key,float fallback){auto it=j.find(key);return it!=j.end()&&it->is_number()&&std::isfinite(it->get<float>())?it->get<float>():fallback;}
float item(const Json&j,size_t i,float fallback){if(j.is_array())return i<j.size()&&j[i].is_number()?j[i].get<float>():fallback;auto key=std::to_string(i+1);return j.is_object()&&j.contains(key)&&j[key].is_number()?j[key].get<float>():fallback;}
Vec horizontal(Vec v){v.y=0;return v;}
Vec position(const Mat&m){return{m[12],m[13],m[14]};}
Vec jsonvec(const Json&j){if(j.is_array())return{item(j,0,0),item(j,1,0),item(j,2,0)};return vector3(j);}
Json data(const Resources&r,const std::string&p){return parse_data(text_decode(r.read(p)));}
Mat spawn_at(const Json&j){Mat m=identity();auto o=j.at("Orientation");auto x=jsonvec(o.at("x")),y=jsonvec(o.at("y")),z=jsonvec(o.at("z")),p=jsonvec(j.at("Position"));for(int k=0;k<3;k++){m[k]=x[k];m[4+k]=y[k];m[8+k]=z[k];m[12+k]=p[k];}return m;}
}
NativeAIBehavior decode_ai_behavior(const Json&p,const Json&h){NativeAIBehavior b;b.native=p;
 b.aggression=std::clamp(value(p,"Aggression",b.aggression),0.f,1.f);b.bump=std::clamp(value(p,"BumpAggression",b.bump),0.f,1.f);b.block=std::clamp(value(p,"BlockAggression",b.block),0.f,1.f);b.overtake=std::clamp(value(p,"OvertakeAggression",b.overtake),0.f,1.f);b.throttle=std::clamp(value(p,"ThrottleLimit",b.throttle),0.f,1.f);b.handicap=std::clamp(value(p,"HandicapRacing",b.handicap),0.f,1.f);b.catch_up=value(p,"CatchUpRacing",b.catch_up);
 b.look_min=std::clamp(value(p,"LookAheadMin",b.look_min),3.f,100.f);b.look_max=std::clamp(value(p,"LookAheadMax",b.look_max),b.look_min,150.f);b.look_modifier=std::clamp(value(p,"LookAheadModifier",b.look_modifier),.1f,4.f);b.brake_time=std::clamp(value(p,"NeutralBrakeTime",b.brake_time),.1f,3.f);b.brake_friction=std::clamp(value(p,"BrakeFrictionScale",b.brake_friction),.2f,3.f);b.friction_adjust=std::clamp(value(p,"FrictionAdjust",b.friction_adjust),0.f,2.f);
 if(h.contains("Racing")){auto&r=h.at("Racing");auto t=r.value("ThrottleMax",Json::object());b.throttle_min=std::clamp(item(t,0,b.throttle_min),0.f,1.f);b.throttle_max=std::clamp(item(t,1,b.throttle_max),b.throttle_min,1.f);b.throttle_reduction=std::clamp(value(r,"ThrottleReduction",b.throttle_reduction),0.f,1.f);b.turn_scale=std::clamp(value(r,"TurnScale",b.turn_scale),1.f,10.f);b.native["handicap_level"]=h;}return b;}
RaceRoute race_ai_route(const Json&course){RaceRoute r;
 // A battle has no checkpoint course. Its bots target other cars directly.
 if(course.is_null())return r;
 for(const auto&p:course.value("points",Json::array())){auto v=jsonvec(p);if(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&(r.points.empty()||length(v-r.points.back())>.01f))r.points.push_back(v);}if(r.points.size()>1&&length(r.points.front()-r.points.back())<.01f)r.points.pop_back();if(course.contains("gates")){float widths=0;size_t n=0;for(const auto&g:course["gates"])if(g.contains("left")&&g.contains("right")){widths+=length(jsonvec(g["left"])-jsonvec(g["right"]))* .5f;++n;}if(n)r.half_width=std::clamp(widths/n,3.f,20.f);}return r;}
std::vector<RaceOpponentConfig>load_race_opponents(const Resources&r,int cls,int handicap,int upgrades,int count,const std::string&stage){
 std::vector<RaceOpponentConfig>out;if(count<=0)return out;cls=std::clamp(cls,1,3);handicap=std::clamp(handicap,1,7);upgrades=std::clamp(upgrades,1,7);auto starts=data(r,stage+"/data/startpoints.bed").at("Startpoints");auto handicaps=data(r,"scripts/handicap.bed").at("Levels").at(std::to_string(handicap));auto ai_upgrades=data(r,"equipmnt/aiupgrades.bed").at("AIUpgrades").at(std::to_string(upgrades));auto equipment=data(r,"equipmnt/aiequipment.bed").at("AIEquipment");auto upgrade_list=data(r,"equipmnt/upgradelist.bed").at("UpgradeList");
 for(int i=1;i<=std::clamp(count,0,7);++i){RaceOpponentConfig c;c.id=i;auto path="drivers/aiprof"+std::to_string(i)+".ini";auto p=data(r,path).at("Profile").at(std::to_string(cls));c.car_id=p.at("CarNum");c.skin=p.value("Skin",1);const char*verified_names[]={"FRANK BENTON","SUE O'NEILL","KATIE DAGGERT","AI 4","RAY SMITH","AI 6","SETH BELLINGER"};c.name=p.value("Name",std::string(verified_names[i-1]));c.behavior=decode_ai_behavior(p,handicaps);c.vehicle=load_vehicle(r,c.car_id);auto slot=std::to_string(i+1);if(!starts.contains(slot))throw std::runtime_error("Native opponent grid position missing: "+slot);c.spawn=spawn_at(starts.at(slot));
  std::vector<std::string>parts;Json equip=Json::object();auto car=std::to_string(c.car_id);if(equipment.contains(car))for(const auto&section:{"Engine","Suspension","Gearbox","Tires"}){auto levels=equipment[car].value(section,Json::object());int level=std::clamp(int(item(levels,size_t(upgrades-1),1)),1,4);equip[section]=level;if(level>1&&upgrade_list.contains(car)&&upgrade_list[car].contains(section)){auto ids=upgrade_list[car][section];auto key=std::to_string(level);if(ids.contains(key)&&ids[key].is_string())parts.push_back(ids[key]);}}if(!parts.empty())apply_selected_upgrades(c.vehicle,r,parts);
  float torque=item(ai_upgrades.value("TorqueMax",Json::object()),size_t(i-1),0),turbo=item(ai_upgrades.value("TurboAcceleration",Json::object()),size_t(i-1),0);c.vehicle.engine_torque*=1+torque;c.vehicle.nitro_g+=turbo;c.provenance={{"profile",path},{"class",cls},{"handicap_level",handicap},{"upgrade_level",upgrades},{"equipment",equip},{"torque_adjust",torque},{"turbo_adjust",turbo},{"spawn_source",stage+"/data/startpoints.bed"},{"spawn_slot",i+1},{"name_source",p.contains("Name")?path:(i!=4&&i!=6?"original FO1 video roster, native car/character identity":"unresolved native executable name; AI profile number shown")}};c.vehicle.provenance["ai"]=c.provenance;out.push_back(std::move(c));
 }return out;
}
RaceAIController::RaceAIController(RaceRoute r,NativeAIBehavior b,size_t seed,const VehicleConfig& vehicle):route_(std::move(r)),behavior_(std::move(b)),vehicle_(vehicle),seed_(seed){}
AICommand RaceAIController::update(const AIVehiclePose&self,const std::vector<AIVehiclePose>&traffic,float dt){AICommand out;if(!std::isfinite(dt)||dt<=0)return out;dt=std::min(dt,.1f);
 if(route_.derby){
  auto pos=position(self.car);const AIVehiclePose*target=nullptr;float best=std::numeric_limits<float>::max();for(const auto&other:traffic){float d=length(horizontal(position(other.car)-pos));if(d>.05f&&other.damage<1&&d<best){best=d;target=&other;}}if(!target){out.brake=1;return out;}
  auto forward=unit(horizontal(direction(self.car,{0,0,1})),{0,0,1});auto right=unit(horizontal(direction(self.car,{1,0,0})),{1,0,0});auto delta=horizontal(position(target->car)+target->velocity*std::min(.5f,best/50)-pos);float angle=std::atan2(dot(delta,right),dot(delta,forward));float desired=std::clamp(angle*1.8f,-1.f,1.f);steer_+=std::clamp(desired-steer_,-dt*4,dt*4);out.steer=steer_;float speed=dot(self.velocity,forward);out.throttle=std::clamp(behavior_.throttle*(.65f+.35f*behavior_.aggression),0.f,1.f);if(std::abs(angle)>1&&speed>9){out.throttle=.1f;out.brake=.65f;}
  // A derby bot may turn toward a target behind it, but never select reverse.
  bool overturned=direction(self.car,{0,1,0}).y<.1f;
  if(length(horizontal(self.velocity))<1.2f||overturned)stuck_+=dt;else stuck_=0;
  if(stuck_>2.5f){out.throttle=.45f;out.brake=0;out.steer=std::abs(desired)>.1f?desired:(seed_%2?1.f:-1.f);}
  if(stuck_>6.f||(overturned&&stuck_>3.f)){auto t=unit(delta,forward);out.recover=true;out.recovery=multiply(translate(pos),rotate_y(std::atan2(t.x,t.z)));out.throttle=0;stuck_=0;steer_=0;}
  return out;
 }
 if(route_.points.size()<3){out.brake=1;return out;}const size_t n=route_.points.size();const auto&points=route_.points;auto pos=position(self.car);size_t near=nearest_;float best=std::numeric_limits<float>::max();
 // Once acquired, navigation advances only along the authored forward order.
 // Bounded arc search prevents global jumps to nearby sections and regression
 // onto the section behind. RaceDirector owns ordered gate/lap progress.
 auto consider=[&](size_t k){auto d=horizontal(points[k]-pos);float error=dot(d,d);if(error<best){best=error;near=k;}};
 if(!ready_){for(size_t k=0;k<n;k++)consider(k);ready_=true;}else{consider(nearest_);float arc=0;size_t k=nearest_;for(size_t step=0;step<n-1;++step){size_t next=(k+1)%n;arc+=length(horizontal(points[next]-points[k]));if(arc>35.f)break;consider(next);k=next;}}nearest_=near;
 // Measure lookahead in metres and interpolate the route. Selecting a whole
 // waypoint made response depend on route point spacing and cut tight corners.
 float speed=length(horizontal(self.velocity));auto forward=unit(horizontal(direction(self.car,{0,0,1})));auto right=unit(horizontal(direction(self.car,{1,0,0})),{1,0,0});float signed_speed=dot(self.velocity,forward);
 auto edge=horizontal(points[(near+1)%n]-points[near]);float edge_length=length(edge);float along=edge_length>.01f?std::clamp(dot(horizontal(pos-points[near]),edge)/(edge_length*edge_length),0.f,1.f):0;
 Vec anchor=points[near]+(points[(near+1)%n]-points[near])*along;
 auto sample=[&](float distance){size_t k=near;Vec start=anchor;for(size_t step=0;step<n;++step){auto end=points[(k+1)%n];float len=length(horizontal(end-start));if(len>=distance&&len>.001f)return start+(end-start)*(distance/len);distance-=len;start=end;k=(k+1)%n;}return start;};
 float look=std::clamp(behavior_.look_min+speed*behavior_.look_modifier*.45f,behavior_.look_min,std::max(behavior_.look_min,std::min(behavior_.look_max,32.f)));
 Vec aim=sample(look);auto tangent=unit(horizontal(sample(look+3)-sample(std::max(0.f,look-3))),forward);auto side=unit(cross({0,1,0},tangent),{1,0,0});float lane=0;
 float traffic_speed=std::numeric_limits<float>::max();
 for(const auto&other:traffic){auto delta=horizontal(position(other.car)-pos);float distance=length(delta);if(distance<.01f||distance>32||other.damage>=1)continue;float ahead=dot(delta,forward),lateral=dot(delta,right);float closing=signed_speed-dot(other.velocity,forward);
  if(ahead>0&&std::abs(lateral)<3.8f&&closing>-.5f){float threat=(1-ahead/32)*std::clamp((closing+3)/8.f,0.f,1.f);float pass_side=lateral>.35f?-1.f:lateral<-.35f?1.f:seed_%2?1.f:-1.f;lane+=pass_side*threat*(2.5f+behavior_.overtake*1.5f);
   // Only a collision corridor can constrain speed, and passing restores the
   // full pace immediately. Bump aggression permits small low-speed contacts.
   float clearance=2.6f-behavior_.bump*.3f;
   if(std::abs(lateral)<clearance&&ahead<6+std::max(0.f,closing)*.65f){float safe=std::max(3.f,dot(other.velocity,forward)+(ahead-3)*1.2f+behavior_.bump);traffic_speed=std::min(traffic_speed,safe);}
  }else if(ahead< -4&&ahead> -18&&std::abs(lateral)<4&&speed>8){lane+=std::clamp(lateral,-1.5f,1.5f)*behavior_.block*.4f;}
 }
 // Leave a full car width at the edge; rate limit blocking and overtakes so
 // rivals cannot instantly swerve into a car alongside them.
 float lane_bound=std::max(0.f,std::min(3.f,route_.half_width-2.2f));lane=std::clamp(lane,-lane_bound,lane_bound);lane_+=std::clamp(lane-lane_,-dt*1.7f,dt*1.7f);out.lane_offset=lane_;aim=aim+side*lane_;
 auto delta=horizontal(aim-pos);float distance=std::max(1.f,length(delta));float angle=std::atan2(dot(delta,right),dot(delta,forward));
 float wheelbase=std::max(1.8f,std::abs((vehicle_.wheels[0].mount.z+vehicle_.wheels[1].mount.z-vehicle_.wheels[2].mount.z-vehicle_.wheels[3].mount.z)*.5f));
 float steering_angle=std::max(.1f,vehicle_.steering_angle*(3.14159265f/180)*(1-vehicle_.steering_reduction*std::clamp(speed/std::max(1.f,vehicle_.steering_limit),0.f,1.f)));
 float desired=std::clamp(std::atan2(2*wheelbase*std::sin(angle),distance)/steering_angle,-1.f,1.f);steer_+=std::clamp(desired-steer_,-dt*4.f,dt*4.f);out.steer=steer_;
 // Use the same authored tire limits and brake torque as the physics world.
 // Keep a reserve for bumps, combined longitudinal force and imperfect grip.
 float surface_lat=1,surface_long=1;auto dirt=vehicle_.tire_surfaces.find(2);if(dirt!=vehicle_.tire_surfaces.end()){surface_lat=dirt->second.lateral;surface_long=dirt->second.longitudinal;}
 float lat_grip=std::clamp(vehicle_.mu*vehicle_.tire_dynamics.lateral*surface_lat*grip_scale_,.25f,2.5f);
 float long_grip=std::clamp(vehicle_.mu*vehicle_.tire_dynamics.longitudinal*surface_long*grip_scale_,.25f,2.5f);
 float radius=std::max(.2f,vehicle_.wheels[0].radius),mass=std::max(500.f,vehicle_.mass);
 float braking=std::max(1.f,std::min(long_grip*9.81f,vehicle_.brake_torque/(radius*mass))*.72f*std::clamp(behavior_.brake_friction,.65f,1.25f));
 float lateral_accel=lat_grip*9.81f*(.68f+.16f*behavior_.aggression);
 // The free-air power balance is only a straight-line ceiling. Actual
 // acceleration and automatic shifts still come from the shared drivetrain.
 float engine_power=vehicle_.engine_torque*vehicle_.max_rpm*.7f/9.5492966f;
 if(!vehicle_.torque_curve.empty()){engine_power=0;for(auto point:vehicle_.torque_curve)engine_power=std::max(engine_power,vehicle_.engine_torque*point.second*point.first/9.5492966f);}
 float target_speed=std::min(70.f,std::cbrt(std::max(1.f,engine_power*.9f)/std::max(.05f,vehicle_.drag.z)));
 if(!vehicle_.ratios.empty())target_speed=std::min(target_speed,vehicle_.max_rpm*.94f/9.5492966f*radius/(vehicle_.ratios.back()*vehicle_.final_ratio));
 // Look far enough to brake from current pace, including a response allowance.
 float horizon=std::clamp(speed*speed/(2*braking)+speed*.4f+25,behavior_.look_max,160.f);
 for(float d=0;d<horizon;d+=4){Vec a=sample(std::max(0.f,d-5)),b=sample(d+5),c=sample(d+15);Vec e1=horizontal(b-a),e2=horizontal(c-b);float l1=length(e1),l2=length(e2);if(l1<.5f||l2<.5f)continue;float turn=std::acos(std::clamp(dot(e1,e2)/(l1*l2),-1.f,1.f));float curvature=turn/((l1+l2)*.5f);if(curvature>.001f){float corner=std::sqrt(lateral_accel/curvature);float remaining=std::max(0.f,d-speed*.25f);target_speed=std::min(target_speed,std::sqrt(corner*corner+2*braking*remaining));}}
 // A displaced car must rejoin without applying a permanent bearing penalty
 // while correctly following a curved racing line.
 if(std::abs(angle)>.65f)target_speed=std::min(target_speed,std::max(5.f,12.f/(std::abs(angle)+.2f)));
 target_speed=std::max(4.f,std::min(target_speed,traffic_speed));out.target_speed=target_speed;
 float throttle_limit=behavior_.throttle*(behavior_.throttle_max+(behavior_.throttle_min-behavior_.throttle_max)*behavior_.handicap);
 // No player-distance throttle reduction: difficulty is the authored pedal
 // limit, and every opponent uses exactly the player's drivetrain and tires.
 float error=target_speed-signed_speed;
 out.throttle=error>-.3f?std::clamp(.32f+error*.34f,0.f,std::max(0.f,throttle_limit)):0;
 out.brake=error<-.8f?std::clamp((-error-.4f)/(braking*.65f),0.f,1.f):0;
 auto route_forward=unit(horizontal(points[(near+1)%n]-points[near]),forward);
 bool wrong_heading=dot(forward,route_forward)<-.25f;
 // Do not accelerate a spun car against the course direction. Align through
 // explicit recovery if steering cannot restore it promptly.
 misaligned_=wrong_heading?misaligned_+dt:0;
 if(wrong_heading){out.throttle=0;out.brake=1;}
 bool overturned=direction(self.car,{0,1,0}).y<.15f;bool offtrack=best>std::pow(route_.half_width+18,2);
 if(speed<1.2f||overturned||offtrack){stuck_+=dt;failed_+=dt;}else{stuck_=0;failed_=std::max(0.f,failed_-dt*2);}
 if(stuck_>2.5f&&!wrong_heading){out.throttle=std::min(.45f,std::max(0.f,throttle_limit));out.brake=0;out.steer=std::abs(desired)>.1f?desired:(seed_%2?1.f:-1.f);}
 if(failed_>6.f||(overturned&&failed_>3)||misaligned_>1.25f){auto p=points[near];auto t=unit(horizontal(points[(near+1)%n]-p),{0,0,1});p=p+unit(cross({0,1,0},t),{1,0,0})*(float(int(seed_%3)-1)*2.2f);out.recover=true;out.recovery=multiply(translate(p+Vec{0,.7f,0}),rotate_y(std::atan2(t.x,t.z)));failed_=stuck_=misaligned_=0;steer_=0;out.throttle=0;}
 return out;
}
}
