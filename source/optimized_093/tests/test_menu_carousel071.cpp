#include "frontend.hpp"
#include "ui_menu_layout.hpp"
#include "legacy_ui/race_ui.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 const auto empty=fs::temp_directory_path()/"openfo_carousel071_empty";fs::create_directories(empty);Resources resources(empty);Frontend menu(resources);
 const MenuPage pages[]={MenuPage::Main,MenuPage::Garage,MenuPage::Tuning,MenuPage::SinglePlayer,MenuPage::Transmission,MenuPage::ClassSelect,MenuPage::Pause,MenuPage::Confirm,MenuPage::CareerAward,MenuPage::Start};
 for(const auto language:{UiLanguage::English,UiLanguage::Ukrainian})for(auto size:{std::pair<int,int>{640,480},{1024,768},{1280,720},{1920,1080},{2560,1080},{3840,2160}}){
  const auto[w,h]=size;MenuViewport view(w,h);const float k=float(view.width/640.);menu.language=language;
  for(auto page:pages){menu.page=page;const auto labels=menu.labels();
   for(size_t selected=0;selected<labels.size();++selected){
    menu.row=int(selected);menu.menu_scroll={};menu.animation_time=0;
    const auto positions=menu.horizontal_positions(0,k);
    need(std::abs(positions[selected].first+positions[selected].second*.5f-320)<.03f,"selected entry centers exactly at every aspect ratio");
    for(size_t i=1;i<positions.size();++i)need(positions[i].first-positions[i-1].first-positions[i-1].second>=27.99f,"long localized entries retain their gap");
    auto commands=menu.draw_commands(w,h,0);std::vector<FrontendDraw> texts;
    for(const auto&d:commands)if(d.kind==FrontendDraw::Text&&d.clip_left>=0){
     need(std::abs(d.w-ui_text_width_pixels(ui_raster_pixels(d.scale,true),true,d.value))<.01f,"draw and layout use the same physical glyph metrics");
     need(d.clip_left>=view.x&&d.clip_right<=view.x+view.width,"carousel clip stays inside safe viewport");
     texts.push_back(d);
    }
    need(!texts.empty(),"selected entry always rendered");
    std::sort(texts.begin(),texts.end(),[](const auto&a,const auto&b){return a.x<b.x;});
    for(size_t i=1;i<texts.size();++i)need(texts[i-1].x+texts[i-1].w+27*k<=texts[i].x,"actual rendered glyph bounds never overlap");
    const auto expected=ui_menu_label(ui_localize(labels[selected],language),k);
    bool centered=false;for(const auto&d:texts)if(d.value==expected.text)centered|=std::abs(d.x+d.w*.5f-w*.5f)<.1f;
    need(centered,"selected text itself is centered, not only its background");
   }
  }
  menu.page=MenuPage::Main;menu.row=0;menu.menu_scroll={};menu.animation_time=0;
  menu.draw_commands(w,h,0);menu.row=2;menu.animation_time=.016;menu.draw_commands(w,h,.016);
  const auto visible_hits=menu.horizontal_positions(-1,k);
  const float visible_hit=std::clamp(visible_hits[1].first+visible_hits[1].second*.5f,9.f,631.f);
  need(visible_hit>=visible_hits[1].first&&visible_hit<visible_hits[1].first+visible_hits[1].second,"network neighbour is inside visible clip");
  menu.click(int(view.x+visible_hit*k),int(view.map_y(414)),w,h);
  need(menu.row==0&&menu.page==MenuPage::LanStart,"animated physical click follows visible entry at every resolution");
  // A long custom translation stays in its slot using the same fitting path.
  for(const auto&label:{std::string("НАДЗВИЧАЙНО ДОВГА НАЗВА МЕРЕЖЕВОГО БАГАТОКОРИСТУВАЦЬКОГО РЕЖИМУ"),std::string("AN EXCEPTIONALLY LONG MULTIPLAYER NETWORK GAME MENU ENTRY")}){
   const auto measured=ui_menu_label(label,k);
   need(measured.text_width<=400.01f,"very long labels fit their maximum slot");
   need(std::abs(measured.text_width*k-ui_text_width_pixels(measured.pixels,true,measured.text))<.01,"fitted label width remains exact");
  }
 }
 for(float distance=0;distance<320;distance+=4){need(ui_menu_edge_alpha(320+distance)>=ui_menu_edge_alpha(324+distance),"edge opacity decreases monotonically");need(std::abs(ui_menu_edge_alpha(320+distance)-ui_menu_edge_alpha(320-distance))<.001,"edge fade is symmetric");}
 need(ui_menu_edge_alpha(0)==0&&ui_menu_edge_alpha(640)==0,"offscreen entries fade completely");
 menu.language=UiLanguage::Ukrainian;menu.page=MenuPage::Main;menu.row=0;menu.menu_scroll={};menu.animation_time=0;
 const auto start=menu.horizontal_positions(0,1);menu.row=2;const auto changed=menu.horizontal_positions(0,1);
 need(std::abs(start[0].first-changed[0].first)<.001,"selection change starts at the current visual position");
 menu.animation_time=.016;const auto moving=menu.horizontal_positions(.016,1);
 need(moving[0].first<start[0].first,"next entry moves toward center");
 const auto visible=menu.horizontal_positions(-1,1);
 need(std::abs(visible[0].first-moving[0].first)<.001,"mouse reads the same visible animation frame");
 const float target=320-moving[2].second*.5f;
 need(moving[2].first>target,"first animated frame does not snap to target");
 for(int i=2;i<=180;++i){menu.animation_time=i/60.;menu.horizontal_positions(menu.animation_time,1);}
 const auto settled=menu.horizontal_positions(-1,1);
 need(std::abs(settled[2].first+settled[2].second*.5f-320)<.03,"new selected entry settles at center");
 // Click the visible network neighbour while a different entry is centering.
 menu.row=0;menu.menu_scroll={};menu.animation_time=0;menu.draw_commands(640,480,0);menu.row=2;menu.animation_time=.016;
 menu.draw_commands(640,480,.016);const auto hits=menu.horizontal_positions(-1,1);
 const float hit=std::clamp(hits[1].first+hits[1].second*.5f,9.f,631.f);
 need(hit>=hits[1].first&&hit<hits[1].first+hits[1].second,"test neighbour is visibly clickable");
 menu.click(int(hit),414,640,480);need(menu.row==0&&menu.page==MenuPage::LanStart,"click follows animated visible neighbour");
 // A positive float fade can still rasterize to zero alpha. Its clipped
 // neighbour must not own an invisible mouse region at the strip edge.
 menu.language=UiLanguage::English;menu.page=MenuPage::Main;menu.row=0;menu.menu_scroll={};menu.animation_time=0;
 const auto edge_start=menu.horizontal_positions(0,1);
 menu.menu_scroll.value+=edge_start[1].first+edge_start[1].second*.5f-1.f;
 const auto edge_draw=menu.draw_commands(640,480,0);bool invisible_neighbor=false;
 for(const auto&d:edge_draw)if(d.kind==FrontendDraw::Text&&d.value=="MULTIPLAYER")invisible_neighbor=d.clip_left>=0&&(d.color>>24)==0&&d.x+d.w>20;
 need(invisible_neighbor,"edge neighbour really rasterizes transparent over test click");
 menu.message.clear();menu.click(20,414,640,480);
 need(menu.row==0&&menu.page==MenuPage::Main&&menu.message.empty(),"zero-alpha edge neighbour has no invisible mouse hit");
 std::cout<<"Menu carousel: UA/EN, six sizes, ten pages, physical metrics, centered easing, edge fade and animated click passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
