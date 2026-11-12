#pragma once
#include "ui_ttf.hpp"
#include "openfo_bed_settings.hpp"
namespace ofo {
inline constexpr float alpha_card_top=147,alpha_card_width=230,alpha_card_height=132;
inline constexpr float alpha_card_left(size_t index){return 78.f+254.f*float(index);}
struct UiMenuRegion {
 float left,top,right,bottom;
 bool contains(double x,double y)const{return x>=left&&x<right&&y>=top&&y<bottom;}
};
// Original PC lanlobby.bed and gameinfo.bed coordinates on the 640x480 canvas.
inline constexpr const char* lan_game_background="data/menu/bg/game_background.tga";
inline constexpr int lan_room_page_size=9;
inline int lan_room_first(int selected,int room_count){
 return std::clamp(selected,0,std::max(0,room_count-1))/lan_room_page_size*lan_room_page_size;
}
inline constexpr UiMenuRegion lan_start_item(int index){return {57,159.f+26*index,583,183.f+26*index};}
inline constexpr UiMenuRegion lan_room_item(int index){return {0,107.f+24*index,245,131.f+24*index};}
inline constexpr UiMenuRegion lan_setting_item(int index,const LobbyLayout& layout){
 const float top=layout.settings_top+layout.settings_row_height*index;
 return {355,top,529,top+layout.settings_row_height};
}
inline constexpr UiMenuRegion lan_footer_item(int index){return {14.f+205*index,393,215.f+205*index,429};}
// The navigation strip uses a single rounded physical font size for metrics,
// drawing and mouse regions. Keeping this separate from general panel labels
// avoids independently fitting each neighbour as it crosses a viewport edge.
struct UiMenuLabel {
 std::string text;
 int pixels=18;
 float text_width=0,slot_width=0;
 float scale=1;
};
inline UiMenuLabel ui_menu_label(const std::string& text,float viewport_scale){
 const float k=std::max(.01f,viewport_scale);
 UiMenuLabel label;label.pixels=std::clamp(int(std::lround(18*k)),6,256);
 const float available=400*k;
 float measured=ui_text_width_pixels(label.pixels,true,text);
 if(measured>available)label.pixels=std::max(6,int(std::floor(label.pixels*available/measured)));
 label.scale=label.pixels/24.f;
 label.text=ui_fit_text(text,available,label.scale,true);
 label.text_width=ui_text_width_pixels(label.pixels,true,label.text)/k;
 label.slot_width=label.text_width+20;
 return label;
}
inline float ui_menu_edge_alpha(float center){
 // A continuous fade across the full strip, with neighbours disappearing at
 // the clipping boundary rather than popping away as complete labels.
 const float t=std::clamp(1.f-std::abs(center-320.f)/320.f,0.f,1.f);
 return t*t*(3-2*t);
}
}
