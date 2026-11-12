#include "frontend.hpp"
#include "ui_ttf.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace ofo;
static void need(bool b,const char*why){if(!b)throw std::runtime_error(why);}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("real resource directory required");Resources r(argv[1]);auto root=fs::temp_directory_path()/"openfo_menu064";fs::remove_all(root);Frontend m(r,root);
 need(m.cars.size()==16,"sixteen native dealer cars");for(const auto&car:m.cars){need(!car.description.empty(),"every car has authored description");for(int ai=1;ai<=2;++ai){auto path="cars/car_"+std::to_string(car.number)+"/skinai"+std::to_string(ai)+".dds";if(!r.exists(path))continue;bool found=false;for(const auto&s:car.skins)found|=s.number==100+ai;need(found,"same-car AI texture present in paint catalog");Json mesh={{"materials",Json::array({{{"textures",Json::array({"body.tga","body_dirt.tga","shadow.tga"})}}})}};need(apply_menu_skin(mesh,r,car.number,100+ai),"bot skin mapping applies");need(mesh["materials"][0]["textures"][0].get<std::string>().find("skinai"+std::to_string(ai)+".dds")!=std::string::npos,"bot skin uses AI filename");need(mesh["materials"][0]["textures"][1]=="body_dirt.tga","bot paint preserves dirt texture");}}
 m.key(13);m.key(13);for(char c:std::string("DRIVER"))m.profile_character(c);m.key(40);m.key(39);m.key(40);m.key(39);m.key(40);m.key(13);need(m.page==MenuPage::Main&&m.choice.driver_gender==1&&m.choice.handling_difficulty==2,"female Pro creation");m.choice.skin=101;m.save_profile();Frontend reload(r,root);need(reload.choice.skin==101,"AI paint stable ID persists");need(reload.choice.driver_gender==1&&reload.choice.handling_difficulty==2,"driver handling persisted");
 Resources alpha_assets(argv[1]);if(argc>3)alpha_assets.mount(argv[3],"alpha_content");
 Frontend alpha(alpha_assets,root/"alpha");
 alpha.key(13);alpha.key(13);for(char c:std::string("JOHNNY"))alpha.profile_character(c);
 alpha.key(40);alpha.key(37);
 need(alpha.profile_draft_gender==2&&alpha.labels()[1]=="DRIVER: JOHNNY (ALPHA)","left wraps from male to Johnny");
 for(auto locale:{UiLanguage::Ukrainian,UiLanguage::English}){
  alpha.language=locale;bool portrait=false,label=false;
  for(const auto&cmd:alpha.draw_commands(640,480)){
   portrait|=cmd.kind==FrontendDraw::Image&&cmd.value==(argc>3?"alpha_content/driver/johnny_portrait.tga":"data/menu/male.dds");
   label|=cmd.kind==FrontendDraw::Text&&cmd.value==(locale==UiLanguage::Ukrainian?"ВОДІЙ: ДЖОННІ (АЛЬФА)":"DRIVER: JOHNNY (ALPHA)");
  }
  need(portrait&&label,"Johnny portrait and localized profile label");
 }
 alpha.key(39);need(alpha.profile_draft_gender==0,"right wraps from Johnny to male");
 alpha.key(39);need(alpha.profile_draft_gender==1,"female keeps profile ID 1");
 alpha.key(13);need(alpha.profile_draft_gender==2,"confirm cycles driver option forward");
 alpha.key(40);alpha.key(40);alpha.key(13);
 need(alpha.page==MenuPage::Main&&alpha.choice.driver_gender==2,"Johnny profile created");
 Frontend reload_alpha(alpha_assets,root/"alpha");need(reload_alpha.choice.driver_gender==2,"Johnny profile restored after restart");
 m.page=MenuPage::Options;m.row=0;m.music_volume=50;m.key(37);need(m.music_volume==40,"audio keyboard decreases");m.click(566,170,640,480);need(m.music_volume==100,"audio slider click reaches max");m.click(370,170,640,480);need(m.music_volume==0,"audio slider click reaches min");m.page=MenuPage::Controls;m.row=ControlSettings::Count;m.key(39);double dz=m.controls.deadzone;m.key(37);need(m.controls.deadzone<dz,"deadzone keyboard changes");m.row=ControlSettings::Count+1;m.key(39);double sens=m.controls.sensitivity;m.key(37);need(m.controls.sensitivity<sens,"sensitivity keyboard changes");
 m.page=MenuPage::SinglePlayer;m.row=3;m.key(13);need(m.choice.bonus_free_roam&&!m.career&&m.page==MenuPage::CarSelect,"bonus entry outside career");m.key(13);m.key(13);need(m.start&&m.choice.event=="FREEROAM"&&m.choice.bots==0,"bonus starts native menu world without bots");need(r.exists(m.choice.track)&&r.exists(m.choice.stage+"/data/startpoints.bed"),"bonus native geometry and spawn data");
 // Returning from the bonus setup must not leak its runtime bypasses into any race mode.
 for(int mode:{0,1,2}){Frontend flow(r);flow.page=MenuPage::SinglePlayer;flow.row=3;flow.key(13);flow.back();need(flow.page==MenuPage::SinglePlayer&&flow.choice.bonus_free_roam,"bonus setup back path reproduced");flow.row=mode;flow.key(13);need(!flow.choice.bonus_free_roam,"normal mode entry clears bonus runtime bypass");need(flow.choice.time_trial==(mode==2),"normal mode selects correct time trial state");flow.key(13);flow.key(13);if(mode==0){need(flow.page==MenuPage::Garage&&flow.career,"bonus back career buys starter and reaches garage");flow.key(13);}need(flow.page==MenuPage::ClassSelect,"normal mode continues to class selection");flow.row=0;flow.key(13);need(flow.page==MenuPage::RaceSetup&&flow.choice.event!="FREEROAM"&&flow.choice.stage!="tracks/menu/menu1/a","normal mode selects authored race stage");if(mode==0){need(flow.choice.career_race&&flow.choice.bots==7,"career restores full race participation");flow.row=3;flow.key(13);need(flow.start&&flow.begin_career_race()&&flow.career_progress.pending,"career can start and record pending event after bonus");flow.complete_career_race({{"car",0},{"rank",1},{"time",120.},{"finished",true}});need(!flow.career_progress.pending&&flow.career_progress.position(flow.choice.level)==1,"career event settles after bonus setup cancellation");}}
 m.choice.bonus_free_roam=true;m.choice.time_trial=true;need(m.activate_profile(m.profile_id)&&!m.choice.bonus_free_roam&&!m.choice.time_trial,"profile activation resets transient event modes");
 m.page=MenuPage::CarSelect;m.animation_time=0;auto first=m.showroom(0);m.animation_time=4;auto still=m.showroom(4);need(first.enabled&&length(first.eye-still.eye)>.01f,"dealer camera dynamically follows original spline");m.select_car(1);m.animation_time=4.01;auto old=m.showroom(4.01);m.animation_time=4.07;auto fading=m.showroom(4.07);need(old.slot==first.slot&&fading.fade_alpha>.4,"dealer fades old shot before cut");m.animation_time=4.15;auto cut=m.showroom(4.15);need(cut.slot==2&&cut.fade_alpha>0,"dealer cuts to authored next slot while black");m.animation_time=4.5;need(m.showroom(4.5).fade_alpha==0,"dealer fade completes");
 // Class selection during career must remain in the authored garage bay.
 m.page=MenuPage::ClassSelect;m.career=true;m.animation_time=5;
 auto garage_class=m.showroom(5);
 need(garage_class.enabled&&garage_class.slot==19&&garage_class.show_car,"career class remains in garage bay");
 m.career=false;auto noncareer_class=m.showroom(5.1);
 need(noncareer_class.enabled&&noncareer_class.slot==19,"quick-race class also remains in garage bay");
 Json snapshots=Json::array();
 for(auto dims:{std::pair<int,int>{640,480},{1280,720},{1920,1080},{2560,1080},{720,1280}}){for(auto page:{MenuPage::CarSelect,MenuPage::Profile,MenuPage::Options,MenuPage::Controls}){m.page=page;m.row=page==MenuPage::Controls?12:0;m.profile_editing=page==MenuPage::Profile;auto commands=m.draw_commands(dims.first,dims.second,4.5);Json snap={{"title",m.title()},{"width",dims.first},{"height",dims.second},{"commands",Json::array()}};for(const auto&c:commands)snap["commands"].push_back({{"kind",int(c.kind)},{"scale",c.scale},{"heading",c.heading},{"x",c.x},{"y",c.y},{"w",c.w},{"h",c.h},{"u",c.u},{"v",c.v},{"uw",c.uw},{"vh",c.vh},{"color",c.color},{"value",c.value}});snapshots.push_back(snap);int description_lines=0;for(auto&d:commands){if(d.kind==FrontendDraw::Text){need(d.x>=-.1&&d.y>=-.1&&d.x+d.w<=dims.first+.1&&d.y+d.h<=dims.second+.1,"064 text bounds at every resolution");if(page==MenuPage::CarSelect&&d.value.size()>20)++description_lines;}need(d.value.find("car_selection_colorblock")==std::string::npos,"no color-selector overlay image");}if(page==MenuPage::Profile){bool portrait=false;for(const auto&d:commands)portrait|=d.kind==FrontendDraw::Image&&d.value=="data/menu/female.dds";need(portrait,"original selected driver portrait");}if(page==MenuPage::CarSelect)need(description_lines>=2,"description wraps to multiple lines");for(size_t a=0;a<commands.size();++a)if(commands[a].kind==FrontendDraw::Text)for(size_t b=a+1;b<commands.size();++b)if(commands[b].kind==FrontendDraw::Text){auto&A=commands[a];auto&B=commands[b];need(!(A.x<B.x+B.w-.5f&&B.x<A.x+A.w-.5f&&A.y<B.y+B.h-.5f&&B.y<A.y+A.h-.5f),"064 text nonoverlap");}}}
 if(argc>2&&argv[2][0])std::ofstream(argv[2])<<snapshots.dump();fs::remove_all(root);std::cout<<"menu064: real AI skin assets, two native handling choices, profile persistence, interactive sliders, dynamic camera fade, bonus world and five-resolution text passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
