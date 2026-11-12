#include "frontend.hpp"
#include "file_io.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ofo;
static void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 auto e=TrackEnvironment::from_json({{"time_of_day",7},{"weather",false},{"sun_intensity","oops"},{"fog_density",-20},{"rain_strength",9}});
 require(!e.night&&!e.rain&&e.sun_intensity==1&&e.fog_density==0&&e.rain_strength==1,"malformed fields use safe defaults and clamp individually");
 e.sun_intensity=std::numeric_limits<float>::infinity();e.fog_density=std::numeric_limits<float>::quiet_NaN();e.rain_strength=-1;e.validate();require(e.sun_intensity==1&&e.fog_density==0&&e.rain_strength==0,"nonfinite runtime sanitization");
 e.night=e.rain=true;auto round=TrackEnvironment::from_json(e.json());require(round.night&&round.rain,"environment round trip");
 if(argc<2){std::cout<<"environment validation passed; pass resources for profile/UI checks\n";return 0;}
 Resources r(argv[1]);auto root=fs::temp_directory_path()/"openfo_environment_test";fs::remove_all(root);
 Frontend m(r,root);require(m.create_profile("WEATHER PLAYER"),"create first profile");auto first=m.profile_id;
 m.choice.stage="tracks/forest/forest1/a";m.apply_track_settings();require(!m.choice.environment.night&&!m.choice.environment.rain,"old saves default day dry");
 m.page=MenuPage::RaceSetup;m.row=4;m.key(39);m.row=5;m.key(13);m.row=6;m.key(39);require(m.choice.environment.night&&m.choice.environment.rain&&m.choice.ai_handicap_level==4,"keyboard conditions and native AI handicap");
 auto settings=read_file(m.settings_path);auto controls=m.controls.keys;auto save=read_file(m.profile_path);auto career=m.career_progress.json();
 m.choice.stage="tracks/forest/forest1/b";m.choice.ai_handicap_level=2;m.apply_track_settings();require(!m.choice.environment.night&&!m.choice.environment.rain&&m.choice.ai_handicap_level==2,"stage b distinct and authored handicap retained");
 m.row=5;m.key(39);require(m.choice.environment.rain&&!m.choice.environment.night,"stage b rain-only override");
 require(read_file(m.settings_path)==settings&&read_file(m.profile_path)==save&&m.controls.keys==controls&&m.career_progress.json()==career,"environment changes leave settings controls career untouched");
 m.choice.stage="tracks/forest/forest1/a";m.apply_track_settings();require(m.choice.environment.night&&m.choice.environment.rain&&m.choice.ai_handicap_level==4,"stage a restores own choices");
 Frontend reload(r,root);reload.choice.stage=m.choice.stage;reload.apply_track_settings();require(reload.choice.environment.night&&reload.choice.environment.rain&&reload.choice.ai_handicap_level==4,"saved track parameters reload");
 require(m.create_profile("SUNNY PLAYER"),"create second profile");m.choice.stage="tracks/forest/forest1/a";m.apply_track_settings();require(!m.choice.environment.night&&!m.choice.environment.rain,"profiles isolated");require(m.activate_profile(first),"restore first profile");m.choice.stage="tracks/forest/forest1/a";m.apply_track_settings();require(m.choice.environment.night&&m.choice.environment.rain,"first profile intact");
 // All parameter mouse regions coincide with the visible rows at 640x480.
 m.page=MenuPage::RaceSetup;m.click(550,214,640,480);require(m.row==4&&!m.choice.environment.night,"mouse day/night row");m.click(550,240,640,480);require(m.row==5&&!m.choice.environment.rain,"mouse weather row");m.row=2;m.key(40);require(m.row==4,"dpad follows panel order");m.row=6;m.key(40);require(m.row==3,"dpad reaches start");
 for(auto size:{std::pair<int,int>{800,600},{1280,720},{1920,1080}}){auto cmds=m.draw_commands(size.first,size.second,.25);bool day=false,rain=false,ai=false;for(auto&c:cmds){day|=c.value==ui_localize("TIME OF DAY",m.language);rain|=c.value==ui_localize("WEATHER",m.language);ai|=c.value==ui_localize("AI DIFFICULTY",m.language);if(c.kind==FrontendDraw::Text&&c.x>=size.first*.59f&&c.y>size.second*.2f&&c.y<size.second*.8f)require(c.x+c.w<=size.first+1,"right panel text stays in viewport");}require(day&&rain&&ai,"visible parameter labels");}
 if(argc>2){for(size_t i=0;i<m.track_slots.size();++i)if(m.track_slots[i].stage=="tracks/forest/forest1/a"){m.select_track(i);break;}Json snapshots=Json::array();for(int variant=0;variant<4;++variant){m.choice.environment.night=variant&1;m.choice.environment.rain=variant&2;m.row=4+bool(variant&2);Json snapshot={{"title",std::string(m.choice.environment.night?"NIGHT":"DAY")+" / "+(m.choice.environment.rain?"RAIN":"DRY")+" (semantic command preview)"},{"width",1280},{"height",720},{"commands",Json::array()}};for(const auto&c:m.draw_commands(1280,720,.25))snapshot["commands"].push_back({{"kind",int(c.kind)},{"scale",c.scale},{"heading",c.heading},{"x",c.x},{"y",c.y},{"w",c.w},{"h",c.h},{"u",c.u},{"v",c.v},{"uw",c.uw},{"vh",c.vh},{"color",c.color},{"value",c.value}});snapshots.push_back(snapshot);}write_file_atomic(argv[2],snapshots.dump());}
 write_file_atomic(m.track_settings_path,"{invalid");m.load_track_settings();require(!m.choice.environment.night&&!m.choice.environment.rain,"broken JSON safely defaults");
 fs::remove_all(root);std::cout<<"environment: finite validation, stage/profile isolation, untouched save/controls/settings, keyboard and mouse passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
