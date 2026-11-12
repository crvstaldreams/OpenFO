#include "frontend_hud.hpp"
#include "ui_layout.hpp"
#include "ui_ttf.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace ofo {
namespace {
float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
std::string stamp(double t){if(!std::isfinite(t)||t<0)return "--:--.---";auto ms=static_cast<long long>(std::min(t,599999.999)*1000+.5);char b[64];std::snprintf(b,sizeof b,"%02lld:%02lld.%03lld",ms/60000,(ms/1000)%60,ms%1000);return b;}
std::string race_stamp(double t){
 if(!std::isfinite(t)||t<0)return "--'--''--";
 auto cs=static_cast<long long>(std::min(t,599999.99)*100+.5);
 char b[48];std::snprintf(b,sizeof b,"%02lld'%02lld''%02lld",cs/6000,(cs/100)%60,cs%100);return b;
}
std::string ordinal(int position,UiLanguage language){
 int n=std::max(1,position);auto value=std::to_string(n);
 if(language==UiLanguage::Ukrainian)return value+(n==1?"-ше":n==2?"-ге":n==3?"-тє":"-те");
 int last=n%10,teen=n%100;
 return value+(teen>=11&&teen<=13?"th":last==1?"st":last==2?"nd":last==3?"rd":"th");
}

}
std::vector<FrontendDraw> FrontendHud::commands(int width,int height,const RaceOverlayState&s)const {
 std::vector<FrontendDraw> out;if(width<=0||height<=0)return out;
 const HudLayout layout(width,height);const float k=layout.scale,dx=layout.extra_x,dy=layout.extra_y;
 float shift=dx*.5f,vertical=dy*.5f,opacity=1;
 auto fade=[&](uint32_t c){return (c&0xffffff)|(uint32_t(std::clamp(opacity,0.f,1.f)*float(c>>24)+.5f)<<24);};
 auto image=[&](const std::string&path,float x,float y,float w,float h,uint32_t color=0xffffffff,float u=0,float v=0,float uw=1,float vh=1){if(!path.empty()&&!r_.exists(path))return;FrontendDraw d;d.kind=path.empty()?FrontendDraw::Rect:FrontendDraw::Image;d.x=(x+shift)*k;d.y=(y+vertical)*k;d.w=w*k;d.h=h*k;d.value=path;d.color=fade(color);d.u=u;d.v=v;d.uw=uw;d.vh=vh;out.push_back(d);};
 auto rect=[&](float x,float y,float w,float h,uint32_t c){image("",x,y,w,h,c);};
 auto panel=[&](float x,float y,float w,float h){rect(x,y,w,h,0xdc101319);rect(x,y,3,h,0xffffd442);};
 // Every HUD string has an explicit physical slot; width uses the same rounded
 // TrueType raster size as Renderer::text, not an atlas or character count.
 auto text=[&](float x,float y,float w,const std::string&value,float size=18.f,bool accent=false,bool right=false,bool translate=true){FrontendDraw d;d.kind=FrontendDraw::Text;d.scale=k*size/18;d.heading=false;d.value=ui_fit_text(translate?ui_localize(value,s.language):value,w*k,d.scale);float used=ui_text_width_pixels(ui_raster_pixels(d.scale),false,d.value);d.x=(x+shift)*k+(right?std::max(0.f,w*k-used):0.f);d.y=(y+vertical)*k;d.w=w*k;d.h=(size+4)*k;d.color=fade(accent?0xffffd442:0xffeef0f4);out.push_back(d);};
 // Classic overlay: slim amber labels and bold white values without panels.
 // Text shadows use the same measured glyph run so anchors stay aligned.
 auto classic_text=[&](float x,float y,float w,const std::string&value,float size,bool accent,bool right=false,bool center=false){
  FrontendDraw d;d.kind=FrontendDraw::Text;d.heading=!accent;d.scale=k*size/(d.heading?24.f:18.f);
  d.value=ui_fit_text(value,w*k,d.scale,d.heading);
  float used=ui_text_width_pixels(ui_raster_pixels(d.scale,d.heading),d.heading,d.value);
  d.x=(x+shift)*k+(right?std::max(0.f,w*k-used):center?std::max(0.f,(w*k-used)*.5f):0.f);
  d.y=(y+vertical)*k;d.w=w*k;d.h=(size+4)*k;
  auto shadow=d;shadow.x+=k;shadow.y+=k;shadow.color=fade(0xde15110d);out.push_back(std::move(shadow));
  d.color=fade(accent?0xffffcf79:0xffffffff);out.push_back(std::move(d));
 };
 auto local=[&](const std::string&value){return ui_localize(value,s.language);};
 auto object_bonus=[&]{
  return (s.language==UiLanguage::Ukrainian?"БОНУС ЗА ОБ’ЄКТИ: ":"OBJECT BONUS: ")+
   std::string("$")+std::to_string(std::max(0,s.prop_bonus))+"  ("+std::to_string(std::max(0,s.prop_hits))+")";
 };
 auto art=[&](const std::string&key,float cx,float cy,float size,float alpha,bool replay=false){auto atlas=replay?"replay_napit":"race_start_items";auto id=std::string(atlas)+"/"+key;auto it=art_.find(id);if(it==art_.end())it=art_.emplace(id,menu_artwork(r_,atlas,key)).first;auto&a=it->second;if(!a)return false;image(a.texture,cx-a.width*size/2,cy-a.height*size/2,a.width*size,a.height*size,(uint32_t(std::clamp(alpha,0.f,1.f)*255)<<24)|0xffffff,float(a.x)/a.atlas_width,float(a.y)/a.atlas_height,float(a.width)/a.atlas_width,float(a.height)/a.atlas_height);return true;};
 auto digit=[&](double remaining,float cy){int n=std::clamp(int(std::ceil(remaining)),1,3);float age=float(std::ceil(remaining)-remaining);if(!art(n==3?"start_three":n==2?"start_two":"start_one",640,cy,1.5f+.3f*(1-ease(age/.27f)),1-ease((age-.76f)/.24f)))text(610,cy-35,60,std::to_string(n),54,true);};
 const bool uk=s.language==UiLanguage::Ukrainian;
 const bool stunt=s.stunt.kind!=StuntKind::None;
 const auto& event=s.stunt;
 auto score=[&](double v){
  const auto kind=event.kind;
  if(kind==StuntKind::Clown)return "$"+std::to_string(int(v));
  const auto unit=uk?(kind==StuntKind::Bowling?"кег.":kind==StuntKind::Highjump||kind==StuntKind::Longjump?"м":"оч."):stunt_score_unit(kind);
  char digits[40];
  std::snprintf(digits,sizeof digits,kind==StuntKind::Highjump||kind==StuntKind::Longjump?"%.1f":"%.0f",v);
  return std::string(digits)+" "+unit;
 };
 if(s.event_intro&&!s.replay){
  constexpr float x=290,w=700;
  rect(-shift,-vertical,1280+dx,720+dy,0x45080a0d);
  panel(x,161,w,394);
  const auto heading=stunt?(uk?"ТРЮК • 3 СПРОБИ":"STUNT • 3 ATTEMPTS"):
   s.battle?(uk?"ДЕРБІ • БІЙ АВТО":"DERBY • CAR BATTLE"):(uk?"АРЕНА • КІЛЬЦЕВА ГОНКА":"ARENA • CIRCUIT RACE");
  text(x+26,179,w-52,heading,15,true,false,false);
  text(x+26,207,w-52,s.event_name.empty()?event.name:s.event_name,31,false,false,false);
  rect(x+26,251,w-52,1,0xff5b5141);
  auto lines=ui_wrap_text(s.event_hint,(w-52)*k,k*20/18,false,6);
  for(size_t i=0;i<lines.size();++i)text(x+26,268+float(i)*25,w-52,lines[i],20,false,false,false);
  if(!stunt&&!s.battle)
   text(x+26,456,w-52,(uk?"КІЛ   ":"LAPS   ")+std::to_string(s.laps),18,true,false,false);
  if(stunt&&event.thresholds[0]>0){
   text(x+26,435,w-52,uk?"ПОРОГИ НАГОРОД":"AWARD TARGETS",13,true,false,false);
   for(size_t i=0;i<3;++i)
    text(x+26+float(i)*216,456,200,score(event.thresholds[i])+"   $"+std::to_string(event.prizes[i]),17,false,false,false);
  }
  text(x+26,513,w-52,uk?"ENTER / A  •  ДО СТАРТУ":"ENTER / A  •  READY",19,true,false,false);
  return out;
 }
 if(s.countdown>0&&!s.replay){digit(s.countdown,360);return out;}
 if(!s.finished&&!s.replay&&s.elapsed>=0&&s.elapsed<.95){float t=float(s.elapsed);if(!art("go",640,315,1.5f,1-ease((t-.55f)/.4f)))text(570,290,140,"GO!",42,true);}
 if(s.recovery_countdown>0&&!s.replay&&!s.finished){panel(425,224,430,210);text(453,243,374,"RETURNING TO TRACK",24,true);digit(s.recovery_countdown,350);}
 opacity=std::isfinite(s.hud_opacity)?std::clamp(s.hud_opacity,0.f,1.f):0;
 if(stunt&&!s.replay){
  shift=0;vertical=0;
  classic_text(15,13,290,event.name,18,true);
  classic_text(15,40,290,(uk?"СПРОБА ":"ATTEMPT ")+std::to_string(event.attempt+1)+" / 3",24,false);
  classic_text(15,77,290,score(event.score),34,false);
  classic_text(15,124,290,race_stamp(event.elapsed),18,true);
  shift=dx;
  if(!event.airborne&&!event.attempt_done){
   const auto aim_prompt=event.charging?
    (uk?"СПОВІЛЬНЕННЯ • ВІДПУСТИ НІТРО ДЛЯ ЗАПУСКУ":"SLOW MOTION • RELEASE NITRO TO LAUNCH"):
    (uk?"УТРИМУЙ НІТРО, ЩОБ ВИБРАТИ КУТ":"HOLD NITRO TO AIM");
   classic_text(840,13,418,aim_prompt,16,true,true);
   if(event.launch_distance>=-4&&event.launch_distance<85){
    const bool ready=event.launch_distance<=4;
    const auto prompt=ready?(uk?"ВІКНО ЗАПУСКУ":"LAUNCH WINDOW"):
      (uk?"ДО ЗАПУСКУ   ":"TO LAUNCH   ")+std::to_string(std::lround(event.launch_distance))+(uk?" м":" m");
    classic_text(860,40,398,prompt,17,ready,true);
   }
  }
  if(event.attempt_done){
   shift=dx*.5f;vertical=dy*.5f;
   panel(362,216,556,283);
   text(386,238,508,event.finished?(uk?"ТРЮК ЗАВЕРШЕНО":"STUNT COMPLETE"):(uk?"СПРОБУ ЗАВЕРШЕНО":"ATTEMPT COMPLETE"),27,true,false,false);
   rect(386,280,508,1,0xff5b5141);
   for(int i=0;i<3;++i){
    text(386+172.f*i,297,164,(uk?"СПРОБА ":"ATTEMPT ")+std::to_string(i+1),14,true,false,false);
    text(386+172.f*i,320,164,i<=event.attempt?score(event.scores[size_t(i)]):"—",24,false,false,false);
   }
   text(386,370,508,(uk?"РАЗОМ   ":"TOTAL   ")+score(event.total),23,false,false,false);
   if(event.finished&&event.kind!=StuntKind::Clown)
    text(386,407,508,(uk?"НАГОРОДА   $":"AWARD   $")+std::to_string(event.award),18,true,false,false);
   text(386,459,508,event.finished?(uk?"ENTER / A  •  ПОВЕРНУТИСЯ":"ENTER / A  •  RETURN"):(uk?"ENTER / A  •  НАСТУПНА СПРОБА":"ENTER / A  •  NEXT ATTEMPT"),17,true,false,false);
  }
  return out;
 }
 if(s.finished&&!s.replay&&!s.results){
  panel(410,216,460,196);if(s.battle)text(438,246,404,s.health>1.-double(.999f)?(uk?"ПЕРЕМОГА В ДЕРБІ":"DERBY WON"):(uk?"АВТО ЗНИЩЕНО":"CAR WRECKED"),30,true,false,false);
  else if(!art("finish",640,272,1.5f,1))text(438,246,404,"RACE FINISHED",30,true);
  text(438,330,404,object_bonus(),17,true,false,false);text(438,373,404,s.battle?(uk?"ENTER / A  •  РЕЗУЛЬТАТИ БОЮ":"ENTER / A  •  BATTLE RESULTS"):"ENTER / A: race results",18,false,false,!s.battle);
 }
 else if(s.finished&&!s.replay){
  panel(194,135,892,466);text(220,154,840,s.battle?(uk?"РЕЗУЛЬТАТИ БОЮ":"BATTLE RESULTS"):"RACE RESULTS",28,true,false,!s.battle);rect(220,195,840,1,0xff555b65);
  if(s.battle)text(220,208,840,s.health>1.-double(.999f)?(uk?"ПЕРЕМОГА":"VICTORY"):(uk?"АВТО ЗНИЩЕНО":"CAR WRECKED"),18,true,false,false);
  text(220,239,55,"POS",14,true);text(290,239,322,"DRIVER",14,true);text(630,239,215,s.battle?"HEALTH":"CAR",14,true);text(864,239,195,"TIME / STATUS",14,true,true);
  for(size_t i=0;i<std::min<size_t>(s.leaderboard.size(),8);++i){auto&e=s.leaderboard[i];float y=263+float(i)*31;rect(214,y,852,29,e.player?0xf02c3037:0xb0181d24);if(e.player)rect(214,y,3,29,0xffffd442);text(225,y+4,48,std::to_string(e.rank>0?e.rank:int(i+1)),18,e.player);text(290,y+4,322,e.name,18,e.player,false,false);text(630,y+4,215,s.battle?std::to_string(e.lap)+" %":e.car_name,17,false,false,false);text(864,y+4,190,s.battle?e.status:e.finished?stamp(e.time):(!e.status.empty()?e.status:"RACING"),17,false,true);}
  text(220,517,840,object_bonus(),17,true,false,false);
  if(!s.result_detail.empty())text(220,541,840,s.result_detail,17,true);
  text(220,568,840,s.finish_prompt,16);
 }
 if(!s.replay&&!s.finished){
  shift=0;vertical=0;
  if(s.battle){
   classic_text(15,13,250,uk?"ЧАС БОЮ":"BATTLE TIME",18,true);
   classic_text(15,36,250,race_stamp(s.elapsed),31,false);
   shift=dx;
   classic_text(1000,13,260,uk?"ВИЖИЛО":"SURVIVORS",18,true,true);
   classic_text(1000,36,260,std::to_string(s.survivors)+" / "+std::to_string(s.participants),34,false,true);
   classic_text(1000,88,260,(uk?"СТАН АВТО   ":"HEALTH   ")+std::to_string(int(std::clamp(s.health,0.,1.)*100))+"%",18,true,true);
  }else{
   classic_text(10,9,250,local("TOTAL TIME"),18,true);
   classic_text(10,32,250,race_stamp(s.elapsed),31,false);
   classic_text(10,78,250,local("LAP TIME"),18,true);
   classic_text(10,101,250,race_stamp(s.lap_time),31,false);
   shift=dx;
   classic_text(1020,9,236,local("POSITION"),18,true,true);
   classic_text(1020,30,236,ordinal(s.position,s.language),38,false,true);
   classic_text(1020,97,236,local("LAP"),18,true,true);
   classic_text(1020,119,236,std::to_string(s.lap)+"/"+std::to_string(s.laps),36,false,true);
   if(std::isfinite(s.off_route_distance)&&s.off_route_distance>=0&&s.recovery_countdown<=0){
    shift=dx*.5f;
    auto uk=s.language==UiLanguage::Ukrainian;
    classic_text(330,183,620,uk?"ПОЗА МАРШРУТОМ":"OFF THE ROUTE",25,true,false,true);
    auto dist=std::to_string(std::llround(std::min(999999.,s.off_route_distance)));
    classic_text(330,214,620,(uk?"ДО ТОЧКИ ВИЇЗДУ: ":"DISTANCE TO EXIT POINT: ")+dist+(uk?" м":" m"),20,false,false,true);
   }
  }
 }
 if(s.replay){shift=dx*.5f;vertical=dy;panel(170,606,940,100);std::string rate=s.replay_rate==.25?"0.25x":s.replay_rate==.5?"0.5x":s.replay_rate==2?"2x":s.replay_rate==4?"4x":"1x";text(190,617,900,"REPLAY   "+stamp(s.replay_position)+" / "+stamp(s.replay_duration)+"   "+(s.replay_paused?"PAUSED":rate),17,true);const char*cmd[]={"EXIT","RESTART",s.replay_paused?"PLAY":"PAUSE","FAST","SLOW"};const char*icons[]={"eject","rewind",s.replay_paused?"play":"pause","ffwd","slowmo"};for(int i=0;i<5;++i){art(std::string(icons[i])+(i==s.replay_selected?"_on":"_off"),214+i*180,661,.9f,1,true);text(240+i*180,651,120,cmd[i],16,i==s.replay_selected);}text(190,685,900,"LEFT / RIGHT + ENTER   |   SPACE pause   |   P / F3 exit",12);}
 if(!s.replay&&!s.music_title.empty()&&s.music_age>=0&&s.music_age<8){
  shift=dx*.5f;vertical=dy;
  float env=ease(float(s.music_age/.65))*(1-ease(float((s.music_age-6.8)/1.2)));
  float saved=opacity;opacity*=env;
  constexpr auto plaque="global/overlay/musicbar.dds";
  if(r_.exists(plaque)){
   float y=604+(1-env)*116;
   image(plaque,440,y,400,100);
   text(465,y+21,278,s.music_artist,15,false,false,false);
   text(465,y+47,278,s.music_title,19,true,false,false);
  }else{
   float y=638+(1-env)*90;
   panel(440,y,400,62);
   text(456,y+9,368,s.music_artist,13,false,false,false);
   text(456,y+29,368,s.music_title,17,true,false,false);
  }
  opacity=saved;
 }
 return out;
}
}
