 void load_race_hud_map(const Geometry&t,const std::string&stage){
  map_texture=nullptr;map_config=Json();race_map_outline=false;
  const auto mode=race_mode(stage);
  if(mode==RaceMode::Stunt)return;
  bool north_up=true;Json native_config;
  try{
   native_config=bed_file(r,stage+"/data/map.bed");
   for(auto key:{"WorldTopLeft","WorldBottomRight","ScreenPos","ScreenSize"})for(auto axis:{"1","2"})
    if(!std::isfinite(native_config.at(key).at(axis).get<float>()))throw std::runtime_error("Invalid map metric");
   north_up=native_config.at("WorldTopLeft").at("2").get<float>()>native_config.at("WorldBottomRight").at("2").get<float>();
  }catch(const std::exception&){native_config=Json();}
  if(mode==RaceMode::Circuit)try{
   Json route=race_course(r,stage);
   wchar_t exe[32768];auto n=GetModuleFileNameW(nullptr,exe,32768);
   if(n&&n<32768){
    auto root=fs::path(std::wstring(exe,n)).parent_path();
    if(auto document=load_track_document(root,stage)){
     auto custom=track_route_json(*document);
     if(custom.value("available",false)){route=std::move(custom);north_up=false;}
    }
   }
   auto outline=race_hud_map(route,north_up);
   if(!outline.pixels.empty()){
    composed_map=upload_pixels(outline.width,outline.height,outline.pixels);map_texture=composed_map.get();
    map_config={{"WorldTopLeft",{{"1",outline.left},{"2",outline.top}}},{"WorldBottomRight",{{"1",outline.right},{"2",outline.bottom}}}};
    race_map_outline=true;minimap_report={{"mode","transparent_route_outline"},{"source",route.value("source",std::string())}};
    return;
   }
  }catch(const std::exception&e){trace("race_map_outline_unavailable",e.what());}
  // Arenas and tracks without an authored loop use the native map.
  try{
   if(!native_config.is_null()){
    map_config=std::move(native_config);
    map_texture=get(r.texture(t.source,map_config.at("MapTexture").get<std::string>()));
    if(map_texture)minimap_report={{"mode","native_map"}};
   }
  }catch(const std::exception&e){trace("race_map_fallback_unavailable",e.what());}
 }

 void draw_race_hud_map(const UI&ui,const State&s,float scale){
  if(!map_texture||(ui.frontend&&!ui.frontend->map_visible))return;
  const auto tl=map_config.at("WorldTopLeft"),br=map_config.at("WorldBottomRight");
  const float left=tl.at("1"),top=tl.at("2"),right=br.at("1"),bottom=br.at("2"),map_scale=scale*1.5f;
  float x=58*scale,y=height-194*scale,w=154*scale,h=154*scale;
  if(!race_map_outline){
   auto pos=map_config.at("ScreenPos"),size=map_config.at("ScreenSize");
   float px=pos.at("1"),py=pos.at("2");
   x=px*map_scale+(px>=320?width-640*map_scale:0);y=py*map_scale+(py>=240?height-480*map_scale:0);
   w=size.at("1").get<float>()*map_scale;h=size.at("2").get<float>()*map_scale;
  }
  quad(map_texture,x,y,w,h);
  if(std::abs(right-left)<=.01f||std::abs(bottom-top)<=.01f)return;
  auto marker=[&](Vec pos,float radius,uint32_t color,bool border){
   float u=(pos.x-left)/(right-left),v=(pos.z-top)/(bottom-top);
   if(u<0||u>1||v<0||v>1||!std::isfinite(u)||!std::isfinite(v))return;
   float q=radius*map_scale,px=x+u*w,py=y+v*h;
   if(border)quad(nullptr,px-q-scale,py-q-scale,2*(q+scale),2*(q+scale),0xdc151515);
   quad(nullptr,px-q,py-q,2*q,2*q,color);
  };
  if(s.opponents)for(const auto&b:*s.opponents)marker(point(b.car,{}),race_map_outline?1.2f:2.f,b.damage>=1?0xff777777:0xffe9e9e9,race_map_outline);
  marker(point(s.car,{}),race_map_outline?1.7f:3.f,0xffffd050,race_map_outline);
 }
