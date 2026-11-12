#include "course_ui.hpp"
#include <algorithm>
#include <cmath>
namespace ofo {
namespace {
using V=std::array<double,3>;
V vec(const Json& j){ V v{}; for(int k=0;k<3;k++){if(j.is_array())v[k]=j.at(k).get<double>();else v[k]=j.value(std::to_string(k+1),j.value(std::string(1,"xyz"[k]),0.));}return v; }
std::vector<Json> numeric(const Json& j){std::vector<std::pair<int,Json>> a;if(j.is_object())for(auto it=j.begin();it!=j.end();++it)try{a.push_back({std::stoi(it.key()),it.value()});}catch(...){}std::sort(a.begin(),a.end(),[](auto&a,auto&b){return a.first<b.first;});std::vector<Json> o;for(auto&v:a)o.push_back(v.second);return o;}
double distance(V a,V b){double d=0;for(int k=0;k<3;k++)d+=(a[k]-b[k])*(a[k]-b[k]);return std::sqrt(d);}
}
Json race_course(const Resources&r,const std::string&base){
 Json out={{"points",Json::array()},{"available",false},{"scope","Prototype ordered route gates; original AI runtime not emulated"}};
 const auto prefix=ui_canonical(base)+(base.empty()||base.back()!='/'?"/":"");
 for(auto&p:r.paths())if(p.rfind(prefix,0)==0&&p.find("splines.ai")!=p.npos){try{
 auto d=parse_data(text_decode(r.read(p)));auto spl=d.value("Splines",Json::object());Json selected;
 for(auto it=spl.begin();it!=spl.end();++it)if(lower(it.key()).find("raceline")!=std::string::npos){selected=it.value();break;}
 if(selected.is_null())continue;std::vector<V> raw;for(auto&v:numeric(selected.value("ControlPoints",Json::object())))raw.push_back(vec(v));
 if(raw.size()<4)continue;if(distance(raw.front(),raw.back())<.01)raw.pop_back();
 // Cyclic Catmull-Rom uses the authored control points, never a bounding-box oval.
 for(size_t i=0;i<raw.size();i++)for(int s=0;s<8;s++){double t=s/8.,t2=t*t,t3=t2*t;V q{};auto a=raw[(i+raw.size()-1)%raw.size()],b=raw[i],c=raw[(i+1)%raw.size()],d=raw[(i+2)%raw.size()];for(int k=0;k<3;k++)q[k]=.5*((2*b[k])+(-a[k]+c[k])*t+(2*a[k]-5*b[k]+4*c[k]-d[k])*t2+(-a[k]+3*b[k]-3*c[k]+d[k])*t3);out["points"].push_back(q);}
 out["source"]=p;out["available"]=true;
 auto directory=p.substr(0,p.find_last_of('/')+1);
 auto split=directory+"splitpoints.bed";
 try{auto sd=parse_data(text_decode(r.read(split)));out["gates"]=Json::array();for(auto&v:numeric(sd.at("Splitpoints")))out["gates"].push_back({{"position",vec(v.at("Position"))},{"left",vec(v.at("Left"))},{"right",vec(v.at("Right"))}});out["gates_source"]=split;}catch(...){}
 // The original reset map is an 8-bit square mask, in rows from TopLeft to
 // BottomRight. Native tracks use both 0/1 and 0/255 palettes.
 try{
  auto meta=parse_data(text_decode(r.read(directory+"resetmap.bed")));
  auto xy=[](const Json&j){return j.is_array()?std::array<double,2>{j.at(0).get<double>(),j.at(1).get<double>()}:std::array<double,2>{j.at("1").get<double>(),j.at("2").get<double>()};};
  auto a=xy(meta.at("ResetMapTopLeft")),b=xy(meta.at("ResetMapBottomRight"));
  auto bytes=r.read(directory+"resetmap.raw");
  auto side=size_t(std::sqrt(double(bytes.size())));
  if(side>=2&&side<=2048&&side*side==bytes.size()&&std::isfinite(b[0]-a[0])&&std::isfinite(b[1]-a[1])&&std::abs(b[0]-a[0])>1&&std::abs(b[1]-a[1])>1&&std::any_of(bytes.begin(),bytes.end(),[](auto v){return v!=0;})){
   out["reset_map"]={{"side",side},{"origin",{a[0],a[1]}},{"span",{b[0]-a[0],b[1]-a[1]}},{"mask",Json::binary(std::move(bytes))}};
  }
 }catch(...){}
 return out;
 }catch(...){} }return out;
}
CourseDirector::CourseDirector(const Json&c,size_t cars,int laps,int bots):course_(c),runners_(cars),laps_(std::clamp(laps,1,9)),bots_(bots){for(auto&v:c.value("points",Json::array()))points_.push_back(vec(v));for(auto&v:c.value("gates",Json::array()))gates_.push_back(vec(v.at("position")));if(gates_.size()<2)gates_=points_;
 route_arc_.resize(points_.size()+1);for(size_t k=0;k<points_.size();++k)route_arc_[k+1]=route_arc_[k]+distance(points_[k],points_[(k+1)%points_.size()]);
 for(size_t gate_index=0;gate_index<gates_.size();++gate_index){auto g=gates_[gate_index],prior=gates_[(gate_index+gates_.size()-1)%gates_.size()];V n={g[0]-prior[0],0,g[2]-prior[2]};double width=25;
 if(course_.contains("gates")&&size_t(gate_index)<course_["gates"].size()){auto&sg=course_["gates"][gate_index];auto l=vec(sg["left"]),rr=vec(sg["right"]);n={-(rr[2]-l[2]),0,rr[0]-l[0]};V tangent={g[0]-prior[0],0,g[2]-prior[2]};double closest=1e30;
 for(size_t k=0;k<points_.size();k++){auto a=points_[k],b=points_[(k+1)%points_.size()];double dx=b[0]-a[0],dz=b[2]-a[2],len=dx*dx+dz*dz;double t=len>1e-9?std::clamp(((g[0]-a[0])*dx+(g[2]-a[2])*dz)/len,0.,1.):0;double d=std::hypot(a[0]+t*dx-g[0],a[2]+t*dz-g[2]);if(d<closest){closest=d;tangent={dx,0,dz};}}
 if(n[0]*tangent[0]+n[2]*tangent[2]<0){n[0]=-n[0];n[2]=-n[2];}width=distance(l,rr)*.5+2;}gate_normals_.push_back(n);gate_widths_.push_back(width);
 size_t closest_index=0;double error=1e300;for(size_t k=0;k<points_.size();++k){double d=distance(points_[k],gates_[gate_index]);if(d<error){error=d;closest_index=k;}}gate_route_.push_back(closest_index);
 }
 corridor_widths_.assign(points_.size(),16.);
 if(c.contains("gates")&&c["gates"].size()>=2&&!route_arc_.empty()&&route_arc_.back()>0){
  for(size_t i=0;i<points_.size();++i){
   double closest=1e300;
   for(size_t j=0;j<std::min(gate_route_.size(),c["gates"].size());++j){
    double gap=std::abs(route_arc_[i]-route_arc_[gate_route_[j]]);
    gap=std::min(gap,route_arc_.back()-gap);
    if(gap<closest){closest=gap;corridor_widths_[i]=std::clamp(gate_widths_[j],8.,24.);}
   }
  }
 }
 if(course_.contains("reset_map")){
  try{
   const auto&m=course_.at("reset_map");
   auto side=m.at("side").get<size_t>();
   auto a=m.at("origin").get<std::array<double,2>>(),b=m.at("span").get<std::array<double,2>>();
   const auto&bytes=m.at("mask").get_binary();
   if(side>=2&&side<=2048&&bytes.size()==side*side&&std::isfinite(a[0])&&std::isfinite(a[1])&&std::isfinite(b[0])&&std::isfinite(b[1])&&std::abs(b[0])>1&&std::abs(b[1])>1){
    reset_map_.assign(bytes.begin(),bytes.end());reset_map_side_=side;reset_map_origin_=a;reset_map_span_=b;
   }
  }catch(...){}
  course_.erase("reset_map");
 }
}
std::vector<CourseInput> CourseDirector::controls(const std::vector<CoursePose>&poses){
 std::vector<CourseInput> out(poses.size());for(size_t i=1;i<poses.size();i++){auto&o=out[i];if(!available()||runners_[i].finished||int(i)>bots_){o.brake=1;continue;}auto&m=poses[i].chassis;V pos={m[12],m[13],m[14]};auto&runner=runners_[i];size_t near=runner.nearest;double best=1e20;
 for(size_t k=0;k<points_.size();k++){if(runner.route_ready){size_t forward=(k+points_.size()-runner.nearest)%points_.size(),back=(runner.nearest+points_.size()-k)%points_.size();if(forward>10&&back>3)continue;}double d=distance(pos,points_[k]);if(d<best){best=d;near=k;}}runner.nearest=near;runner.route_ready=true;size_t k=near;double ahead=0;while(ahead<std::clamp(10+std::abs(poses[i].speed_kmh)*.2,10.,22.)){size_t n=(k+1)%points_.size();ahead+=distance(points_[k],points_[n]);k=n;if(k==near)break;}auto q=points_[k];double dx=q[0]-pos[0],dz=q[2]-pos[2],localx=dx*m[0]+dz*m[2],localz=dx*m[8]+dz*m[10];double angle=std::atan2(localx,localz);if(runner.reverse>0){o.throttle=-.75;o.steer=std::abs(angle)>.2?std::clamp(-angle,-1.,1.):(i%2?-.75:.75);continue;}o.steer=std::clamp(angle*1.7,-1.,1.);double target=std::clamp(48.-std::abs(angle)*25.,22.,48.);o.throttle=poses[i].speed_kmh<target?1.:.1;o.brake=poses[i].speed_kmh>target+5?.65:0.;}
 return out;
}
void CourseDirector::update(double dt,const std::vector<CoursePose>&poses){if(!available()||gates_.size()<2)return;for(size_t i=0;i<std::min(poses.size(),runners_.size());i++){auto&r=runners_[i];if(r.finished)continue;r.time+=std::max(0.,dt);if(i>0){double step=std::isfinite(dt)?std::clamp(dt,0.,.1):0;r.reverse=std::max(0.,r.reverse-step);r.ejected=poses[i].driver_ejected?r.ejected+step:0;if(std::abs(poses[i].speed_kmh)<3&&r.route_ready&&!poses[i].driver_ejected)r.stuck+=step;else r.stuck=0;if(r.stuck>2.5+.45*i){r.reverse=1.1+.2*i;r.stuck=0;}}V p={poses[i].chassis[12],poses[i].chassis[13],poses[i].chassis[14]};if(i==0){bool was_off=off_route();update_off_route(poses[i]);if(was_off||off_route()){r.previous=p;r.ready=true;continue;}}if(!r.ready){r.previous=p;r.ready=true;continue;}auto g=gates_[r.next];const auto&n=gate_normals_[r.next];double width=gate_widths_[r.next];
 double a=(r.previous[0]-g[0])*n[0]+(r.previous[2]-g[2])*n[2],b=(p[0]-g[0])*n[0]+(p[2]-g[2])*n[2];double t=(b-a)!=0?-a/(b-a):0;V hit={r.previous[0]+(p[0]-r.previous[0])*t,g[1],r.previous[2]+(p[2]-r.previous[2])*t};
 if(a<=0&&b>0&&distance(hit,g)<=width&&distance(p,r.previous)<20){r.next++;if(r.next>=int(gates_.size())){r.next=0;double lap=r.time-r.last;r.completed_lap=lap;r.best=r.best?std::min(r.best,lap):lap;r.last=r.time;r.lap++;r.finished=r.lap>laps_;}}r.previous=p;}}
void CourseDirector::reset(size_t i){if(i>=runners_.size())return;auto&r=runners_[i];r.next=0;r.ready=false;r.route_ready=false;r.stuck=r.reverse=r.ejected=0;r.last=r.time;if(i==0){last_on_route_.reset();departure_anchor_.reset();}}
std::optional<CourseDirector::Projection> CourseDirector::project_route(const CoursePose&pose) const {
 if(points_.size()<2)return std::nullopt;
 V position={pose.chassis[12],pose.chassis[13],pose.chassis[14]},nearest{},forward{};
 for(double v:position)if(!std::isfinite(v))return std::nullopt;
 double best=1e300,best_height=1e300,best_gate=1e300,best_arc=0,best_width=16;
 const bool ordered=!gates_.empty()&&!runners_.empty()&&gate_route_.size()==gates_.size();
 const size_t gate_index=ordered?gate_route_[size_t(runners_[0].next)%gates_.size()]:0;
 for(size_t i=0;i<points_.size();++i){const auto&a=points_[i];const auto&b=points_[(i+1)%points_.size()];
  V delta={b[0]-a[0],b[1]-a[1],b[2]-a[2]};
  // Native AIRaceLine Y contains editor handles, not terrain elevation.
  // Project in XZ; the physics world supplies the actual road height.
  double length=delta[0]*delta[0]+delta[2]*delta[2];
  if(!std::isfinite(length)||length<1e-10)continue;
  double t=std::clamp(((position[0]-a[0])*delta[0]+(position[2]-a[2])*delta[2])/length,0.,1.);
  V p={a[0]+t*delta[0],a[1]+t*delta[1],a[2]+t*delta[2]};
  double d=std::hypot(position[0]-p[0],position[2]-p[2]),height=std::abs(position[1]-p[1]);
  double to_gate=0;
  if(ordered){to_gate=route_arc_[gate_index]-(route_arc_[i]+t*(route_arc_[i+1]-route_arc_[i]));if(to_gate<0)to_gate+=route_arc_.back();}
  // At coincident branches prefer the route approaching the next ordered gate.
  // With no gates, height remains a tie-breaker for genuinely stacked routes.
  bool tie=std::abs(d-best)<=.05;
  if(d<best-.05||(tie&&(ordered?to_gate<best_gate:height<best_height))){best=d;best_height=height;best_gate=to_gate;nearest=p;forward=delta;best_arc=route_arc_[i]+t*(route_arc_[i+1]-route_arc_[i]);best_width=corridor_widths_[i]*(1-t)+corridor_widths_[(i+1)%points_.size()]*t;}
 }
 if(best==1e300)return std::nullopt;
 double length=std::hypot(forward[0],forward[2]),fx=forward[0]/length,fz=forward[2]/length;
 return Projection{{float(fz),0,float(-fx),0,0,1,0,0,float(fx),0,float(fz),0,float(nearest[0]),float(nearest[1]),float(nearest[2]),1},best,best_arc,best_width};
}
std::optional<std::array<float,16>> CourseDirector::recovery_transform(const CoursePose&pose) const {
 auto p=project_route(pose);
 if(!p)return std::nullopt;
 return p->transform;
}
bool CourseDirector::reset_map_contains(double x,double z) const {
 double u=(x-reset_map_origin_[0])/reset_map_span_[0],v=(z-reset_map_origin_[1])/reset_map_span_[1];
 if(!(u>=0&&u<1&&v>=0&&v<1))return false;
 const size_t col=std::min(size_t(u*reset_map_side_),reset_map_side_-1),row=std::min(size_t(v*reset_map_side_),reset_map_side_-1);
 return reset_map_[row*reset_map_side_+col]!=0;
}
void CourseDirector::update_off_route(const CoursePose&pose){
 const double x=pose.chassis[12],y=pose.chassis[13],z=pose.chassis[14];
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return;
 std::optional<Projection> p;
 if(!reset_map_side_)p=project_route(pose);
 const bool inside=reset_map_side_?reset_map_contains(x,z):p&&p->distance<=p->half_width;
 if(!departure_anchor_){
  if(inside){last_on_route_=pose;return;}
  auto anchor=last_on_route_?project_route(*last_on_route_):(p?p:project_route(pose));
  if(!anchor)return;
  // The line's Y stores editing handles; use actual chassis elevation as the
  // physics road probe hint, particularly on a stacked track.
  anchor->transform[13]=last_on_route_?last_on_route_->chassis[13]:float(y);
  if(last_on_route_&&reset_map_side_&&!reset_map_contains(anchor->transform[12],anchor->transform[14])){
   // A smoothed line can overshoot an authored edge. In that case the last
   // accepted chassis position is safer than the projected position.
   anchor->transform[12]=last_on_route_->chassis[12];
   anchor->transform[14]=last_on_route_->chassis[14];
  }
  departure_anchor_=anchor;departure_point_={x,z};
  return;
 }
 if(!inside)return;
 // Require re-entry near the recorded section; touching an adjacent route
 // branch after crossing the infield must not dismiss the warning.
 if(!p)p=project_route(pose);
 if(!p)return;
 double gap=std::abs(p->arc-departure_anchor_->arc);
 gap=std::min(gap,std::max(0.,route_arc_.back()-gap));
 const double limit=std::clamp(3.*departure_anchor_->half_width,24.,55.);
 if(gap>limit)return;
 if(reset_map_side_){
  const double sx=std::abs(reset_map_span_[0])/reset_map_side_*.75;
  const double sz=std::abs(reset_map_span_[1])/reset_map_side_*.75;
  if(!reset_map_contains(x-sx,z)||!reset_map_contains(x+sx,z)||!reset_map_contains(x,z-sz)||!reset_map_contains(x,z+sz))return;
 }else if(p->distance>p->half_width-2.)return;
 departure_anchor_.reset();last_on_route_=pose;
}
double CourseDirector::off_route_distance(const CoursePose&pose) const {
 if(!off_route())return -1.;
 const double distance=std::hypot(pose.chassis[12]-departure_point_[0],pose.chassis[14]-departure_point_[1]);
 return std::isfinite(distance)?distance:-1.;
}
std::optional<std::array<float,16>> CourseDirector::departure_transform() const {
 if(!off_route())return std::nullopt;
 return departure_anchor_->transform;
}
bool CourseDirector::recovery_candidate_valid(const std::array<float,16>&transform) const {
 if(reset_map_side_)return reset_map_contains(transform[12],transform[14]);
 CoursePose pose;pose.chassis=transform;
 auto p=project_route(pose);
 return p&&p->distance<=p->half_width-2.;
}
void CourseDirector::relocated(size_t i,const CoursePose&pose){
 if(i>=runners_.size())return;
 auto&r=runners_[i];r.previous={pose.chassis[12],pose.chassis[13],pose.chassis[14]};r.ready=true;r.route_ready=false;r.stuck=r.reverse=r.ejected=0;
 if(i==0){departure_anchor_.reset();last_on_route_=pose;}
}
CourseGuidance CourseDirector::guidance(size_t runner,const CoursePose&pose) const {
 CourseGuidance result;
 if(!available()||runner>=runners_.size()||runners_[runner].finished)return result;
 const auto&m=pose.chassis;V position={m[12],m[13],m[14]};
 for(double v:position)if(!std::isfinite(v))return result;
 if(!std::isfinite(pose.speed_kmh)||!std::isfinite(m[0])||!std::isfinite(m[2])||!std::isfinite(m[8])||!std::isfinite(m[10]))return result;
 // Project onto route segments rather than vertices. Ordered checkpoint progress
 // resolves coincident/crossing segments without steering toward a later gate.
 size_t segment=0;double fraction=0,best=1e300,best_gate=1e300;
 const auto&arc=route_arc_;size_t gate_index=gates_.empty()?0:gate_route_[size_t(runners_[runner].next)%gates_.size()];
 for(size_t i=0;i<points_.size();++i){const auto&a=points_[i];const auto&b=points_[(i+1)%points_.size()];V d={b[0]-a[0],b[1]-a[1],b[2]-a[2]};double length=0,dot=0;for(int k=0;k<3;++k){length+=d[k]*d[k];dot+=(position[k]-a[k])*d[k];}
  if(!std::isfinite(length)||length<1e-10||std::hypot(d[0],d[2])<1e-6)continue;
  double t=std::clamp(dot/length,0.,1.);V p{};for(int k=0;k<3;++k)p[k]=a[k]+d[k]*t;
  double error=distance(p,position),to_gate=arc[gate_index]-(arc[i]+t*std::sqrt(length));
  if(to_gate<0)to_gate+=arc.back();
  if(error<best-.05||(std::abs(error-best)<=.05&&to_gate<best_gate)){best=error;best_gate=to_gate;segment=i;fraction=t;}
 }
 if(best==1e300)return result;
 const auto&a=points_[segment];const auto&b=points_[(segment+1)%points_.size()];V target{};for(int k=0;k<3;++k)target[k]=a[k]+fraction*(b[k]-a[k]);
 double remaining=std::clamp(20.+std::abs(pose.speed_kmh)*.25,20.,40.);
 // Walk forward over the cyclic authored spline, including the lap seam.
 for(size_t n=0;n<=points_.size();++n){const auto&end=points_[(segment+1)%points_.size()];double length=distance(target,end);if(length>=remaining&&length>1e-9){for(int k=0;k<3;++k)target[k]+=(end[k]-target[k])*remaining/length;break;}remaining-=length;target=end;segment=(segment+1)%points_.size();if(remaining<=1e-9)break;}
 double dx=target[0]-position[0],dz=target[2]-position[2];double right=dx*m[0]+dz*m[2],forward=dx*m[8]+dz*m[10];
 if(std::hypot(right,forward)<1e-8)return result;
 result.available=true;result.target=target;result.distance=distance(position,target);result.yaw=std::atan2(right,forward);return result;
}
Json CourseDirector::leaderboard(const std::vector<CoursePose>&poses) const {
 auto table=hud();std::vector<size_t> order(runners_.size());for(size_t i=0;i<order.size();++i)order[i]=i;
 auto progress=[&](size_t i){return (runners_[i].lap-1)*int(gates_.size())+runners_[i].next;};
 auto to_gate=[&](size_t i){if(i>=poses.size()||gates_.empty())return 1e30;auto&m=poses[i].chassis;return distance({m[12],m[13],m[14]},gates_[size_t(runners_[i].next)%gates_.size()]);};
 std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){auto&A=runners_[a];auto&B=runners_[b];if(A.finished!=B.finished)return A.finished;if(A.finished)return A.time<B.time;if(progress(a)!=progress(b))return progress(a)>progress(b);return to_gate(a)<to_gate(b);});
 Json result=Json::array();for(size_t rank=0;rank<order.size();++rank){auto index=order[rank];auto row=table[index];row["car"]=index;row["rank"]=rank+1;row["checkpoint_progress"]=progress(index);result.push_back(std::move(row));}return result;
}
Json CourseDirector::hud()const{Json j=Json::array();for(auto&r:runners_)j.push_back({{"lap",std::min(r.lap,laps_)},{"laps",laps_},{"time",r.time},{"lap_time",r.finished?r.completed_lap:std::max(0.,r.time-r.last)},{"last_lap",r.completed_lap},{"best",r.best},{"finished",r.finished},{"next_gate",r.next}});return j;}
}
