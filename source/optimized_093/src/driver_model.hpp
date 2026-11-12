#pragma once

namespace ofo {
// IDs are persisted as driver_gender in existing profile saves.
enum class DriverModel { Male=0, Female=1, JohnnyAlpha=2 };

constexpr DriverModel driver_model_from_id(int id) {
 return id==1?DriverModel::Female:id==2?DriverModel::JohnnyAlpha:DriverModel::Male;
}

constexpr int step_driver_model(int id,int direction) {
 return (int(driver_model_from_id(id))+(direction<0?2:1))%3;
}

constexpr const char* driver_model_label(DriverModel model) {
 switch(model){
 case DriverModel::Female:return "FEMALE";
 case DriverModel::JohnnyAlpha:return "JOHNNY (ALPHA)";
 default:return "MALE";
 }
}

constexpr const char* driver_model_portrait(DriverModel model,bool alpha_asset=true) {
 switch(model){
 case DriverModel::Female:return "data/menu/female.dds";
 case DriverModel::JohnnyAlpha:return alpha_asset?"alpha_content/driver/johnny_portrait.tga":"data/menu/male.dds";
 default:return "data/menu/male.dds";
 }
}
}
