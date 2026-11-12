#include "ui_ttf.hpp"
#include "smash_award.hpp"
#include "ui_menu_layout.hpp"
#include "frontend.hpp"
#include "legacy_ui/race_ui.hpp"
#include "legacy_ui/menu_balks.hpp"
#include <algorithm>
#include <sstream>
#include <tuple>
namespace ofo {

std::vector<FrontendDraw> Frontend::draw_commands(int width,int height,double clock)const{
 std::vector<FrontendDraw> out;if(width<=0||height<=0)return out;
 MenuViewport safe(width,height);float k=float(safe.width/640),ox=float(safe.x),oy=float(safe.y);uint32_t color=0xffffffff;double now=animation_time>=0?animation_time:clock;float pulse=selection_brightness(now);
 auto rect=[&](float x,float y,float w,float h,uint32_t c){FrontendDraw d;d.x=ox+x*k;d.y=float(safe.map_y(y));d.w=w*k;d.h=h*k;d.color=c;out.push_back(d);};
 auto img=[&](const std::string&p,float x,float y,float w,float h,float u=0,float v=0,float uw=1,float vh=1){if(!assets||!assets->exists(p))return;FrontendDraw d;d.kind=FrontendDraw::Image;d.value=p;d.x=ox+x*k;d.y=float(safe.map_y(y));d.w=w*k;d.h=h*k;d.u=u;d.v=v;d.uw=uw;d.vh=vh;d.color=color;
 // Match the source viewport clipping, including scrolled neighbouring icons.
 float left=std::max(d.x,ox),top=std::max(d.y,0.f),right=std::min(d.x+d.w,ox+640*k),bottom=std::min(d.y+d.h,float(height));
 if(right<=left||bottom<=top||d.w<=0||d.h<=0)return;
 d.u+=(left-d.x)/d.w*d.uw;d.v+=(top-d.y)/d.h*d.vh;d.uw*=(right-left)/d.w;d.vh*=(bottom-top)/d.h;d.x=left;d.y=top;d.w=right-left;d.h=bottom-top;out.push_back(d);};
 auto artwork=[&](const MenuArtwork&a,float x,float y,float w,float h){if(a)img(a.texture,x,y,w,h,float(a.x)/a.atlas_width,float(a.y)/a.atlas_height,float(a.width)/a.atlas_width,float(a.height)/a.atlas_height);};
 auto find_artwork=[&](const std::string&atlas,const std::string&item)->const MenuArtwork&{auto id=atlas+"/"+item;auto it=artwork_cache.find(id);if(it==artwork_cache.end())it=artwork_cache.emplace(id,assets?menu_artwork(*assets,atlas,item):MenuArtwork{}).first;return it->second;};
 auto art=[&](const std::string&atlas,const std::string&item,float x,float y,float w,float h){artwork(find_artwork(atlas,item),x,y,w,h);};
 auto cover=[&](const std::string& path){
  if(!assets||!assets->exists(path))return;
  FrontendDraw d;d.kind=FrontendDraw::Image;d.value=path;d.w=float(width);d.h=float(height);
  const float aspect=float(width)/height;
  if(aspect>4.f/3){d.vh=(4.f/3)/aspect;d.v=(1-d.vh)*.5f;}else{d.uw=aspect/(4.f/3);d.u=(1-d.uw)*.5f;}
  out.push_back(d);
 };
 // Text remains semantic until the renderer rasterizes the bundled TrueType face.
 auto label=[&](float x,float y,const std::string&source,FontStyle style=FontStyle::Regular18,float scale=1.5f,float bound=0,bool translate=true){
  const auto s=translate?ui_localize(source,language):source;
  if(s.empty()||x<0||x>=632)return;
  float sc=scale/1.5f,available=(bound>0?std::min(bound,632-x):632-x)*k;
  // Re-measure after rounding exactly as text() will rasterize it. Long names,
  // bindings and translations stay inside their own row or information panel.
  float measured=ui_text_width_scaled(style,s,sc*k);
  if(measured>available){sc*=available/measured;measured=ui_text_width_scaled(style,s,sc*k);}
  for(int n=0;n<4&&measured>available;++n){sc*=.98f;measured=ui_text_width_scaled(style,s,sc*k);}
  FrontendDraw d;d.kind=FrontendDraw::Text;d.value=s;d.x=ox+x*k;d.y=float(safe.map_y(y-ui_font_pixels(style)*sc));
  d.scale=sc*k*ui_font_pixels(style)/18.f;d.color=color;d.heading=ui_font_bold(style);if(d.heading)d.scale*=18.f/24.f;
  d.value=ui_fit_text(s,available,d.scale,d.heading);if(d.value.empty())return;d.w=ui_text_width_pixels(ui_raster_pixels(d.scale,d.heading),d.heading,d.value);d.h=float(ui_raster_pixels(d.scale,d.heading));out.push_back(d);
 };
 auto source_text=[&](float x,float y,const std::string&s,FontStyle style){label(x,y+ui_font_pixels(style),s,style);};
 auto single_line=[](std::string value){for(char& c:value)if(static_cast<unsigned char>(c)<32)c=' ';return value;};
 auto selected=[&](bool on){unsigned alpha=unsigned((on?pulse:.58f)*255);color=(alpha<<24)|(on?0x00ffd442:0x00aeb4bc);};
 // Completion marks follow the source I / II / III colored-disc vocabulary.
 auto completion_medal=[&](int place,float x,float y,float size){if(place<1||place>3)return;uint32_t saved=color;color=0xffffffff;art("common",place==1?"first":place==2?"second":"third",x,y,size,size);color=saved;};

 if(page==MenuPage::Credits)img("data/menu/bg/credits"+std::to_string(credits_index)+".tga",0,0,640,480);
 if(page==MenuPage::RaceSetup){auto found=previews.find(choice.track);if(found!=previews.end())cover(found->second);}
 if(page==MenuPage::LanLobby||page==MenuPage::LanRooms)cover(lan_game_background);
 // Restore the PC menu's original BED-driven rust/keys atlas. The patch's
 // menutemplate keeps these native coordinates and skips console upper_ext.
 auto balk=[&](const char* item,float y){append_menu_balk(out,find_artwork("balks",item),width,height,y);};
 balk("upperbalk",-1);
 balk("lowerbalk",393);
 balk("lowerbalk_ext",437);
 if(pause_context){FrontendDraw d;d.x=0;d.y=63*k;d.w=float(width);d.h=height-150*k;d.color=0x73000000;out.push_back(d);}
 const bool room_title=page==MenuPage::LanLobby&&lan&&!lan->room_name().empty();
 color=0xffffd442;size_t title_first=out.size();label(7,49,single_line(title()),FontStyle::Italic24,1.25f,0,!room_title);for(size_t i=title_first;i<out.size();++i)out[i].x-=ox;
 if(page==MenuPage::Garage||page==MenuPage::Tuning||page==MenuPage::Upgrade||page==MenuPage::Stats){art("common","rahaikoni",410,31,25,23);source_text(440,31,"$"+std::to_string(money),FontStyle::Italic14);}
 auto items=labels();bool horizontal=page==MenuPage::SmashBonus||page==MenuPage::CareerAward||page==MenuPage::Start||page==MenuPage::Garage||page==MenuPage::Tuning||page==MenuPage::Main||page==MenuPage::SinglePlayer||page==MenuPage::Transmission||page==MenuPage::ClassSelect||page==MenuPage::Pause||page==MenuPage::Confirm;
 if(horizontal){
  auto positions=horizontal_positions(clock,k);
  for(size_t i=0;i<items.size();++i){
   const float x=positions[i].first,w=positions[i].second;
   if(x+w<=8||x>=632)continue;
   const float fade=ui_menu_edge_alpha(x+w*.5f);
   const bool active=int(i)==row;
   if(active&&page!=MenuPage::Main){
    const float left=std::max(8.f,x),right=std::min(632.f,x+w);
    rect(left,427,right-left,1,(uint32_t(255*fade)<<24)|0x00ffd442);
   }
   const auto metrics=ui_menu_label(ui_localize(items[i],language),k);
   FrontendDraw d;d.kind=FrontendDraw::Text;d.value=metrics.text;
   d.x=ox+(x+10)*k;d.y=float(safe.map_y(404))-(page==MenuPage::Main?3.f:0.f);
   d.w=metrics.text_width*k;d.h=float(metrics.pixels);d.scale=metrics.scale;d.heading=true;
   d.color=(uint32_t(255*fade*(active?.8f+.2f*pulse:.72f))<<24)|(active?0x00ffd442:0x00aeb4bc);
   d.clip_left=ox+8*k;d.clip_right=ox+632*k;
   if(!d.value.empty())out.push_back(d);
  }
 }

 color=0xffffffff;
 if(page==MenuPage::Controls){
  constexpr float x=343,y=159;
  color=xbox.connected?0xff94dfac:0xffb4bac4;
  label(343,159,xbox.connected?"XBOX CONNECTED":"XBOX NOT CONNECTED",FontStyle::Regular10,1.4f);
  color=0xffffffff;
  img("hud_assets/xbox_controller.tga",x,y,230,138);
  auto indicator=[&](float dx,float dy,const char* text,uint16_t button,uint32_t tint){
   const bool on=(xbox.buttons&button)!=0;
   rect(x+dx-6,y+dy-6,12,12,on?tint:0xff1b1e24);
   color=on?0xff101115:tint;
   label(x+dx-3,y+dy+3,text,FontStyle::Regular10,1.12f);
  };
  indicator(172,52,"Y",0x8000,0xffffd452);indicator(183,63,"B",0x2000,0xfffa7471);
  indicator(172,74,"A",0x1000,0xff8cdf9b);indicator(161,63,"X",0x4000,0xff81bfff);
  for(auto [dx,dy,mask]:{std::tuple{76,80,1},std::tuple{76,96,2},std::tuple{68,88,4},std::tuple{85,88,8},
                         std::tuple{105,63,32},std::tuple{125,63,16}})
   if(xbox.buttons&mask)rect(x+dx-2,y+dy-2,4,4,0xffffd442);
  auto stick=[&](float dx,float dy,float px,float py,bool pressed){
   rect(x+dx+px*10-3,y+dy-py*10-3,6,6,pressed?0xffffd442:0xffc8d0da);
  };
  stick(53,60,xbox.left_x,xbox.left_y,(xbox.buttons&0x40)!=0);
  stick(146,84,xbox.right_x,xbox.right_y,(xbox.buttons&0x80)!=0);
  for(int i=0;i<2;++i){
   const float dx=i?162.f:26.f;
   const float trigger=i?xbox.right_trigger:xbox.left_trigger;
   rect(x+dx+3,y+13,36,4,0xff313841);
   rect(x+dx+3,y+13,36*trigger,4,0xffffd442);
   color=0xffc8d0da;
   label(x+dx+13,y+25,i?"RT":"LT",FontStyle::Regular10,1.1f);
   if(xbox.buttons&(i?0x0200:0x0100))rect(x+dx+3,y+32,36,3,0xffffd442);
  }
  rect(348,299,221,3,0xff555b64);rect(457,296,3,9,0xff9098a2);
  rect(457+xbox.steering*105,297,4,7,0xffffd442);
  color=0xffe0e3e7;
  label(344,316,"LS: STEER   LT: BRAKE   RT: GAS",FontStyle::Regular10,1.3f,229);
  label(344,333,"A: NITRO   B: RECOVER   X: HANDBRAKE",FontStyle::Regular10,1.3f,229);
  label(344,350,"Y: CAMERA   LB/RB: SHIFT",FontStyle::Regular10,1.3f,229);
  label(344,367,"START: PAUSE",FontStyle::Regular10,1.3f,229);
 }
 if(!lan_page()&&!horizontal&&page!=MenuPage::CarSelect&&page!=MenuPage::RaceSetup&&page!=MenuPage::AlphaTracks&&page!=MenuPage::Upgrade&&page!=MenuPage::Profile&&page!=MenuPage::Credits)
  rect(48,123,544,1,0xff43464c);
 if(page==MenuPage::CareerAward){
  rect(0,63,640,330,0xff28292c);
  const int place=last_career_result.value("place",0);
  const bool dnf=last_career_result.value("dnf",false),podium=!dnf&&place>=1&&place<=3;
  if(podium)art("trophies","goldtrophy",47,116,140,193);
  color=0xffffce40;
  label(220,108,last_career_result.value("campaign_complete",false)?"CAREER COMPLETE!":podium?"CONGRATULATIONS!":"RACE COMPLETE",FontStyle::Italic24);
  color=0xffffffff;
  label(220,142,dnf?"DNF":std::to_string(place)+(place==1?"st PLACE":place==2?"nd PLACE":place==3?"rd PLACE":"th PLACE"),FontStyle::Italic24);
  const int total=last_career_result.value("award",0), bonus=last_career_result.value("prop_bonus",0);
  color=0xffffce40;label(220,182,"RACE AWARD",FontStyle::Regular18,1.5f,235);
  color=0xffffffff;label(465,182,"$"+std::to_string(last_career_result.value("race_prize",total-bonus)),FontStyle::Regular18);
  art("smashysmash","bonus_roadblock",186,193,24,24);
  color=0xffffce40;label(220,212,"OBJECT BONUS",FontStyle::Regular18,1.5f,176);
  color=0xffffffff;label(401,212,"×"+std::to_string(last_career_result.value("prop_hits",0)),FontStyle::Regular18,1.5f,59);
  label(465,212,"$"+std::to_string(bonus),FontStyle::Regular18);
  color=0xffffce40;label(220,250,"TOTAL AWARD",FontStyle::Italic24,1.5f,235);
  color=0xffffffff;label(465,250,"$"+std::to_string(total),FontStyle::Italic24);
  if(last_career_result.value("class_unlocked",0)>0){color=0xffffce40;label(220,290,"NEW CLASS UNLOCKED",FontStyle::Regular18);}
  else if(last_career_result.contains("unlocked_levels")&&!last_career_result["unlocked_levels"].empty()){
   int level=last_career_result["unlocked_levels"][0];color=0xffffce40;label(220,290,"YOU HAVE UNLOCKED",FontStyle::Regular18);
   color=0xffffffff;for(const auto&t:track_slots)if(t.level==level){label(220,321,t.name,FontStyle::Regular18,1.5f,0,false);break;}
  }
  color=0xffffffff;
 }
 if(page==MenuPage::SmashBonus){
  // resultsmenu.bed::menu_smashresult and smashyblock: 3/3/2, the
  // original category order, atlas icons and translucent native bars.
  rect(-ox/k,63,float(width)/k,330,0xff646158);
  constexpr const char* icons[]={"bonus_roadblock","bonus_fence","bonus_machinery","bonus_signs","bonus_accessory","bonus_nature","bonus_structure","bonus_tire"};
  constexpr float xs[]={54,222,403},ys[]={118,182,248};
  const auto hits=last_career_result.value("prop_hits_by_type",std::array<int,10>{});
  const auto cents=last_career_result.value("prop_cents_by_type",std::array<int,10>{});
  for(int i=0;i<8;++i){
   float x=xs[i%3],y=ys[i/3];color=0xffffffff;
   art("smashysmash","balk",x-27,y+8,190,42);
   art("smashysmash",icons[i],x,y,56,55);
   color=0xffffce40;label(x+62,y+26,"#",FontStyle::Condensed12,1.5f,40,false);
   label(x+105,y+26,"TOTAL",FontStyle::Condensed12,1.5f,55);
   color=0xffffffff;label(x+62,y+45,std::to_string(hits[i+1]),FontStyle::Regular18,1.2f,40,false);
   label(x+105,y+45,"$"+std::to_string(smash_dollars(cents[i+1])),FontStyle::Regular18,1.2f,55,false);
  }
  color=0xffffce40;label(199,338,"SMASH BONUS TOTAL",FontStyle::Italic24,1.125f,425);
  color=0xffffffff;label(199,368,"$"+std::to_string(last_career_result.value("prop_bonus",0)),FontStyle::Italic24);
 }
 if(page==MenuPage::CarSelect&&!cars.empty()){
  if(career){art("common","rahaikoni",410,31,25,23);source_text(440,31,"$"+std::to_string(money),FontStyle::Italic14);color=0xffffcc40;source_text(48,82,career_progress.car?"TRADE IN: $"+std::to_string(career_progress.car_price/2):"BUY YOUR FIRST CAR",FontStyle::Regular18);color=0xffffffff;}
  const auto&car=cars[car_index];rect(47,130,256,2,0xffffd442);
  if(career&&career_progress.car!=car.number&&!career_car_unlocked(car.car_class)){
   color=0xffffa482;
   label(55,387,"FINISH PREVIOUS CLASS",FontStyle::Regular10);color=0xffffffff;
  }
  // Logo rectangles and atlas dimensions remain in BED source coordinates even
  // when the DDS is remastered at higher resolution. Fit without stretching.
  if(car.logo&&car.logo.atlas_width>0&&car.logo.atlas_height>0&&assets&&assets->exists(car.logo.texture)){
   float fit=std::min(230.f/car.logo.width,32.f/car.logo.height);
   float w=car.logo.width*fit,h=car.logo.height*fit;
   artwork(car.logo,60,136+(32-h)*.5f,w,h);
  }else label(60,151,car.name,FontStyle::Regular18,1.5f,230,false);
  color=0xffc8cbd0;std::istringstream words(ui_localize(car.description,language));std::string word,line;float text_y=170;
  while(words>>word){auto next=line.empty()?word:line+" "+word;if(!line.empty()&&ui_text_width_scaled(FontStyle::Regular10,next,k)>230*k){source_text(60,text_y,line,FontStyle::Regular10);text_y+=15;line=word;}else line=next;}if(!line.empty())source_text(60,text_y,line,FontStyle::Regular10);
  const bool alpha=car.car_class==alpha_car_class;
  color=0xffaeb4bc;source_text(60,269,"POWER",FontStyle::Regular10);source_text(60,297,"WEIGHT",FontStyle::Regular10);
  color=0xffffffff;source_text(155,269,std::to_string(int(car.stats.horsepower))+"hp",FontStyle::Condensed12);source_text(155,297,std::to_string(int(car.stats.weight))+"kg",FontStyle::Condensed12);
  if(alpha){color=0xffffd442;label(60,344,"ALPHA CONTENT",FontStyle::Regular18,1.5f,230);}
  else{color=0xffaeb4bc;source_text(60,325,"PRICE",FontStyle::Regular10);color=0xffffffff;source_text(155,325,"$"+std::to_string(car.stats.price),FontStyle::Italic14);}
  for(size_t i=0;i<car.skins.size();++i){float y=183+float(i)*19;const auto&skin=car.skins[i];unsigned alpha=skin.number==choice.skin?255:150;uint32_t tint=0x00ffffff;if(skin.has_colors){const auto&c=skin.colors[0];tint=uint32_t(c[0]*255)<<16|uint32_t(c[1]*255)<<8|uint32_t(c[2]*255);}color=(alpha<<24)|tint;art("common","color_glassblock",557,y,33,16);if(skin.number>=101){color=skin.number==choice.skin?0xffffd442:0xffc8cbd0;label(511,y+12,"AI "+std::to_string(skin.number-100),FontStyle::Regular10,1.2f,42);}}
  if(career){color=0xffffcc40;source_text(400,345,career_progress.car==car.number?"USE CAR":"BUY / TRADE",FontStyle::Italic24);color=0xffffffff;}
  color=0xffffffff;if(!alpha){art("common","nuoliylos",564,165,21,14);art("common","nuolialas",564,319,21,14);}
  const int count=int(car_selection_count());
  if(count){
   if(car_scroll.context==int(page)){while(car_scroll.value-float(car_index)>count*.5f)car_scroll.value-=float(count);while(float(car_index)-car_scroll.value>count*.5f)car_scroll.value+=float(count);}
   const float visual_car=car_scroll.update(float(car_index),now,int(page));
   for(int d=-std::min(3,count/2);d<=std::min(3,(count-1)/2);++d){
    const int i=int(car_index)+d,slot=(i+count)%count;const float x=280+(i-visual_car)*109.f;selected(d==0);
    if(d==0)art("car_icons","circle",x+11,380,57,49);
    const auto& item=cars[size_t(slot)];
    if(item.car_class==alpha_car_class)artwork(item.icon,x+11,392,57,26.5f);
    else artwork(item.icon,x+11,380,57,49);
   }
  }
 }
 color=0xffffffff;
 if(lan_page()){
  constexpr uint32_t gold=0xffffc800;
  auto focus=[&](int index){color=index==row?gold:0xffffffff;};
  auto footer=[&](int slot,int index){
   const auto hit=lan_footer_item(slot);focus(index);
   constexpr float scale=1.f;
   const float text_width=std::min(hit.right-hit.left-20,ui_text_width_scaled(FontStyle::Italic24,items[index],scale/1.5f*k)/k);
   label((hit.left+hit.right-text_width)*.5f,420,items[index],FontStyle::Italic24,scale,hit.right-hit.left-20,false);
  };
  if(page==MenuPage::LanStart){
   color=0xffe6e6e6;label(70,132,"LAN / DNS",FontStyle::Regular10);
   for(size_t i=0;i<items.size();++i){
    focus(int(i));label(70,lan_start_item(int(i)).top+19,items[i],FontStyle::Regular18,1.32f,496,false);
   }
  }else if(page==MenuPage::LanRooms){
   // lanresults.bed has separate game-name and population columns.
   color=gold;label(3,89,"GAME",FontStyle::Regular10,1.5f,143);
   label(150,89,"PLAYERS",FontStyle::Regular10,1.5f,90);
   const int rooms=room_lookup.valid()?0:int(online_rooms.size());
   const int first=lan_room_first(row,rooms);
   if(room_lookup.valid()){color=0xffffffff;label(3,121,items.front(),FontStyle::Regular10,1.5f,239,false);}
   for(int i=first;i<std::min(first+lan_room_page_size,rooms);++i){
    const auto& room=online_rooms[i];const auto hit=lan_room_item(i-first);
    focus(i);if(i!=row&&!room.open)color=0xffb4bac4;
    label(3,hit.top+15,single_line(room.name),FontStyle::Regular10,1.5f,141,false);
    label(153,hit.top+15,std::to_string(room.players)+"/"+std::to_string(lan_max_players),FontStyle::Regular10,1.5f,90,false);
   }
   if(row>=0&&row<rooms){
    const auto& room=online_rooms[row];
    const auto track=std::find_if(track_slots.begin(),track_slots.end(),[&](const auto& t){return t.stage==room.stage;});
    color=gold;label(258,99,"TRACK",FontStyle::Regular10);
    color=0xffffffff;label(258,113,track==track_slots.end()?single_line(room.stage):track->name,FontStyle::Regular10,1.5f,230,false);
    if(!room.open){color=gold;label(258,145,"RACE IN PROGRESS",FontStyle::Regular10);}
   }
   footer(1,int(items.size())-1);
  }else if(lan&&lan->phase()==LanSession::Phase::Lobby){
   // lanlobby.bed: native ready/name/car table, eight 30-pixel rows and two rules.
   rect(-ox/k,80,float(width)/k,2,gold);
   rect(-ox/k,339,float(width)/k,2,gold);
   color=gold;label(5,76,"READY",FontStyle::Regular10,1.5f,55);
   label(65,76,"NAME",FontStyle::Regular10);label(243,76,"CAR",FontStyle::Regular10);
   const auto& check=find_artwork("menucommon","checked");
   for(size_t i=0;i<std::min(lan->players().size(),size_t(lan_max_players));++i){
    const auto& player=lan->players()[i];const float y=85+bed_settings.lobby.roster_row_height*float(i);
    color=0xffffffff;
    if(player.ready){if(check)artwork(check,11,y+8,20,20);else label(11,y+21,"+",FontStyle::Regular10,1.5f,20,false);}
    label(65,y+20,single_line(player.name),FontStyle::Regular10,1.5f,167,false);
    const auto car=std::find_if(cars.begin(),cars.end(),[&](const auto& entry){return entry.number==player.car_id;});
    label(243,y+20,car==cars.end()?"#"+std::to_string(player.car_id):car->name,FontStyle::Regular10,1.5f,102,false);
   }
   const auto track=lan_track();const auto values=lan_setting_values(track);
   const char* settings[]={"MODE","TRACK","LAPS","CAR","PAINT"};
   for(int i=0;i<5;++i){
    const auto hit=lan_setting_item(i,bed_settings.lobby);
    color=gold;label(365,hit.top+11,settings[i],FontStyle::Regular10);
    focus(i);label(365,hit.top+26,single_line(values[i]),FontStyle::Regular10,1.5f,145,false);
    if(i>=3||(lan->is_host()&&(i!=2||lan->rules().mode==RaceMode::Circuit))){
     const auto& left=find_artwork("menucommon","mininuolivasen");
     const auto& right=find_artwork("menucommon","mininuolioikea");
     if(left&&right){artwork(left,355,hit.top+15,9,13);artwork(right,517,hit.top+15,9,13);}
     else{label(355,hit.top+26,"<",FontStyle::Regular10);label(517,hit.top+26,">",FontStyle::Regular10);}
    }
   }
   color=0xffffffff;
   if(track)art("track_icons",track->icon,540,96,64,37);
   color=gold;label(365,268,"PLAYERS",FontStyle::Regular10);
   color=0xffffffff;label(365,283,std::to_string(lan->players().size())+"/"+std::to_string(lan_max_players),FontStyle::Regular10,1.5f,100,false);
   color=lan_talking?0xff8ddd9f:0xffffffff;
   label(5,357,lan_talking?"VOICE TRANSMITTING":"F8: PUSH TO TALK",FontStyle::Regular10,1.5f,340);
   if(lan->is_host()&&lan->room_name().empty()){
    color=0xffffffff;label(493,357,ui_localize("LAN PORT",language)+" "+std::to_string(lan->port()),FontStyle::Regular10,1.5f,130,false);
   }
   footer(0,5);
   if(lan->is_host())footer(1,6);
   footer(2,int(items.size())-1);
  }else{
   if(items.size()>1){color=0xffffffff;label(65,119,items.front(),FontStyle::Regular18,1.5f,520,false);}
   footer(1,int(items.size())-1);
  }
  color=0xffffffff;
 }
 if(page==MenuPage::AlphaTracks){
  rect(48,108,544,2,0xffffd442);
  color=0xffaeb4bc;label(78,135,"FREE DRIVE",FontStyle::Regular10);
  for(size_t i=0;i<alpha_tracks.size();++i){
   const float x=alpha_card_left(i),y=alpha_card_top;const bool active=int(i)==row;
   rect(x,y,alpha_card_width,active?3.f:1.f,active?0xffffd442:0xff43464c);
   color=active?0xffffffff:0xffb0b0b0;
   img(alpha_tracks[i].preview,x+6,y+15,218,218*104.f/446.f,0,0,446.f/512.f,104.f/128.f);
   color=active?0xffffd442:0xffe6e6e6;
   label(x+11,y+105,alpha_tracks[i].name,FontStyle::Regular18,1.5f,alpha_card_width-22);
  }
  color=0xffaeb4bc;label(78,315,"CHOOSE A TRACK TO DRIVE",FontStyle::Regular10);
 }
 if(page==MenuPage::RaceSetup){
  if(selected_class<4)img("data/menu/class_"+std::string(1,char('d'-selected_class))+"_fork.dds",78,161,selected_class==1?256:512,256);
  for(auto i:class_tracks()){const auto&t=track_slots[i];bool locked=career&&!career_event_unlocked(t.level);selected(i==track_slot);art(locked?"track_icons_inactive":"track_icons",t.icon,t.x,t.y,64,37);color=0xffffffff;if(career){int pos=career_progress.position(t.level);if(pos>=1&&pos<=3)completion_medal(pos,t.x+46,t.y+19,18);}if(i==track_slot){selected(true);art("track_items","selector",t.x-2,t.y-2,68,41);color=0xffffffff;}}
  if(track_slot<track_slots.size()&&!track_slots[track_slot].name_icon.empty()){
   const auto&id=track_slots[track_slot].name_icon;auto key="track_name_icons/"+id;
   auto found=artwork_cache.find(key);if(found==artwork_cache.end())found=artwork_cache.emplace(key,assets?menu_artwork(*assets,"track_name_icons",id):MenuArtwork{}).first;
   const auto&logo=found->second;if(logo){float fit=std::min(190.f/logo.width,43.f/logo.height);float w=logo.width*fit,h=logo.height*fit;artwork(logo,396,70+(43-h)*.5f,w,h);}
  }
  rect(383,113,217,2,0xffffd442);
  label(394,139,names.count(choice.track)?names.at(choice.track):fs::path(choice.track).parent_path().parent_path().filename().string(),FontStyle::Regular18,1.5f,194,false);
  const int setting_rows[]={1,2,4,5,6};
  const char* ai=choice.ai_handicap_level<=2?"EASY":choice.ai_handicap_level<=5?"NORMAL":"HARD";
  const std::string setting_names[]={mode()==RaceMode::Battle||stunt_mode()?"MODE":"LAPS","OPPONENTS","TIME OF DAY","WEATHER","AI DIFFICULTY"};
  const std::string setting_values[]={mode()==RaceMode::Battle?"DERBY":stunt_mode()?"STUNT":std::to_string(choice.laps),choice.time_trial||stunt_mode()?"NONE":std::to_string(choice.bots),choice.environment.night?"NIGHT":"DAY",choice.environment.rain?"RAIN":"DRY",std::string(ai)+" ("+std::to_string(choice.ai_handicap_level)+")"};
  for(int i=0;i<5;++i){float y=149+i*26;if(row==setting_rows[i])rect(392,y,2,25,0xffffd442);color=0xffaeb4bc;label(398,y+16,setting_names[i],FontStyle::Regular10,1.2f,95);color=row==setting_rows[i]?0xffffd442:0xffffffff;label(497,y+16,"< "+setting_values[i]+" >",FontStyle::Regular10,1.35f,90);}
  rect(396,286,190,1,0xff393d44);color=0xffaeb4bc;label(396,303,"PRIZES",FontStyle::Regular10);
  const MenuTrack*t=track_slot<track_slots.size()?&track_slots[track_slot]:nullptr;for(int i=0;i<3;++i){float x=396+i*64;completion_medal(i+1,x,310,15);color=0xffffffff;label(x,344,"$"+std::to_string(t?t->prizes[i]:0),FontStyle::Regular10,1.1f,60);}
  color=0xffaeb4bc;label(396,374,"TAB / D-PAD: SELECT   LEFT / RIGHT: CHANGE",FontStyle::Regular10,.85f,192);
  color=row==3?0xffffcc40:0xffe6e6e6;label(260,418,career&&!career_event_unlocked(choice.level)?"LOCKED":"START RACE",FontStyle::Regular18,1.5f,145);
  if(career){color=0xffffcc40;label(65,100,"TOP THREE UNLOCKS THE NEXT STAGE",FontStyle::Regular10);if(t){int pos=career_progress.position(t->level);color=0xffffffff;label(396,359,career_event_unlocked(t->level)?(pos?"BEST PLACE: "+std::to_string(pos):"NOT COMPLETED"):"PREVIOUS STAGE TOP THREE",FontStyle::Regular10);}}

 }
 if(!lan_page()&&!horizontal&&page!=MenuPage::CarSelect&&page!=MenuPage::RaceSetup&&page!=MenuPage::AlphaTracks&&page!=MenuPage::Upgrade&&page!=MenuPage::Profile){size_t first=(page==MenuPage::Controls||page==MenuPage::TrackProjects)?size_t(row/6)*6:page==MenuPage::GameOptions?size_t(row/7)*7:0,last=(page==MenuPage::Controls||page==MenuPage::TrackProjects)?std::min(first+6,items.size()):page==MenuPage::GameOptions?std::min(first+7,items.size()):items.size();for(size_t i=first;i<last;++i){if(int(i)==row){float y=180+float(i-first)*(page==MenuPage::GameOptions?25:32);rect(57,y-21,3,page==MenuPage::GameOptions?24:30,0xffffd442);}color=int(i)==row?0xffffd442:0xffe6e6e6;label(70,180+(i-first)*(page==MenuPage::GameOptions?25:32),items[i],FontStyle::Regular18,1.5f,((page==MenuPage::GameOptions&&i==5)||(page==MenuPage::Options&&i<3)||(page==MenuPage::Controls&&(i==ControlSettings::Count||i==ControlSettings::Count+1)))? (page==MenuPage::Controls?136:286):(page==MenuPage::Controls?248:500));if((page==MenuPage::GameOptions&&i==5)||(page==MenuPage::Options&&i<3)||(page==MenuPage::Controls&&(i==ControlSettings::Count||i==ControlSettings::Count+1))){double t=page==MenuPage::GameOptions?bloom_intensity/2.:page==MenuPage::Options?(i==0?music_volume:i==1?race_music_volume:effects_volume)/100.:i==ControlSettings::Count?(controls.deadzone-.05)/.35:(controls.sensitivity-.5)/1.5;float y=169+float(i-first)*(page==MenuPage::GameOptions?25:32);const float slider_x=page==MenuPage::Controls?211.f:370.f,slider_width=page==MenuPage::Controls?104.f:196.f;rect(slider_x,y,slider_width,4,0xff555b64);rect(slider_x,y,float(t)*slider_width,4,0xffffd442);rect(slider_x-3+float(t)*slider_width,y-4,6,12,int(i)==row?0xffffd442:0xffc8cbd0);}}if(page==MenuPage::Options){color=0xffaeb4bc;label(70,132,"PGUP/PGDN / LB/RB: TABS   ENTER/A: SELECT   ESC/B: BACK",FontStyle::Regular10);}if(page==MenuPage::Controls){color=0xffffffff;label(70,130,"UP/DOWN: SELECT   ENTER: REBIND   PGUP/PGDN / LB/RB: TABS",FontStyle::Regular10);}if(page==MenuPage::GameOptions){color=0xffaeb4bc;label(70,132,"UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   PGUP/PGDN / LB/RB: TABS",FontStyle::Regular10);label(500,132,std::to_string(first+1)+"-"+std::to_string(last)+" / "+std::to_string(items.size()),FontStyle::Regular10);}}
 color=0xffffffff;
 if((page==MenuPage::Tuning||page==MenuPage::Upgrade)&&!cars.empty()){
  rect(48,124,256,2,0xffffd442);auto ids=category_upgrades();if(!ids.empty()){auto&u=upgrades[ids[upgrade_index%ids.size()]];label(56,144,page==MenuPage::Upgrade?u.title:items[std::clamp(tuning_category,0,5)],FontStyle::Condensed12,1.5f,238);
   std::istringstream words(ui_localize(u.description,language));std::string word,line;float y=166;while(words>>word){auto next=line.empty()?word:line+" "+word;if(!line.empty()&&ui_text_width_scaled(FontStyle::Regular10,next,k)>221*k){source_text(58,y,line,FontStyle::Regular10);y+=15;line=word;if(y>292)break;}else line=next;}if(y<=292&&!line.empty())source_text(58,y,line,FontStyle::Regular10);
   if(page==MenuPage::Upgrade){auto own=car_upgrades(cars[car_index].number);bool installed=std::find(own.begin(),own.end(),u.id)!=own.end();source_text(116,333,installed?"INSTALLED":!upgrade_available(ids[upgrade_index%ids.size()])?"UNAVAILABLE":"$"+std::to_string(u.prices[std::clamp(cars[car_index].car_class-1,0,2)]),FontStyle::Italic14);if(part_scroll.context==int(page)){while(part_scroll.value-float(upgrade_index)>ids.size()*.5f)part_scroll.value-=float(ids.size());while(float(upgrade_index)-part_scroll.value>ids.size()*.5f)part_scroll.value+=float(ids.size());}float visual_part=part_scroll.update(float(upgrade_index),now,int(page));for(int d=-std::min(3,int(ids.size()/2));d<=std::min(3,int((ids.size()-1)/2));++d){int i=int(upgrade_index)+d;int slot=(i+int(ids.size()))%int(ids.size());selected(d==0);art("car_icons","circle",291+(i-visual_part)*91.f,380,57,49);art("tuning_icons",upgrades[ids[slot]].icon,291+(i-visual_part)*91.f,380,57,49);}}
  }
 }
 color=0xffffffff;
 if(page==MenuPage::Profile){
  rect(48,130,540,2,0xffffd442);
  if(!profile_delete_id.empty()){
   auto target=std::find_if(player_profiles.begin(),player_profiles.end(),[&](const auto&p){return p.id==profile_delete_id;});
   color=0xffffce40;label(80,184,"DELETE THIS PROFILE?",FontStyle::Italic24);
   color=0xffffffff;
   if(target!=player_profiles.end())label(80,224,target->name,FontStyle::Regular18,1.2f,480,false);
   color=profile_delete_yes?0xffe6e6e6:0xffffd442;label(104,275,"CANCEL",FontStyle::Regular18);
   color=profile_delete_yes?0xffffd442:0xffffb0a0;label(332,275,"DELETE PROFILE",FontStyle::Regular18);
  }else if(profile_editing){
   img(driver_model_portrait(driver_model_from_id(profile_draft_gender),assets&&assets->exists("alpha_content/driver/johnny_portrait.tga")),368,140,208,208);
   label(68,165,"NEW PROFILE",FontStyle::Italic24);
   for(size_t i=0;i<items.size();++i){
    color=int(i)==row?0xffffd442:0xffe6e6e6;
    label(68,196+float(i)*32,i==0?ui_localize("NAME: ",language)+profile_draft+"_":items[i],FontStyle::Regular18,1.5f,285,i!=0);
   }
   color=0xffaeb4bc;label(68,344,"UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   ENTER: CREATE",FontStyle::Regular10);
  }else{
   const size_t first=size_t(std::max(0,row)/5)*5;
   for(size_t i=first;i<std::min(first+5,items.size());++i){
    color=int(i)==row?0xffffce40:0xffe6e6e6;
    label(68,164+float(i-first)*34,i<player_profiles.size()?player_profiles[i].name+(player_profiles[i].id==profile_id?ui_localize("  [ACTIVE]",language):""):items[i],FontStyle::Regular18,1.5f,i<player_profiles.size()?400:500,i>=player_profiles.size());
   }
   color=0xffffffff;label(68,344,"Each player has a separate career save",FontStyle::Regular10);
   for(size_t i=first;i<std::min(first+5,player_profiles.size());++i){
    const float y=140+34*float(i-first);
    color=0xffffb0a0;label(491,y+21,"DELETE PROFILE",FontStyle::Regular10,1.0f,85);
   }
  }
 }
 if(page==MenuPage::Options||page==MenuPage::GameOptions||page==MenuPage::Controls){label(120,414,"GAME",FontStyle::Italic24,1);label(300,414,"AUDIO",FontStyle::Italic24,1);label(450,414,"CONTROLS",FontStyle::Italic24,1);}
 if(page!=MenuPage::Start){art("pc_gui_icons","esc",350,437,28,28);art("pc_gui_icons","enter",453,437,28,28);label(379,455,"BACK",FontStyle::Regular18,.9f);label(482,455,"SELECT",FontStyle::Regular18,.9f);}
 if(!message.empty()||!controller_hint.empty()){color=0xffffd966;label(300,16,message.empty()?controller_hint:message,FontStyle::Regular10,1.05f,328);}
 return out;
}
std::vector<FrontendDraw> Frontend::replay_commands(int width,int height,double position,double duration,double rate,bool paused,int selected)const{
 std::vector<FrontendDraw> out;if(width<=0||height<=0)return out;float k=std::min(width/1280.f,height/720.f),ox=(width-1280*k)*.5f,oy=(height-720*k);
 FrontendDraw panel;panel.x=ox+170*k;panel.y=oy+606*k;panel.w=940*k;panel.h=100*k;panel.color=0xcc15171b;out.push_back(panel);
 auto stamp=[](double t){t=std::max(0.,t);int seconds=int(t);std::string sec=std::to_string(seconds%60);return std::to_string(seconds/60)+":"+(sec.size()==1?"0":"")+sec;};
 auto text=[&](float x,float y,const std::string&value,bool highlighted){FrontendDraw d;d.kind=FrontendDraw::Text;d.x=ox+x*k;d.y=oy+y*k;d.scale=.65f*k;d.color=highlighted?0xffffd050:0xffffffff;d.value=ui_localize(value,language);out.push_back(d);};
 text(190,617,"REPLAY  "+stamp(position)+" / "+stamp(duration)+"  "+(paused?"PAUSED":std::to_string(rate)+"x"),true);
 const char*commands[]={"EXIT","RESTART",paused?"PLAY":"PAUSE","FAST","SLOW"};const char*icons[]={"eject","rewind",paused?"play":"pause","ffwd","slowmo"};
 for(int i=0;i<5;++i){auto art=assets?menu_artwork(*assets,"replay_napit",std::string(icons[i])+(i==selected?"_on":"_off")):MenuArtwork{};if(art){FrontendDraw d;d.kind=FrontendDraw::Image;d.x=ox+(217+i*180)*k;d.y=oy+655*k;d.w=art.width*1.2f*k;d.h=art.height*k;d.value=art.texture;d.u=float(art.x)/art.atlas_width;d.v=float(art.y)/art.atlas_height;d.uw=float(art.width)/art.atlas_width;d.vh=float(art.height)/art.atlas_height;out.push_back(d);}text(242+i*180,653,commands[i],i==selected);}
 return out;
}
}
