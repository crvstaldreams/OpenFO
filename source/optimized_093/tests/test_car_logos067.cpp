#include "frontend.hpp"
#include "legacy_ui/race_ui.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
static void need(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
static bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(int argc,char**argv){try{
 need(argc>1,"original resource directory required");Resources resources(argv[1]);Frontend menu(resources);menu.page=MenuPage::CarSelect;menu.language=UiLanguage::English;menu.animation_time=1;
 // Literal source BED coordinates indexed by actual car ID, not dealer order.
 const int xy[16][2]={{0,0},{0,90},{0,45},{0,135},{0,180},{0,225},{0,270},{0,315},{221,0},{221,45},{221,90},{221,135},{221,180},{221,225},{221,270},{221,315}};
 need(menu.cars.size()==16,"all sixteen authored dealer cars loaded");
 for(size_t i=0;i<menu.cars.size();++i){menu.car_index=i;const auto&car=menu.cars[i];need(car.number>=1&&car.number<=16,"valid car ID");
  for(auto size:{std::pair<int,int>{640,480},{1280,720},{2560,1080},{720,1280},{320,480}}){
   auto[w,h]=size;MenuViewport viewport(w,h);float k=float(viewport.width/640.);auto commands=menu.draw_commands(w,h,1);const FrontendDraw*logo=nullptr;
   for(const auto&d:commands){if(d.kind==FrontendDraw::Image&&d.value.find("car_logos.")!=std::string::npos){need(!logo,"one selected car title logo");logo=&d;}need(!(d.kind==FrontendDraw::Text&&d.value==car.name),"native logo replaces plain car name");}
   need(logo,"selected car title uses atlas artwork");const auto&d=*logo;const auto&p=xy[car.number-1];
   need(near(d.u,p[0]/512.f)&&near(d.v,p[1]/512.f)&&near(d.uw,220/512.f)&&near(d.vh,44/512.f),"car ID resolves exact BED UV including swapped car2/car3 rows");
   need(near(d.w/d.h,5.f),"title retains source aspect ratio");
   need(d.x>=viewport.x+60*k-.01&&d.x+d.w<=viewport.x+290*k+.01&&d.y>=viewport.map_y(136)-.01&&d.y+d.h<=viewport.map_y(168)+.01,"logo fits description panel title slot at every aspect");
   for(const auto&t:commands)if(t.kind==FrontendDraw::Text)need(!(d.x<t.x+t.w-.1&&t.x<d.x+d.w-.1&&d.y<t.y+t.h-.1&&t.y<d.y+d.h-.1),"logo does not overlap description or stats text");
  }
 }
 // Missing metadata and an unavailable image both preserve the proper name.
 menu.car_index=0;auto original=menu.cars[0].logo;
 for(int missing=0;missing<2;++missing){menu.cars[0].logo=missing?original:MenuArtwork{};if(missing)menu.cars[0].logo.texture="data/menu/missing_car_logo.dds";
  bool name=false,logo=false;for(const auto&d:menu.draw_commands(640,480,1)){name|=d.kind==FrontendDraw::Text&&d.value==menu.cars[0].name;logo|=d.kind==FrontendDraw::Image&&d.value.find("car_logos.")!=std::string::npos;}need(name&&!logo,"unavailable logo falls back to literal proper car name");}
 std::cout<<"car logos067: sixteen car IDs, BED UVs, five aspect ratios, non-overlap and fallback passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
