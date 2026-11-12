#include "alpha_car.hpp"
#include "alpha_driver_seat.hpp"
#include "menu_steering.hpp"
#include "native_driver.hpp"
#include "vehicle_damage.hpp"
#include <stdexcept>
#include <iostream>
using namespace ofo;

void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}

void check_grip(const NativeDriver& driver,const DriverPose& pose,const Geometry& skin,
                Vec forward,int hand){
 const auto& bind=driver.hands[hand];
 const size_t lower=hand?10:8;
 need(length(point(pose.skin[lower],bind.wrist)-pose.wrist_target[hand])<.001f,
      "Forearm ends at the fitted wrist");
 need(length(point(pose.hand_skin[hand],bind.wrist)-pose.wrist_target[hand])<.001f,
      "Hand starts at the fitted wrist");
 float near=0,far=0,nearest=1;
 for(size_t i=0;i<skin.meshes.size();++i){
  for(size_t j=0;j<skin.meshes[i].vertices.size();++j){
   const auto& w=driver.weights[i][j];
   unsigned weight=0;
   for(size_t n=0;n<4;++n)if(w.bone[n]==lower)weight+=w.weight[n];
   const auto original=driver.geometry.meshes[i].vertices[j].position;
   if(weight<230||dot(original-bind.wrist,bind.along)<.035f)continue;
   const auto offset=skin.meshes[i].vertices[j].position-pose.hand_target[hand];
   near=std::min(near,dot(offset,forward));
   far=std::max(far,dot(offset,forward));
   nearest=std::min(nearest,length(offset));
  }
 }
 need(near<-.025f&&far>.025f&&nearest<.023f,"Skinned hand reaches around both sides of the rim");
}

int main(int argc,char**argv){try{
 Resources r(argc>1?argv[1]:"/tmp/ofo068-data");
 const bool alpha=argc>2;
 if(alpha)r.mount(argv[2],"alpha_content");
 const auto replacement=load_menu_steering(r);
 for(int model=0;model<(alpha?3:2);++model){
  const auto driver=load_native_driver(r,driver_model_from_id(model));
  auto skin=make_driver_geometry(driver);
  for(int id=1;id<=(alpha?20:16);++id){
   const auto cfg=load_vehicle(r,id);
   auto body=load_geometry(r,cfg.body);
   const auto placement=load_driver_placement(r,id,body,&driver);
   need(replacement.apply(body),"Menu rim is available on every car");
   const auto mount=steering_mount(body);
   need(mount.has_value(),"Replacement rim can be measured");
   const auto center=point(placement.steering,{0,0,placement.rim_z});
   need(length(point(mount->frame,{0,0,mount->rim_z})-center)<.006f,
        "Rim replacement retains the fitted wheel center");
   need(std::abs(mount->radius-placement.rim_radius)<.001f,"Replacement retains fitted wheel radius");
   const auto right=unit(direction(placement.steering,{1,0,0}));
   const auto forward=unit(direction(placement.steering,{0,0,1}));
   for(float steer:{-.85f,-.6f,-.2f,0.f,.2f,.6f,.85f}){
    VehiclePanelDefinition hinge;
    hinge.pivot=point(placement.steering,{});
    hinge.axis=forward;
    hinge.lower_limit=-10;
    hinge.upper_limit=10;
    const auto wheel=vehicle_panel_hinge(hinge,steering_wheel_angle(steer));
    const auto pose=sample_seated_driver_pose(driver,placement,steer);
    need(pose.hands_locked,"Both wrists reach their targets throughout steering");
    const auto top=point(placement.steering,{0,placement.rim_radius,placement.rim_z});
    need(steer==0||dot(point(wheel,top)-top,right)*direction(rotate_y(steer),{0,0,1}).x>0,
         "Steering wheel top follows physical road-wheel turn");
    deform_driver(driver,pose,skin);
    for(int h=0;h<2;++h){
     const auto local=placement.grip[h];
     need(std::abs(std::hypot(local.x,local.y)-mount->radius*.944f)<.001f,"Grip is on the rim tube centerline");
     need(length(point(wheel,point(placement.steering,local))-pose.hand_target[h])<1e-5f,
          "Grip follows the rendered wheel's steering transform");
     check_grip(driver,pose,skin,forward,h);
    }
   }
   const auto old_size=body.meshes.size(),old_triangles=body.triangles;
   need(add_alpha_driver_seat(r,body,placement.seat)==alpha_car_id(id),
        "The seat is added only to the four prototypes");
   if(!alpha_car_id(id))continue;
   need(body.meshes.size()==old_size+1&&body.triangles==old_triangles+152,"One complete bucket seat added");
   const auto& seat=body.meshes.back();
   const auto& mat=body.materials.at(seat.material);
   need(!seat.solid&&seat.vertices.size()==141,"Seat mesh is visual and preserves donor topology");
   need(r.texture(body.source,mat.texture[0])=="cars/shared/interior.dds","Seat uses the original interior atlas");
   need(std::abs(seat.center.x-placement.seat.x)<.01f,"Seat is centered under the driver");
   need(!add_alpha_driver_seat(r,body,placement.seat),"Adding a seat twice is harmless");
   if(model!=0)continue;
   auto showroom=load_geometry(r,alpha_car_directory(id)+"/model.showcar");
   add_alpha_showroom_seat(r,showroom);
   need(showroom.meshes.back().name==seat.name,"Alpha showroom uses the same seat mesh");
  }
 }
 std::cout<<"Steering direction, wrist and visible grip across the steering range, alpha race and showroom seats passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
