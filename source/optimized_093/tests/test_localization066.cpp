#include "frontend.hpp"
#include "frontend_hud.hpp"
#include "ui_ttf.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ofo;
static void need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char**argv){try{
 need(argc>1,"resource directory required");Resources r(argv[1]);auto root=fs::temp_directory_path()/"openfo_localization066";fs::remove_all(root);Frontend a(r,root);
 a.page=MenuPage::Main;auto draw=a.draw_commands(1280,720);bool translated=false;for(auto&d:draw)translated|=d.value=="ГОЛОВНЕ МЕНЮ";need(translated,"new installation defaults to Ukrainian");
 need(a.create_profile("Пілот"),"create Ukrainian profile");a.page=MenuPage::GameOptions;a.row=10;a.key(39);need(a.language==UiLanguage::English,"language selection switches to English");Frontend b(r,root);need(b.language==UiLanguage::English,"English persists after restart");need(b.create_profile("Другий"),"second profile");need(b.language==UiLanguage::Ukrainian,"new profile defaults to Ukrainian");need(a.language==UiLanguage::English,"instances have independent locale");need(b.activate_profile(a.profile_id)&&b.language==UiLanguage::English,"switch restores profile language");
 for(auto lang:{UiLanguage::English,UiLanguage::Ukrainian}){b.language=lang;for(auto page:{MenuPage::Main,MenuPage::GameOptions,MenuPage::Options,MenuPage::Controls,MenuPage::CarSelect,MenuPage::RaceSetup}){b.page=page;for(auto dims:{std::pair<int,int>{640,480},{1280,720},{1920,1080},{720,1280}}){auto commands=b.draw_commands(dims.first,dims.second);for(const auto&d:commands)if(d.kind==FrontendDraw::Text){
  need(std::abs(ui_text_width_pixels(ui_raster_pixels(d.scale,d.heading),d.heading,d.value)-d.w)<1.1f,"translated glyph width matches raster size");
  const float left=d.clip_left>=0?std::max(d.x,d.clip_left):d.x,right=d.clip_right>=0?std::min(d.x+d.w,d.clip_right):d.x+d.w;
  if((d.color>>24)&&right>left)need(left>=-.1&&d.y>=-.1&&right<=dims.first+.1&&d.y+d.h<=dims.second+.1,"visible translated text fits viewport");
 }}}}
 need(ui_localize("PEPPER",UiLanguage::Ukrainian)=="PEPPER","car proper name preserved");need(ui_localize("LAPS: 3",UiLanguage::Ukrainian)=="КОЛА: 3","dynamic numeric label translated");need(ui_localize("MAIN MENU",UiLanguage::English)=="MAIN MENU","English source retained");
 FrontendHud hud(r);RaceOverlayState state;state.language=UiLanguage::Ukrainian;auto commands=hud.commands(1280,720,state);bool total=false;for(auto&d:commands)total|=d.value=="ЗАГАЛЬНИЙ ЧАС";need(total,"HUD translates semantic labels");
 auto wrapped=ui_wrap_text("Налаштування української мови та відображення навколишнього середовища",150,1,false,8);need(wrapped.size()>1,"Cyrillic wraps");for(auto&line:wrapped)need(ui_text_width_pixels(18,false,line)<=150,"Cyrillic line fits");
 fs::remove_all(root);std::cout<<"localization066: defaults, profiles, persistence, locale isolation and layout passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
