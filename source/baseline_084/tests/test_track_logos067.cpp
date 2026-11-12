#include "frontend.hpp"
#include "legacy_ui/race_ui.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
static void need(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
static bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(int argc,char**argv){try{
 need(argc>1,"original resource directory required");Resources resources(argv[1]);Frontend menu(resources);menu.page=MenuPage::RaceSetup;menu.language=UiLanguage::English;menu.animation_time=1;
 // Hand-checked native trackmapping.bed Level -> NameIcon. In particular the
 // Gravel Mound courses intentionally use the original forest1 artwork.
 const std::map<int,std::string> mapping={{7,"racing1"},{2,"forest2"},{31,"pit1"},{1,"forest1"},{44,"pit2"},{35,"racing2"},{5,"pit2"},{28,"forest1"},{4,"pit1"},{39,"winter2"},{43,"pit1"},{42,"forest3"},{3,"forest1"},{15,"town2"},{18,"winter2"},{45,"pit3"},{14,"winter2"},{8,"racing2"},{34,"racing1"},{30,"forest3"},{36,"town2"},{40,"forest1"},{17,"winter1"},{9,"racing1"},{11,"town2"},{10,"racing2"},{38,"winter1"},{13,"winter1"},{12,"town3"},{29,"forest2"},{32,"pit2"},{41,"forest2"},{37,"town3"},{16,"town3"},{6,"pit3"},{33,"pit3"},{19,"stunt"},{20,"stunt"},{48,"stunt"},{47,"stunt"},{49,"stunt"},{46,"stunt"},{25,"arena"},{26,"arena"},{27,"arena"},{22,"derby"},{23,"derby"},{24,"derby"}};
 const std::map<std::string,std::pair<int,int>> xy={{"arena",{0,0}},{"forest2",{0,40}},{"town2",{0,80}},{"pit1",{0,120}},{"pit2",{0,160}},{"racing2",{0,200}},{"forest1",{172,0}},{"forest3",{172,40}},{"town3",{172,80}},{"pit3",{172,120}},{"racing1",{172,160}},{"winter1",{172,200}},{"winter2",{344,0}},{"stunt",{344,40}},{"derby",{344,80}}};
 need(menu.track_slots.size()==48,"all native stage slots loaded");
 for(size_t i=0;i<menu.track_slots.size();++i){menu.select_track(i);const auto&t=menu.track_slots[i];need(t.name_icon==mapping.at(t.level),"catalog retains literal NameIcon independently of stage folder");const auto&p=xy.at(mapping.at(t.level));
  for(auto size:{std::pair<int,int>{640,480},{1280,720},{2560,1080},{720,1280},{320,480}}){auto[w,h]=size;MenuViewport view(w,h);float k=float(view.width/640.);auto commands=menu.draw_commands(w,h,1);const FrontendDraw*logo=nullptr;bool name=false;
   for(const auto&d:commands){if(d.kind==FrontendDraw::Image&&d.value.find("track_name_icons.")!=std::string::npos){need(!logo,"one selected location logo");logo=&d;}name|=d.kind==FrontendDraw::Text&&near(d.x,float(view.x)+394*k)&&d.y>=view.map_y(113)&&d.y+d.h<=view.map_y(140);}
   need(logo,"selected stage displays native location logo");const auto&d=*logo;const float clipped_width=p.first==344?168.f:171.f;
   need(near(d.u,p.first/512.f)&&near(d.v,p.second/256.f)&&near(d.uw,clipped_width/512.f)&&near(d.vh,39/256.f),"exact native stage mapping and clipped BED UV");
   need(d.u+d.uw<=1.00001f&&d.v+d.vh<=1.00001f,"edge atlas logos never wrap into adjacent texture content");
   need(near(d.w/d.h,clipped_width/39.f),"clipped source aspect retained");
   need(d.x>=view.x+396*k-.01&&d.x+d.w<=view.x+586*k+.01&&d.y>=view.map_y(70)-.01&&d.y+d.h<=view.map_y(113)+.01,"location title remains above information panel");
   need(name,"existing selected track name remains below logo");
   for(const auto&text:commands)if(text.kind==FrontendDraw::Text)need(!(d.x<text.x+text.w-.1&&text.x<d.x+d.w-.1&&d.y<text.y+text.h-.1&&text.y<d.y+d.h-.1),"location logo does not overlap text");
  }
 }
 menu.select_track(0);auto icon=menu.track_slots[0].name_icon;
 for(const auto&missing:{std::string(),std::string("unavailable_location")}){menu.track_slots[0].name_icon=missing;bool logo=false,name=false;for(const auto&d:menu.draw_commands(640,480,1)){logo|=d.kind==FrontendDraw::Image&&d.value.find("track_name_icons.")!=std::string::npos;name|=d.kind==FrontendDraw::Text&&near(d.x,394)&&d.y>=113&&d.y+d.h<=140;}need(!logo&&name,"missing mapping/artwork retains selected track label");}
 menu.track_slots[0].name_icon=icon;menu.career=true;auto stage=menu.choice.stage;auto level=menu.choice.level;
 for(int row=0;row<=6;++row){menu.row=row;menu.draw_commands(1280,720,1);need(menu.row==row&&menu.choice.stage==stage&&menu.choice.level==level,"location artwork preserves career selection and setting rows");}
 std::cout<<"track logos067: 48 native stage mappings, edge-clipped UVs, five aspect ratios, labels, fallback and career rows passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
