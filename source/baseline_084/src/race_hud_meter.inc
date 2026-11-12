 Texture*muscle_meter_atlas=nullptr;
 bool muscle_meter_checked=false;

 void draw_meter_arc(float cx,float cy,float radius,float scale,float fill,uint32_t accent,bool active){
  constexpr int divisions=16;
  auto radial=[&](const MeterArcPoint& at,float w,float h,uint32_t color){
   quad(nullptr,at.x-w*.5f,at.y-h*.5f,w,h,color,0,0,1,1,at.angle,w*.5f,h*.5f);
  };
  for(int i=0;i<=divisions;++i){
   const float fraction=float(i)/divisions;
   const auto at=meter_arc_point(cx,cy,radius,fraction);
   const float w=(i%4==0?12.f:9.f)*scale;
   radial(at,w+2*scale,6*scale,0xe7111215);
   radial(at,w,2.5f*scale,fill>0&&fraction<=fill?accent:0xff57514b);
  }
  const auto marker=meter_arc_point(cx,cy,radius,fill);
  radial(marker,24*scale,12*scale,0xff111215);
  radial(marker,19*scale,7*scale,active?0xfffff2cf:accent);
  radial(marker,12*scale,2*scale,0xfffff1db);
 }

 void draw_race_hud_meter(const UI&ui,const State&s,float scale){
  const bool battle=ui.mode==RaceMode::Battle;
  const bool stunt=ui.mode==RaceMode::Stunt;
  const bool dual_damage=ui.mode==RaceMode::Circuit&&arena_circuit(ui.track);
  const float damage=meter_arc_fraction(s.damage);
  const float nitro_fill=meter_arc_fraction(s.nitro);
  const float angle_fill=meter_arc_fraction(float(ui.stunt.angle),0,stunt_max_launch_angle);
  const uint32_t amber=0xffffbe65,red=0xfff06855;
  const bool mph=ui.frontend&&ui.frontend->imperial;
  const auto reading=muscle_meter_readings(mph,battle,stunt,s.speed,s.rpm,s.gear,s.nitro,s.damage);
  if(!muscle_meter_checked){
   muscle_meter_checked=true;
   if(r.exists(muscle_meter_texture))muscle_meter_atlas=get(muscle_meter_texture);
  }
  if(muscle_meter_atlas){
   // Plates, needles and gear share the same atlas batch. Arc ticks and
   // markers follow in one untextured batch for either graphics backend.
   const float k=scale*.69f*1.2f;
   const float x=width-(muscle_meter_face.w+14)*k,y=height-(muscle_meter_face.h+8)*k;
   auto sprite=[&](MeterRect uv,float left,float top,float w,float h,uint32_t tint=0xffffffff,
                   float angle=0,float pivot_x=0,float pivot_y=0){
    constexpr float atlas=512.f;
    quad(muscle_meter_atlas,x+left*k,y+top*k,w*k,h*k,tint,uv.x/atlas,uv.y/atlas,uv.w/atlas,uv.h/atlas,
         angle,pivot_x*k,pivot_y*k);
   };
   auto face=muscle_meter_face;
   if(mph)face.y=muscle_meter_imperial_y;
   sprite(face,0,0,face.w,face.h);
   auto draw_needle=[&](float cx,float cy,float w,float h,float angle){
    const float tail=w*muscle_needle_axle_x/muscle_meter_needle_sprite.w;
    const float axle=h*muscle_needle_axle_y/muscle_meter_needle_sprite.h;
    sprite(muscle_meter_needle_sprite,cx-tail,cy-axle,w,h,0xffffffff,angle,tail,axle);
    sprite(muscle_meter_hub_sprite,cx-8,cy-8,16,16);
   };
   draw_needle(muscle_meter_speed_x,muscle_meter_speed_y,85,15,reading.speed_angle);
   draw_needle(muscle_meter_rpm_x,muscle_meter_rpm_y,70,13,reading.rpm_angle);
   const int digits=reading.speed>=100?3:reading.speed>=10?2:1;
   int number=reading.speed;
   for(int i=digits-1;i>=0;--i){
    sprite(muscle_meter_digit(number%10),muscle_meter_speed_x-10*digits+20*i,160,20,29,0xffffd697);
    number/=10;
   }
   sprite(muscle_meter_digit(reading.gear),353,187,23,32,0xffffd697);
   const auto cx=x+muscle_meter_speed_x*k,cy=y+muscle_meter_speed_y*k;
   if(stunt)draw_meter_arc(cx,cy,116*k,k,angle_fill,amber,ui.stunt.charging);
   else if(battle)draw_meter_arc(cx,cy,116*k,k,damage,red,false);
   else{
    draw_meter_arc(cx,cy,116*k,k,nitro_fill,amber,s.nitro_active);
    if(dual_damage)draw_meter_arc(cx,cy,140*k,k,damage,red,false);
   }

   // Cover the old inactive bar and use its inset as the arc legend.
   quad(nullptr,x+227*k,y+180*k,108*k,37*k,0xff08090b);
   const bool uk=text_language==UiLanguage::Ukrainian;
   auto legend=[&](float top,const std::string& value,uint32_t tint){
    text(x+232*k,y+top*k,value,k*15.f/18.f,tint,false,true);
   };
   auto percent=[](float fill){return std::to_string(int(std::lround(fill*100)))+"%";};
   if(stunt)legend(192,(uk?"КУТ  ":"ANGLE  ")+std::to_string(int(std::lround(ui.stunt.angle)))+"°",amber);
   else if(battle)legend(192,(uk?"ШКОДА  ":"DMG  ")+percent(damage),red);
   else{
    legend(dual_damage?181:192,(uk?"НІТРО  ":"NITRO  ")+percent(nitro_fill),amber);
    if(dual_damage)legend(198,(uk?"ШКОДА  ":"DMG  ")+percent(damage),red);
   }
   return;
  }

  // Earlier content packs can still use their native meter artwork.
  const float meter=scale*1.5f*1.2f,size=128*meter,x=width-136*meter,y=height-136*meter;
  for(const auto&layer:meter_artwork(battle,stunt,0,0))
   if(auto*t=get(layer.texture))
    quad(t,x+size*layer.x/256,y+size*layer.y/256,size*layer.w/256,size*layer.h/256,
         0xffffffff,layer.x/256,layer.y/256,layer.w/256,layer.h/256);
  auto rpm=meter_needle(std::isfinite(s.rpm)?s.rpm:0);
  if(needle)quad(needle,x+(rpm.pivot_x-18)*meter,y+(rpm.pivot_y-4)*meter,64*meter,8*meter,
                 0xffffffff,0,0,1,1,rpm.angle,18*meter,4*meter);
  if(glass)quad(glass,x,y,size,size);
  const auto cx=x+72*meter,cy=y+62*meter;
  if(stunt)draw_meter_arc(cx,cy,68*meter,meter*.65f,angle_fill,amber,ui.stunt.charging);
  else if(battle)draw_meter_arc(cx,cy,68*meter,meter*.65f,damage,red,false);
  else{
   draw_meter_arc(cx,cy,68*meter,meter*.65f,nitro_fill,amber,s.nitro_active);
   if(dual_damage)draw_meter_arc(cx,cy,83*meter,meter*.65f,damage,red,false);
  }
  const auto instruments=meter_instruments();
  auto meter_text=[&](MeterRect slot,const std::string&value,bool right=false){
   float font=slot.h/18*meter;
   const float available=slot.w*meter;
   float used=ui_text_width_pixels(ui_raster_pixels(font),false,value);
   if(used>available){
    font*=available/used;
    used=ui_text_width_pixels(ui_raster_pixels(font),false,value);
   }
   text(x+slot.x*meter+std::max(0.f,(available-used)*(right?1.f:.5f)),y+slot.y*meter,value,font,0xffffffff);
  };
  meter_text(instruments.speed,std::to_string(reading.speed),true);
  meter_text(instruments.unit,mph?"mph":"km/h");
  const int gear_cell=s.gear<0?0:s.gear==0?1:std::min(s.gear,6)+1;
  const auto gear=instruments.gear;
  if(auto*t=get("global/overlay/overlaygearfonts.dds"))
   quad(t,x+gear.x*meter,y+gear.y*meter,gear.w*meter,gear.h*meter,0xffffffff,gear_cell/8.f,0,1/8.f,1);
 }
