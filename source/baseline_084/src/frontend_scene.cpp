#include "frontend.hpp"
#include "vehicle_dynamics.hpp"
#include "presentation_camera.hpp"
#include "editor_scene_query.hpp"
#include "legacy_ui/menu_camera.hpp"
#include "ui_menu_layout.hpp"
#include <algorithm>
namespace ofo {
std::string Frontend::selected_stage()const{
 if(!choice.stage.empty())return clean_path(choice.stage);
 auto path=clean_path(choice.track);auto at=path.find("/geometry/");return at==path.npos?path:path.substr(0,at)+"/a";
}
FrontendScene Frontend::showroom(double seconds)const{
 FrontendScene out;if(pause_context||page==MenuPage::CareerAward||page==MenuPage::SmashBonus||page==MenuPage::RaceSetup||page==MenuPage::AlphaTracks||page==MenuPage::TrackProjects||page==MenuPage::Credits||!assets)return out;
 if((page==MenuPage::LanLobby||page==MenuPage::LanRooms)&&assets->exists(lan_game_background))return out;
 try{
 if(showroom_points.is_null())showroom_points=parse_data(text_decode(assets->read("data/tracks/menu/menu1/a/data/startpoints.bed"))).at("Startpoints");const auto&points=showroom_points;
 if(showroom_cameras.is_null())showroom_cameras=parse_data(text_decode(assets->read("data/tracks/menu/menu1/a/data/cardealer_cameras.bed"))).at("CardealerCameras");const auto&cameras=showroom_cameras;
 bool static_dealer=page==MenuPage::CarSelect||page==MenuPage::Transmission||page==MenuPage::ClassSelect;
 bool garage=page==MenuPage::Garage||page==MenuPage::Stats,tuning=page==MenuPage::Tuning||page==MenuPage::Upgrade;
 const int id=selected_car_id();const bool alpha=id>16;
 out.slot=tuning?18:garage?19:(page==MenuPage::CarSelect||page==MenuPage::Transmission||page==MenuPage::ClassSelect)?(alpha?1:int(car_index+1)):20;
 auto profile=cameras.at("Default"),over=cameras.value(std::to_string(out.slot),Json::object());for(auto it=over.begin();it!=over.end();++it)profile[it.key()]=it.value();
 auto v3=[](const Json&j){return MenuCameraPoint{j.at("1").get<float>(),j.at("2").get<float>(),j.at("3").get<float>()};};
 std::array<MenuCameraPoint,3> knots{v3(profile.at("CameraOffset").at("1")),v3(profile.at("CameraOffset").at("2")),v3(profile.at("CameraOffset").at("3"))};
 auto offset=menu_camera_spline(knots,static_dealer?.5f:menu_camera_phase(animation_time>=0?animation_time:seconds)),target=v3(profile.at("TargetOffset"));
 auto slot=points.at(std::to_string(out.slot));Vec p=vector3(slot.at("Position")),x=vector3(slot.at("Orientation").at("x")),y=vector3(slot.at("Orientation").at("y")),z=vector3(slot.at("Orientation").at("z"));
 out.car_transform={x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,p.x,p.y,p.z,1};out.eye=p+x*offset[0]+y*offset[1]+z*offset[2];out.target=p+x*target[0]+y*target[1]+z*target[2];out.fov=profile.value("CameraFov",70.f);
 if(tuning){int markers[]={1,12,21,30,33,27};int marker=markers[std::clamp(tuning_category,0,5)];auto ids=category_upgrades();if(page==MenuPage::Upgrade&&!ids.empty()){auto&part=upgrades[ids[upgrade_index%ids.size()]];marker=part.dummy;out.fov=float(part.fov);}if(showroom_dummies.is_null())showroom_dummies=parse_data(text_decode(assets->read("data/tracks/menu/menu1/a/data/dummies.bed"))).at("Dummies");const auto&d=showroom_dummies;out.eye=p;out.target=vector3(d.at("tuning"+std::string(marker<10?"0":"")+std::to_string(marker)).at("Position"));}
 if(!static_dealer)out.eye=animated_menu_eye(out.eye,out.target,animation_time>=0?animation_time:seconds,tuning);
 // Original projection scales tan(fov/2) by .75 before aspect multiplication.
 out.fov=float(2*std::atan(std::tan(out.fov*3.141592653589793/360)*.75)*180/3.141592653589793);
 // Smoothly retarget camera position, focus and lens when moving between the
 // tuning categories and individual part dummies. Repeated input retains the
 // current interpolated pose; long gaps initialize without a stale fly-through.
 double now=animation_time>=0?animation_time:seconds;
 if(static_dealer||!camera_blend_valid||now<camera_blend_time||now-camera_blend_time>1){camera_blend_eye=out.eye;camera_blend_target=out.target;camera_blend_fov=out.fov;camera_blend_valid=true;}
 else {float a=float(1-std::exp(-std::clamp(now-camera_blend_time,0.,.1)*9));camera_blend_eye=camera_blend_eye+(out.eye-camera_blend_eye)*a;camera_blend_target=camera_blend_target+(out.target-camera_blend_target)*a;camera_blend_fov+=(out.fov-camera_blend_fov)*a;}
 camera_blend_time=now;out.eye=camera_blend_eye;out.target=camera_blend_target;out.fov=camera_blend_fov;
 // Dealer changes retain the old pose for the fade-out, then cut while black.
 if(static_dealer){
  if(showroom_car>=0&&showroom_car!=int(car_index)){showroom_transition=now;}
  showroom_car=int(car_index);
  if(showroom_transition>=0&&now-showroom_transition<.24){double age=std::max(0.,now-showroom_transition);out.fade_alpha=float(age<.12?age/.12:1-(age-.12)/.12);if(age<.12&&showroom_previous.enabled){auto previous=showroom_previous;previous.fade_alpha=out.fade_alpha;return previous;}}
 }else {showroom_car=-1;showroom_transition=-1;}
 out.enabled=true;out.show_car=!cars.empty()&&(garage||page==MenuPage::CarSelect||page==MenuPage::Transmission||page==MenuPage::ClassSelect);out.skin=choice.skin;out.car_geometry=alpha?"alpha_content/cars/car_"+std::to_string(id-16)+"/model.showcar":"menu/cars/menucar_"+std::to_string(id)+".bgm";out.title_smoke=out.slot==20;
 if(out.show_car){
  const auto key=std::make_pair(id,out.slot);
  auto found=showroom_lift.find(key);
  if(found==showroom_lift.end()){
   float lift=0;
   try{
    if(!showroom_ground)
     showroom_ground=std::make_shared<EditorSceneQuery>(load_geometry(*assets,out.geometry,out.config_base));
    const auto& floor=*showroom_ground;
    auto displayed=load_geometry(*assets,out.car_geometry);
    auto vehicle=load_vehicle(*assets,id,{});
    for(const auto&wheel:vehicle.wheels){
     Vec bottom=point(out.car_transform,suspension_mount(wheel)-Vec{0,wheel.radius,0});
     if(auto hit=floor.ray({bottom.x,p.y+.35f,bottom.z},{0,-1,0},.9f))
      lift=std::max(lift,hit->y-bottom.y);
    }
    // A few menu meshes bake in a tire offset beyond the dynamics radius.
    for(const auto&mesh:displayed.meshes){
     auto name=lower(mesh.name);
     if(name.find("tire")==name.npos&&name.find("wheel")==name.npos)continue;
     Vec bottom{0,INFINITY,0};
     for(const auto&vertex:mesh.vertices){
      auto v=point(out.car_transform,point(mesh.transform,vertex.position));
      if(v.y<bottom.y)bottom=v;
     }
     if(std::isfinite(bottom.y))
      if(auto hit=floor.ray({bottom.x,p.y+.35f,bottom.z},{0,-1,0},.9f))
       lift=std::max(lift,hit->y-bottom.y);
    }
   }catch(const std::exception&){}
   // The fitted prototype tires extend farther below the CARC origin.
   const float lift_limit=alpha?.30f:.22f;
   found=showroom_lift.emplace(key,std::clamp(lift,0.f,lift_limit)).first;
  }
  out.car_transform[13]+=found->second;
  out.target.y+=found->second;
 }
 showroom_previous=out;
 }catch(const std::exception&){}
 return out;
}
}
