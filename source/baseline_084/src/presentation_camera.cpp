#include "presentation_camera.hpp"
namespace ofo {
namespace {
float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*t*(t*(t*6-15)+10);}
Vec mix(Vec a,Vec b,float t){return a*(1-t)+b*t;}
CameraPose blend(const CameraPose&a,const CameraPose&b,float t){return{mix(a.eye,b.eye,t),mix(a.target,b.target,t),a.fov+(b.fov-a.fov)*t};}
}
float camera_clearance(float fov,float aspect){return .18f+.1f*std::tan(std::clamp(fov,30.f,110.f)*3.14159265f/360)*std::sqrt(1+std::max(.1f,aspect)*std::max(.1f,aspect));}
namespace {
Vec local_point(const Mat&m,Vec p){p=p-point(m,{});return {dot(p,direction(m,{1,0,0})),dot(p,direction(m,{0,1,0})),dot(p,direction(m,{0,0,1}))};}
bool intersects(Vec a,Vec b,const CameraObstacle&o,float r){a=local_point(o.transform,a);b=local_point(o.transform,b);Vec d=b-a;float enter=0,leave=1;for(int k=0;k<3;k++){float lo=o.lo[k]-r,hi=o.hi[k]+r;if(std::abs(d[k])<1e-7f){if(a[k]<lo||a[k]>hi)return false;}else{float x=(lo-a[k])/d[k],y=(hi-a[k])/d[k];if(x>y)std::swap(x,y);enter=std::max(enter,x);leave=std::min(leave,y);if(enter>leave)return false;}}return true;}
}
bool camera_segment_clear(Vec a,Vec b,const std::vector<CameraObstacle>& obstacles,float clearance){for(const auto&o:obstacles)if(intersects(a,b,o,clearance))return false;return true;}
CameraPose camera_safe_pose(CameraPose pose,const std::vector<CameraObstacle>& obstacles,float aspect){
 const float radius=camera_clearance(pose.fov,aspect);
 auto clear=[&](Vec eye){for(const auto&o:obstacles)if(intersects(eye,eye,o,radius))return false;return true;};
 // The target is a composition anchor, often below the chassis in the native
 // template (target.y=0, chassis.lo.y>0). A target-to-eye sweep therefore starts
 // inside the expanded subject, even though the lens is already clear. It is
 // not a valid camera-motion sweep and must not shorten the boom into the car.
 if(clear(pose.eye))return pose;
 const Vec desired=pose.eye;
 Vec back=desired-pose.target;back.y=0;back=unit(back,{0,0,-1});
 const Vec right=cross(Vec{0,1,0},back);
 Vec best=desired;float best_cost=1e30f;
 // Resolve only cars around the lens. Prefer the closest clear eye, preserving
 // the authored target, horizon and rearward boom. The bounded search cannot
 // turn a neighboring car into a multi-storey overhead camera.
 for(int rear=0;rear<=8;++rear)for(int side=-8;side<=8;++side){
  Vec eye=desired+back*(rear*.25f)+right*(side*.25f);
  for(size_t attempt=0;attempt<=obstacles.size();++attempt){
   float raised=eye.y;
   for(const auto&o:obstacles)if(intersects(eye,eye,o,radius)){
    float roof=-1e30f;
    for(int c=0;c<8;++c)roof=std::max(roof,point(o.transform,{c&1?o.hi.x:o.lo.x,c&2?o.hi.y:o.lo.y,c&4?o.hi.z:o.lo.z}).y);
    raised=std::max(raised,roof+radius+.02f);
   }
   if(raised==eye.y)break;
   eye.y=raised;
  }
  Vec view=eye-pose.target;float planar=length(Vec{view.x,0,view.z});
  if(eye.y>desired.y+1.5f||std::abs(view.y)>planar*.7f||!clear(eye))continue;
  Vec change=eye-desired;float cost=dot(change,change);
  if(cost<best_cost){best_cost=cost;best=eye;}
 }
 pose.eye=best;
 return pose;
}
Vec animated_menu_eye(Vec eye,Vec target,double clock,bool tuning){if(!std::isfinite(clock))return eye;float time=float(std::fmod(std::max(0.,clock),120.));Vec delta=eye-target;float angle=std::sin(time*.42f)*(tuning?.035f:.14f);delta=direction(rotate_y(angle),delta)*(1+(tuning?.004f:.018f)*std::sin(time*.31f));delta.y+=(tuning?.012f:.06f)*std::sin(time*.68f);return target+delta;}
CinematicCamera countdown_camera(double remaining,Vec lo,Vec hi,const CameraPose&gameplay,const Mat&car,const std::vector<CameraObstacle>& obstacles,float aspect){
 CinematicCamera out;out.pose=gameplay;if(!std::isfinite(remaining)||remaining<=0)return out;float elapsed=float(3-std::clamp(remaining,0.,3.));out.active=true;out.shot=std::min(2,int(elapsed));float t=smooth(elapsed-out.shot);
 for(int k=0;k<3;k++)if(!std::isfinite(lo[k])||!std::isfinite(hi[k])||hi[k]<=lo[k]){lo={-.9f,.1f,-2};hi={.9f,1.2f,2};break;}
 float side=lo.x-std::max(1.5f,(hi.x-lo.x)*.85f),mid=(lo.y+hi.y)*.5f,front=hi.z,rear=lo.z;
 auto pose=[&](Vec eye,Vec target,float fov){return CameraPose{point(car,eye),point(car,target),fov};};
 CameraPose a,b;
 if(out.shot==0){a=pose({side,mid+1.05f,front+2.7f},{0,mid,front*.45f},58);b=pose({side-.25f,mid+.85f,front+.65f},{0,mid,front*.25f},62);}
 else if(out.shot==1){a=pose({side-.3f,mid+.75f,front+.4f},{0,mid,.3f},56);b=pose({side-.3f,mid+.95f,rear-.4f},{0,mid,rear*.25f},62);}
 else{a=pose({side-.1f,mid+1.15f,rear-2.2f},{0,mid,0},66);b=gameplay;}
 out.pose=blend(a,b,t);
 if(!obstacles.empty()){
  // All lateral movement takes place above the whole grid. The final descent
  // is vertical at the collision-cleared gameplay eye, never a diagonal through cars.
  auto end=camera_safe_pose(gameplay,obstacles,aspect);
  float roof=end.eye.y;for(const auto&o:obstacles)for(int c=0;c<8;c++)roof=std::max(roof,point(o.transform,{c&1?o.hi.x:o.lo.x,c&2?o.hi.y:o.lo.y,c&4?o.hi.z:o.lo.z}).y);
  roof+=camera_clearance(std::max(80.f,gameplay.fov),aspect)+.8f;
  Vec p0=point(car,{side,0,front+2.7f}),p1=point(car,{side-.3f,0,front+.4f}),p2=point(car,{side-.3f,0,rear-2.2f});
  Vec e=elapsed<1?mix(p0,p1,smooth(elapsed)):elapsed<2?mix(p1,p2,smooth(elapsed-1)):mix(p2,end.eye,smooth((elapsed-2)/.65f));
  e.y=elapsed<2.65f?roof:roof+(end.eye.y-roof)*smooth((elapsed-2.65f)/.35f);
  out.pose.eye=e;out.pose.target=mix(point(car,{0,mid,0}),end.target,smooth((elapsed-2)/1.f));out.pose.fov=66+(end.fov-66)*smooth((elapsed-2)/1.f);
 }
 out.fade=1-smooth(elapsed/.18f);for(float cut:{1.f,2.f})out.fade=std::max(out.fade,1-smooth(std::abs(elapsed-cut)/.1f));out.letterbox=1-smooth((elapsed-2.55f)/.45f);return out;
}
float race_hud_opacity(double remaining,double elapsed){if(remaining>0||!std::isfinite(remaining)||!std::isfinite(elapsed))return 0;return smooth(float(std::max(0.,elapsed)/.35));}
}
