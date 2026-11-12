#include "frontend.hpp"
#include "frontend_hud.hpp"
#include "race_hud_map.hpp"
#include "legacy_ui/race_ui.hpp"
#include "legacy_ui/menu_balks.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 auto empty=fs::temp_directory_path()/"openfo_ui_layout_empty";fs::create_directories(empty);Resources r(argc>1?fs::path(argv[1]):empty);Frontend menu(r);FrontendHud hud(r);
 auto outline=race_hud_map(Json{{"available",true},{"points",{{-100,40,-60},{100,-20,-60},{100,10,60},{-100,0,60}}}});
 need(outline.width==256&&outline.height==256&&outline.top>outline.bottom,"native route map uses north-up UV coordinates");
 need(outline.pixels.front()==0&&outline.pixels[128*256+128]==0,"minimap background and loop interior are transparent");
 auto on_track=outline.pixels[64*256+128];
 need((on_track>>24)==255&&(on_track&0xffffff)==0xffffff,"authored route draws a solid white center line");
 need(!race_hud_map(Json{{"available",false}}).width,"unavailable routes must not create a bogus outline");
 auto edited=race_hud_map(Json{{"available",true},{"points",{{0,0,0},{120,0,0},{120,0,100}}}},false);
 need(edited.top<edited.bottom,"editor route preserves increasing world Z down-screen");
 for(auto size:{std::pair<int,int>{640,480},{1024,768},{1280,720},{1280,1024},{1920,1080},{2560,1080},{3840,2160},{720,1280}}){auto[w,h]=size;
  RaceOverlayState race;race.language=UiLanguage::English;race.elapsed=7.59;race.lap_time=4.41;race.position=2;race.countdown=0;race.show_positions=true;race.leaderboard.push_back({2,"PLAYER",1,3,30,false,true,""});
  auto live=hud.commands(w,h,race);
  auto label=[&](const std::string&v){return std::find_if(live.begin(),live.end(),[&](const FrontendDraw&d){return d.value==v;});};
  auto total=label("TOTAL TIME"),lap_time=label("LAP TIME"),position=label("2nd"),lap=label("1/3");
  need(total!=live.end()&&lap_time!=live.end()&&total->x<w*.15f&&lap_time->y>total->y,"time labels stack in the upper-left corner");
  need(position!=live.end()&&lap!=live.end()&&position->x>w*.75f&&lap->y>position->y,"ordinal position and lap stack in the upper-right corner");
  need(label("00'07''59")!=live.end()&&label("00'04''41")!=live.end(),"race timer shows centiseconds using native-style punctuation");
  need(label("PLAYER")==live.end()&&std::none_of(live.begin(),live.end(),[](const FrontendDraw&d){return d.kind==FrontendDraw::Rect;}),"ordinary race HUD omits dashboard panels and live standings");
  race.off_route_distance=47.7;
  auto warning=hud.commands(w,h,race);
  need(std::any_of(warning.begin(),warning.end(),[](const FrontendDraw&d){return d.value=="DISTANCE TO EXIT POINT: 48 m";}),"warning reports distance to the recorded departure point");
  race.off_route_distance=-1;
  for(auto&c:hud.commands(w,h,race)){need(c.x>=-.1f&&c.y>=-.1f,"HUD must remain inside narrow/portrait viewport");if(c.kind!=FrontendDraw::Text)need(c.x+c.w<=w+.1f&&c.y+c.h<=h+.1f,"HUD panels must fit viewport");}
  race.finished=true;race.results=true;race.prop_hits=7;race.prop_bonus=140;
  auto results=hud.commands(w,h,race);
  for(auto&c:results)if(c.kind==FrontendDraw::Rect)need(c.x>=0&&c.x+c.w<=w+.1f,"results fit narrow screen");
  need(std::any_of(results.begin(),results.end(),[](const FrontendDraw&d){return d.value=="OBJECT BONUS: $140  (7)";}),"results show money and qualifying object count");
  // Exercise native BED rectangles even on CI without copyrighted resources.
  const struct {const char* name;int y,height;} slices[]={{"upperbalk",193,63},{"lowerbalk",1,63},{"lowerbalk_ext",48,58}};
  for(auto slice:slices){auto art=menu_artwork(r,"balks",slice.name);if(!art)art={"data/menu/balks.dds",512,256,0,slice.y,512,slice.height};need(art.atlas_width==512&&art.atlas_height==256&&art.y==slice.y&&art.height==slice.height,"original BED slice mapping");menu.artwork_cache[std::string("balks/")+slice.name]=art;}
  MenuViewport view(w,h);const float k=float(view.width/640.);
  for(auto page:{MenuPage::Main,MenuPage::Start,MenuPage::CarSelect,MenuPage::RaceSetup,MenuPage::Pause}){
   menu.page=page;menu.pause_context=page==MenuPage::Pause;
   auto cmds=menu.draw_commands(w,h,0);bool top=false,bottom=false;float widths[3]={};
   for(const auto& c:cmds)if(c.kind==FrontendDraw::Image&&c.value.find("balks.dds")!=std::string::npos){
    need(c.x>=-.001f&&c.y>=-.001f&&c.x+c.w<=w+.01f&&c.y+c.h<=h+.01f,"balk tiles clipped to physical viewport");
    need(c.u>=0&&c.v>=0&&c.uw>0&&c.vh>0&&c.u+c.uw<=1.00001f&&c.v+c.vh<=1.00001f,"balk UVs within atlas");
    int band=c.y<63*k?0:c.y<view.map_y(437)-.1?1:2;
    const auto slice=slices[band];
    need(c.v>=slice.y/256.f-.0001f&&c.v+c.vh<=(slice.y+slice.height)/256.f+.0001f,"UV never samples adjacent atlas region");
    need(std::abs(c.w/c.uw-640*k)<.02f,"horizontal detail scale preserved across resolutions");
    need(std::abs(c.h/c.vh-256*k)<.02f,"vertical detail scale preserved across resolutions");
    widths[band]+=c.w;top|=c.y==0;bottom|=std::abs(c.y+c.h-h)<.1f;
   }
   need(top&&bottom,"native textured bands anchored to both screen edges");
   for(float covered:widths)need(std::abs(covered-w)<.1f,"each native band covers complete physical width");
  }
  menu.pause_context=false;menu.page=MenuPage::Start;
  menu.click(int(w*.5),int(h-75*std::min(w/640.,h/480.)),w,h);need(menu.page==MenuPage::Profile,"bottom anchored button hit matches drawing after resize");
 }
 menu.language=UiLanguage::English;
 for(auto page:{MenuPage::SinglePlayer,MenuPage::Options,MenuPage::GameOptions,MenuPage::Controls,MenuPage::Profile,
                MenuPage::CarSelect,MenuPage::RaceSetup,MenuPage::Tuning,MenuPage::Upgrade,MenuPage::AlphaTracks,
                MenuPage::Stats,MenuPage::TrackProjects,MenuPage::LanStart}){
  menu.page=page;menu.row=0;
  const auto commands=menu.draw_commands(640,480,0);
  need(std::none_of(commands.begin(),commands.end(),[](const FrontendDraw& d){
   return d.kind==FrontendDraw::Rect&&d.w>100&&d.h>8;
  }),"menu text has no filled plates or list panels");
 }
 menu.master_address.clear();menu.click(100,249,640,480);menu.lan_character('1');
 need(menu.page==MenuPage::LanStart&&menu.row==3&&menu.master_address=="1","master address mouse selection keeps the text field active");
 menu.lan=std::make_shared<LanSession>();
 LanPlayer local;local.name="A DRIVER IN THE ROOM";local.car_id=1;
 LanRules rules;rules.stage="tracks/forest/forest1/a";
 menu.lan->host(0,local,rules);menu.lan->set_ready(true);
 menu.page=MenuPage::LanLobby;menu.row=0;
 if(const auto art=menu_artwork(r,"menucommon","checked");art){
  const auto commands=menu.draw_commands(640,480,0);
  need(std::any_of(commands.begin(),commands.end(),[&](const FrontendDraw& d){
   return d.kind==FrontendDraw::Image&&d.value==art.texture&&d.x==11&&d.y==93&&d.u==float(art.x)/art.atlas_width&&d.v==float(art.y)/art.atlas_height;
  }),"ready driver uses the original checked atlas sprite");
 }
 for(auto size:{std::pair<int,int>{640,480},{1280,720},{720,1280}}){
  const auto [w,h]=size;
  const MenuViewport view(w,h);const float k=float(view.width/640);
  const auto commands=menu.draw_commands(w,h,0);
  const auto text_at=[&](const char* value,float x){
   return std::any_of(commands.begin(),commands.end(),[&](const FrontendDraw& d){
    return d.kind==FrontendDraw::Text&&d.value==value&&std::abs(d.x-(view.x+x*k))<.1f;
   });
  };
  need(text_at("READY",5)&&text_at("NAME",65)&&text_at("CAR",243)&&text_at("A DRIVER IN THE ROOM",65),"lobby uses the original READY, NAME and CAR roster columns");
  int separators=0;
  for(const auto& d:commands)if(d.kind==FrontendDraw::Rect&&d.w>=w-.1f&&std::abs(d.h-2*k)<.1f)++separators;
  need(separators==2,"lobby preserves both original gold separators");
  if(r.exists("data/menu/bg/game_background.tga")){
   need(commands.front().kind==FrontendDraw::Image&&commands.front().value=="data/menu/bg/game_background.tga","lobby draws its original PC background behind the frame");
  }
 }
 const auto default_lobby=menu.draw_commands(640,480,0);
 menu.bed_settings.lobby.settings_top=89;
 menu.bed_settings.lobby.settings_row_height=28;
 const auto adjusted=menu.draw_commands(640,480,0);
 auto laps_y=[](const auto& commands){
  const auto label=std::find_if(commands.begin(),commands.end(),[](const FrontendDraw& d){
   return d.kind==FrontendDraw::Text&&d.value=="LAPS"&&d.x==365;
  });
  need(label!=commands.end(),"lobby draws the laps label");
  return label->y;
 };
 need(std::abs(laps_y(default_lobby)-laps_y(adjusted)-16.f)<.01f,"lobby draws labels at the configured BED positions");
 const int old_laps=menu.lan->rules().laps;
 menu.click(521,154,640,480);
 need(menu.row==2&&menu.lan->rules().laps==old_laps+1,"visible lobby laps row accepts mouse input");
 menu.click(275,125,640,480);
 need(menu.row==2,"read-only roster does not activate game settings");
 menu.lan->close();menu.lan.reset();
 menu.page=MenuPage::LanRooms;menu.row=0;
 for(int i=1;i<=10;++i)menu.online_rooms.push_back({"Room "+std::to_string(i),rules.stage,27267,3,false});
 auto directory=menu.draw_commands(640,480,0);
 need(std::any_of(directory.begin(),directory.end(),[](const FrontendDraw& d){return d.value=="Room 9"&&d.x==3;})&&
      std::none_of(directory.begin(),directory.end(),[](const FrontendDraw& d){return d.value=="Room 10";}),"directory displays nine rooms in the native first page");
 menu.click(80,311,640,480);
 need(menu.row==8&&menu.page==MenuPage::LanRooms,"directory click selects the visible ninth room and leaves a closed room alone");
 menu.key(40);
 directory=menu.draw_commands(640,480,0);
 need(std::any_of(directory.begin(),directory.end(),[](const FrontendDraw& d){return d.value=="Room 10"&&d.x==3&&d.y==112;}),"directory scrolls to the visible tenth room");
 menu.click(320,410,640,480);
 need(menu.page==MenuPage::LanStart,"directory's lower-balk back action is clickable");
 menu.page=MenuPage::GameOptions;bool option=false;for(auto&label:menu.labels())option|=label.find("LOD:")!=std::string::npos;need(option,"LOD toggle available in game options");bool before=menu.lod_enabled;menu.row=3;menu.key(13);need(menu.lod_enabled!=before,"LOD menu toggle affects renderer setting");
 std::cout<<"UI layout: eight resolutions; HUD containment, bands and mouse mapping passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
