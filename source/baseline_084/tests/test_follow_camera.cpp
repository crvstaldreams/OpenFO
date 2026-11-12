#include "follow_camera.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 Vec offset{0,2,-6},target{0,1,1};FollowCamera slow,fast;
 auto base=slow.update(identity(),offset,target,80,0,0,0,0);auto pace=fast.update(identity(),offset,target,80,40,1,0,0);
 need(length(pace.eye-base.eye)<1e-5f&&length(pace.target-base.target)<1e-5f&&pace.fov==base.fov,"speed and steering preserve authored follow composition");
 auto translated=fast.update(translate({0,0,1}),offset,target,80,80,-1,.01,0);need(length(translated.eye-Vec{0,2,-5})<1e-5f,"translation follows car without spring lag");
 FollowCamera turn;turn.update(identity(),offset,target,80,0,0,0,0);auto turning=turn.update(rotate_y(1.f),offset,target,80,0,0,.01,0);need(length(turning.eye-offset)>.001f&&length(turning.eye-point(rotate_y(1.f),offset))>.1f,"orientation follows smoothly");need(std::abs(length(Vec{turning.eye.x,0,turning.eye.z})-6)<1e-5f,"turning preserves authored boom length");
 FollowCamera a,b;a.update(identity(),offset,target,80,0,0,0,0);b.update(identity(),offset,target,80,0,0,0,0);CameraPose pa,pb;
 for(int i=1;i<=60;i++)pa=a.update(identity(),offset,target,80,20,.5f,i/60.,0);
 for(int i=1;i<=120;i++)pb=b.update(identity(),offset,target,80,20,.5f,i/120.,0);
 need(length(pa.eye-pb.eye)<.02f&&std::abs(pa.fov-pb.fov)<.01f,"spring consistent at 60 and 120 Hz");
 auto paused=a.update(translate({2,0,0}),offset,target,80,20,.5f,1,0);need(length(pa.eye-paused.eye)<1e-5f,"paused simulation freezes camera");
 auto recovered=a.update(translate({100,0,0}),offset,target,80,0,0,1.01,1);need(std::abs(recovered.eye.x-100)<1e-5f,"recovery resets spring immediately");
 auto photo=a.update(identity(),offset,target,80,0,0,1.02,1,true);need(length(photo.eye-base.eye)<1e-5f,"photo exit reset has no stale velocity");
 auto invalid=a.update(identity(),offset,target,80,0,0,-1,1);need(std::isfinite(invalid.eye.x),"backward clock stable");
 FollowCamera boost;auto idle=boost.update(identity(),offset,target,80,20,0,0,0);auto onset=boost.update(identity(),offset,target,80,80,0,.01,0,false,true);need(onset.fov>idle.fov&&onset.fov<81,"nitro begins smoothly rather than jumps");need(onset.eye.z<idle.eye.z,"nitro acceleration briefly pulls camera back");CameraPose full=onset;for(int i=2;i<=100;i++)full=boost.update(identity(),offset,target,80,80,0,i*.01,0,false,true);need(full.fov>87&&full.fov<=88,"active nitro has bounded FOV response");auto released=boost.update(identity(),offset,target,80,80,0,1.01,0);need(released.fov<full.fov&&released.fov>80,"nitro release fades smoothly");for(int i=102;i<=400;i++)released=boost.update(identity(),offset,target,80,200,0,i*.01,0);need(std::abs(released.fov-80)<.001f&&length(released.eye-offset)<.001f,"ordinary speed never changes authored camera after boost fades");auto recovery=boost.update(identity(),offset,target,80,80,0,4.01,1);need(recovery.fov==80&&length(recovery.eye-offset)<1e-5f,"recovery clears boost transient");
 NitroCameraEnvelope lens;need(lens.update(true,0,0)==0,"boost lens initializes without snap");auto lens_on=lens.update(true,.1,0);need(lens_on>0&&lens_on<1,"bumper/cockpit share gradual active lens envelope");need(lens.update(false,.1,0)==lens_on,"paused boost lens freezes");need(lens.update(true,.11,0,true)==0,"mode/photo transition clears lens transient");need(lens.update(true,.12,1)==0,"recovery clears all mode lens effects");
 std::cout<<"follow camera: fixed composition, smooth orientation, dt, pause, recovery and photo reset passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
