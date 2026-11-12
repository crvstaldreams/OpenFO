#include "frontend.hpp"
#include "frontend_hud.hpp"
#include "ui_ttf.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ofo;
static void need(bool value,const std::string&why){if(!value)throw std::runtime_error(why);}
// Layout bounds describe complete glyph runs; clipping defines the visible run.
static FrontendDraw visible_text(FrontendDraw d){
 const float right=d.clip_right>=0?std::min(d.x+d.w,d.clip_right):d.x+d.w;
 d.x=d.clip_left>=0?std::max(d.x,d.clip_left):d.x;
 d.w=(d.color>>24)?std::max(0.f,right-d.x):0.f;return d;
}
int main(int argc,char**argv){try{
 const auto empty=fs::temp_directory_path()/"openfo_menu_layout062_empty";fs::create_directories(empty);
 Resources resources(argc>1?fs::path(argv[1]):empty);Frontend menu(resources);Json snapshots=Json::array();
 for(auto dims:{std::pair<int,int>{640,480},{1280,720},{1920,1080},{2560,1080},{720,1280}}){
  auto[w,h]=dims;
  for(MenuPage page:{MenuPage::Profile,MenuPage::CareerAward,MenuPage::SmashBonus,MenuPage::Garage,MenuPage::Tuning,MenuPage::Upgrade,MenuPage::Stats,MenuPage::GameOptions,MenuPage::Main,MenuPage::SinglePlayer,MenuPage::RaceSetup,MenuPage::ClassSelect,MenuPage::CarSelect,MenuPage::Transmission,MenuPage::Pause,MenuPage::Confirm,MenuPage::Options,MenuPage::Controls,MenuPage::Credits}){
   menu.page=page;menu.animation_time=2;menu.row=0;menu.message="SERVICE: menu layout audit";menu.last_career_result={{"place",1},{"award",709},{"prop_bonus",9},{"prop_hits_by_type",std::array<int,10>{0,1,15}},{"prop_cents_by_type",std::array<int,10>{0,160,705}}};
   auto commands=menu.draw_commands(w,h,2);int cups=0,smash_art=0;bool total9=false,road2=false,fence7=false;Json snapshot={{"title",menu.title()},{"width",w},{"height",h},{"commands",Json::array()}};
   for(auto&c:commands){
    need(std::isfinite(c.x)&&std::isfinite(c.y),"finite coordinates");
    if(c.kind==FrontendDraw::Text){auto visible=visible_text(c);if(visible.w>0)need(visible.x>=-.1&&visible.y>=-.1&&visible.x+visible.w<=w+.1&&visible.y+visible.h<=h+.1,"visible text bounds: "+menu.title()+" "+c.value);float measured=ui_text_width_pixels(ui_raster_pixels(c.scale,c.heading),c.heading,c.value);need(std::abs(measured-c.w)<1.1,"text measurement matches raster size: "+c.value);}
    if(c.kind==FrontendDraw::Image&&c.value.find("/trophies.")!=std::string::npos)++cups;
    if(c.kind==FrontendDraw::Image&&c.value.find("/smashysmash.")!=std::string::npos)++smash_art;
    if(c.kind==FrontendDraw::Text){total9|=c.value=="$9";road2|=c.value=="$2";fence7|=c.value=="$7";}
    if(c.kind==FrontendDraw::Image)need(c.u>=0&&c.v>=0&&c.u+c.uw<=1.001&&c.v+c.vh<=1.001,"atlas UV bounds: "+c.value);
    snapshot["commands"].push_back({{"clip_left",c.clip_left},{"clip_right",c.clip_right},{"kind",int(c.kind)},{"scale",c.scale},{"heading",c.heading},{"x",c.x},{"y",c.y},{"w",c.w},{"h",c.h},{"u",c.u},{"v",c.v},{"uw",c.uw},{"vh",c.vh},{"color",c.color},{"value",c.value}});
   }
   need(cups==(page==MenuPage::CareerAward&&menu_artwork(resources,"trophies","goldtrophy")?1:0),"one native cup only on award page");
   if(page==MenuPage::SmashBonus){need(total9&&road2&&fence7,"original example rounds to $2, $7 and $9");if(menu_artwork(resources,"smashysmash","balk"))need(smash_art==16,"all eight original icons and bars");}
   for(size_t a=0;a<commands.size();++a)if(commands[a].kind==FrontendDraw::Text)for(size_t b=a+1;b<commands.size();++b)if(commands[b].kind==FrontendDraw::Text){auto A=visible_text(commands[a]),B=visible_text(commands[b]);bool overlaps=A.w>0&&B.w>0&&A.x<B.x+B.w-.5f&&B.x<A.x+A.w-.5f&&A.y<B.y+B.h-.5f&&B.y<A.y+A.h-.5f;need(!overlaps,"overlapping text in "+menu.title()+": "+A.value+" / "+B.value);}
   snapshots.push_back(snapshot);
  }
 }
 menu.page=MenuPage::CareerAward;menu.row=0;menu.key(13);need(menu.page==MenuPage::SmashBonus,"cup continues to category bonus");menu.key(13);need(menu.page==MenuPage::Garage,"bonus continues to garage");
 menu.page=MenuPage::Tuning;menu.row=menu.tuning_category=0;auto serial=menu.navigation_serial;menu.key(39);need(menu.tuning_category==1&&menu.row==1,"horizontal tuning updates camera category");need(menu.navigation_serial>serial,"cycling emits navigation event");
 menu.page=MenuPage::Main;menu.row=0;menu.animation_time=0;auto initial=menu.horizontal_positions();menu.row=5;auto first=menu.horizontal_positions();need(std::abs(initial[0].first-first[0].first)<.01,"selection starts at visible position");menu.animation_time=.05;auto moving=menu.horizontal_positions();menu.animation_time=.8;for(int n=0;n<12;++n){menu.animation_time+=.05;menu.horizontal_positions();}auto final=menu.horizontal_positions();need(moving[0].first<first[0].first&&moving[0].first>final[0].first,"carousel moves smoothly toward selection");
 need(ui_text_width(FontStyle::Regular18,"WWW")>ui_text_width(FontStyle::Regular18,"III")*2,"portable Play uses proportional advances");
 auto fit=ui_fit_text("A very long UTF-8 name \xc3\x89",70,1,false);need(ui_text_width_pixels(18,false,fit)<=70,"ellipsis fitting stays inside physical bounds");
 if(resources.exists("data/tracks/menu/menu1/a/data/dummies.bed")){
  Frontend camera(resources);camera.page=MenuPage::Tuning;camera.tuning_category=0;camera.animation_time=0;auto from=camera.showroom(0);
  camera.tuning_category=1;camera.animation_time=.016;auto middle=camera.showroom(.016);
  Frontend exact(resources);exact.page=MenuPage::Tuning;exact.tuning_category=1;exact.animation_time=.016;auto destination=exact.showroom(.016);
  auto distance=[](Vec a,Vec b){auto d=a-b;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);};
  need(from.enabled&&middle.enabled&&destination.enabled,"tuning camera reads original dummies");need(distance(from.target,middle.target)>0&&distance(from.target,middle.target)<distance(from.target,destination.target),"camera focus eases toward selected category");
 }
 FrontendHud hud(resources);RaceOverlayState race;race.elapsed=142.37;race.lap_time=42.1;race.position=3;race.participants=8;race.lap=2;race.laps=3;race.show_positions=true;race.music_artist="ARTIST / TRACK RADIO";race.music_title="A native music title for the current race";race.music_age=2;
 for(int i=0;i<8;++i){RaceOverlayEntry e;e.rank=i+1;e.name=i==2?"PLAYER WITH A LONG PROFILE NAME":"OPPONENT "+std::to_string(i+1);e.car_name="NATIVE CAR MODEL";e.player=i==2;e.finished=i<5;e.time=180.127+i*.742;e.lap=2;e.laps=3;e.status=i<5?"FINISHED":"RACING";race.leaderboard.push_back(e);}
 for(int mode=0;mode<6;++mode){race.finished=mode==1;race.results=true;race.replay=mode==2;race.stunt.kind=mode==3||mode==4?StuntKind::Highjump:StuntKind::None;race.event_intro=mode==3;race.event_hint=event_intro_hint(resources,RaceMode::Stunt,race.stunt.kind,race.language);race.stunt.name="HIGHJUMP";race.stunt.attempt_done=race.stunt.finished=mode==4;race.stunt.attempt=2;race.stunt.thresholds={55,40,25};race.stunt.prizes={700,400,200};race.stunt.scores={12.4,35.7,43.8};race.countdown=mode==5?2.6:0;race.result_detail="CAREER AWARD $700 | EVENT COMPLETED";
  Json snap={{"title",mode==0?"RACE HUD":mode==1?"RACE RESULTS":mode==2?"REPLAY":mode==3?"STUNT INSTRUCTIONS":mode==4?"STUNT COMPLETE":"COUNTDOWN"},{"width",1280},{"height",720},{"commands",Json::array()}};
  for(auto&c:hud.commands(1280,720,race))snap["commands"].push_back({{"clip_left",c.clip_left},{"clip_right",c.clip_right},{"kind",int(c.kind)},{"scale",c.scale},{"heading",c.heading},{"x",c.x},{"y",c.y},{"w",c.w},{"h",c.h},{"u",c.u},{"v",c.v},{"uw",c.uw},{"vh",c.vh},{"color",c.color},{"value",c.value}});snapshots.push_back(snap);
 }
 if(argc>2)std::ofstream(argv[2])<<snapshots.dump();
 std::cout<<"menu layout062: 95 page/resolution combinations, measured text containment/non-overlap, UV bounds, navigation, and easing passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
