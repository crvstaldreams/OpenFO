 void draw_track_loading(const UI& ui,float scale){
  const auto& screen=ui.loading;
  auto* preview=screen.preview.empty()?nullptr:get(screen.preview);
  if(!preview)preview=get("menu/loading_pc.tga");
  if(preview){
   const auto uv=loading_cover_uv(width,height,preview->w,preview->h);
   quad(preview,0,0,float(width),float(height),0xffffffff,uv.u,uv.v,uv.w,uv.h);
  }else quad(nullptr,0,0,float(width),float(height),0xff1a1d22);
  quad(nullptr,0,0,float(width),float(height),0x24000000);

  const bool ukrainian=!ui.frontend||ui.frontend->language==UiLanguage::Ukrainian;
  const float inset=32*scale,top=height-151*scale;
  const float bar_y=height-30*scale,bar_width=width-2*inset;
  quad(nullptr,0,top,float(width),151*scale,0xee0b0e12);
  quad(nullptr,0,top,float(width),1*scale,0xff69717b);
  float title_x=inset;
  const auto& logo=screen.logo;
  if(logo){
   if(auto* atlas=get(logo.texture)){
    const float fit=std::min(std::min(214*scale,width*.28f)/logo.width,56*scale/logo.height);
    const float w=logo.width*fit,h=logo.height*fit;
    quad(atlas,inset,top+24*scale+(56*scale-h)*.5f,w,h,0xffffffff,
         float(logo.x)/logo.atlas_width,float(logo.y)/logo.atlas_height,
         float(logo.width)/logo.atlas_width,float(logo.height)/logo.atlas_height);
    title_x+=w+32*scale;
   }
  }
  const auto title=ui_fit_text(screen.name,std::max(0.f,width-inset-title_x),scale*1.35f,true);
  text(title_x,top+20*scale,title,scale*1.35f,0xfff6f4ee,true,true);
  if(screen.laps){
   const auto laps=std::to_string(screen.laps)+(ukrainian?(screen.laps==1?" КОЛО":screen.laps<=4?" КОЛА":" КІЛ"):(screen.laps==1?" LAP":" LAPS"));
   text(title_x,top+59*scale,laps,.90f*scale,0xffd0d4d9,false,true);
  }
  const auto percent=std::to_string(std::min(screen.progress,100u))+"%";
  const float font=.82f*scale;
  text(inset,top+91*scale,ukrainian?"ЗАВАНТАЖЕННЯ ТРАСИ":"LOADING TRACK",font,0xffffd442,false,true);
  const auto percent_width=ui_text_width_pixels(ui_raster_pixels(font),false,percent);
  text(width-inset-percent_width,top+91*scale,percent,font,0xffffd442,false,true);
  quad(nullptr,inset,bar_y,bar_width,6*scale,0xff343941);
  const auto fill=loading_progress(screen.progress);
  if(fill>0)quad(nullptr,inset,bar_y,bar_width*fill,6*scale,0xffffd442);
 }
