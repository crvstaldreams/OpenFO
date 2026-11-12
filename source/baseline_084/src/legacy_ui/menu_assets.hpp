#pragma once
#include "compat.hpp"
namespace ofo {
inline constexpr int alpha_car_class=5;
struct MenuArtwork { std::string texture; int atlas_width=0,atlas_height=0,x=0,y=0,width=0,height=0; explicit operator bool()const{return width>0&&height>0;} };
struct MenuCarStats { double horsepower=0,weight=0,torque=0; int price=0,speed=0,acceleration=0,strength=0; };
struct MenuSkin { int number=1; std::string texture,damaged_texture; std::array<std::array<float,3>,2> colors{}; bool has_colors=false; };
struct MenuCar { int number=0,car_class=0; std::string name,image; size_t scene_index=0; MenuArtwork icon,logo; MenuCarStats stats; std::vector<MenuSkin> skins; std::string description; };
struct MenuTrack { int level=0,menu_class=1,column=0,row=0,laps=3; float x=0,y=0; std::string name,track,stage,preview,icon,event,subevent,name_icon; std::array<int,3> prizes{}; std::array<double,3> scores{}; };
struct MenuUpgrade {std::string category,title,description,id,required,icon;int dummy=0;double fov=25;bool racing=false;std::array<int,3> prices{};Json effects;};
std::map<std::string,std::string> menu_strings(const Resources&);
std::vector<MenuUpgrade> menu_upgrades(const Resources&);
// Read the literal mapping/layout tables; this does not execute BED callbacks.
std::vector<MenuTrack> menu_track_catalog(const Resources&);
std::vector<MenuTrack> alpha_track_catalog(const Resources&);
// Replace only authored skin texture references; works on a mesh or complete scene.
bool apply_menu_skin(Json&,const Resources&,int car_number,int skin_number);
std::vector<MenuCar> menu_car_catalog(const Resources&);
MenuArtwork menu_artwork(const Resources&,const std::string& item);
MenuArtwork menu_artwork(const Resources&,const std::string& atlas,const std::string& item);
}
