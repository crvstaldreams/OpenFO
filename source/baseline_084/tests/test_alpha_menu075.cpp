#include "frontend.hpp"
#include "alpha_car.hpp"
#include "editor_scene_query.hpp"
#include "ui_menu_layout.hpp"
#include "legacy_ui/race_ui.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace ofo;
namespace {
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool command(const std::vector<FrontendDraw>& draw,FrontendDraw::Kind kind,const std::string& value){
 return std::any_of(draw.begin(),draw.end(),[&](const FrontendDraw& d){return d.kind==kind&&d.value==value;});
}
}
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("usage: test_alpha_menu075 retail-data alpha_content");
 Resources resources(argv[1]);resources.mount(argv[2],"alpha_content");
 Frontend menu(resources);menu.language=UiLanguage::English;
 need(menu.cars.size()==20,"four alpha cars follow the complete retail roster");
 for(size_t i=0;i<16;++i)need(menu.cars[i].number>=1&&menu.cars[i].number<=16,"retail car retained before alpha group");
 for(int i=0;i<4;++i)need(menu.cars[16+i].number==17+i,"alpha cars are ordered at end");
 menu.page=MenuPage::SinglePlayer;menu.row=1;menu.key(13);
 need(menu.page==MenuPage::CarSelect,"quick race opens dealer");
 menu.select_car(15);menu.key(39);
 need(menu.selected_car_id()==17,"car navigation crosses into alpha group");
 auto car=menu.draw_commands(1280,720,1);
 need(command(car,FrontendDraw::Text,"ALPHA CONTENT"),"selected prototype shows section label");
 need(command(car,FrontendDraw::Image,"alpha_content/menu/car/car1.tga"),"original prototype car thumbnail");
 need(menu.showroom().car_geometry=="alpha_content/cars/car_1/model.showcar","prototype showroom uses original prototype model");
 menu.key(13);menu.key(13);
 need(menu.page==MenuPage::ClassSelect,"transmission leads to classes with alpha car selected");
 auto classes=menu.labels();
 need(classes.size()==5&&classes.back()=="ALPHA CONTENT","alpha tracks follow standard class options");
 menu.row=int(classes.size())-1;menu.key(13);
 need(menu.page==MenuPage::AlphaTracks&&menu.labels().size()==2,"independent two-track submenu");
 auto cards=menu.draw_commands(1280,720,1);
 need(command(cards,FrontendDraw::Image,"alpha_content/menu/track/track1a.tga")&&
      command(cards,FrontendDraw::Image,"alpha_content/menu/track/track2a.tga"),"original track previews are visible");
 menu.key(27);
 need(menu.page==MenuPage::ClassSelect&&menu.row==4,"Escape returns to alpha class selection");
 menu.key(13);menu.key(39);menu.key(13);
 need(menu.start&&menu.selected_stage()=="alpha_content/tracks/track2","keyboard starts selected prototype stage");
 need(menu.choice.bonus_free_roam&&menu.choice.event=="FREEROAM"&&!menu.choice.career_race&&menu.choice.bots==0,"alpha stages launch in isolated free drive");
 menu.clear_actions();menu.key(27);menu.row=0;menu.key(13);
 need(menu.page==MenuPage::RaceSetup&&!menu.choice.bonus_free_roam,"return to retail class restores normal race");
 menu.key(27);menu.row=4;menu.key(13);
 MenuViewport viewport(1920,1080);
 const int x=int(viewport.x+(alpha_card_left(0)+alpha_card_width/2)*viewport.width/640);
 const int y=int(viewport.map_y(alpha_card_top+alpha_card_height/2));
 menu.click(x,y,1920,1080);
 need(menu.start&&menu.selected_stage()=="alpha_content/tracks/track1","card click selects and launches matching track");
 menu.clear_actions();menu.enter_career();menu.page=MenuPage::CarSelect;menu.select_car(15);menu.key(39);
 need(menu.selected_car_id()<=16,"career wraps within the retail car roster");
 menu.page=MenuPage::ClassSelect;need(menu.labels().size()==4,"career excludes prototype class");
 Frontend dealer(resources);
 dealer.page=MenuPage::CarSelect;
 for(int id=17;id<=20;++id){
  dealer.select_car(size_t(id-1));
  const double time=2.*(id-16);
  dealer.showroom(time);
  const auto scene=dealer.showroom(time+.4);
  need(scene.car_geometry==alpha_car_directory(id)+"/model.showcar","Selected prototype reaches the dealer stand");
  const auto display=load_geometry(resources,scene.car_geometry);
  EditorSceneQuery floor(load_geometry(resources,scene.geometry,scene.config_base));
  int tires=0;
  bool touches=false;
  for(const auto& mesh:display.meshes){
   if(mesh.name.rfind("tire_",0)!=0)continue;
   Vec bottom{0,INFINITY,0};
   for(const auto& vertex:mesh.vertices){
    auto p=point(scene.car_transform,point(mesh.transform,vertex.position));
    if(p.y<bottom.y)bottom=p;
   }
   const auto ground=floor.ray(bottom+Vec{0,.4f,0},{0,-1,0},.8f);
   need(ground.has_value()&&bottom.y>=ground->y-.004f,"Fitted tire cannot sink through the original showroom floor");
   touches=touches||bottom.y<=ground->y+.004f;
   ++tires;
  }
  need(tires==4&&touches,"Prototype rests on its lowest tire in the dealer");
 }
 std::cout<<"Alpha dealer, original previews, independent track menu, keyboard launch and career isolation passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
