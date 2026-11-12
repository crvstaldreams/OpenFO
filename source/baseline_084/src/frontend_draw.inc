// Include inside Renderer::Impl after frontend.hpp has been included.
void frontend(const Frontend& menu,double clock){
 for(const auto& c:menu.draw_commands(width,height,clock)){
  if(c.kind==FrontendDraw::Text)text(c.x,c.y,c.value,c.scale,c.color,c.heading,true,c.clip_left,c.clip_right);
  else quad(c.kind==FrontendDraw::Image?get(c.value):nullptr,c.x,c.y,c.w,c.h,c.color,c.u,c.v,c.uw,c.vh);
 }
}
void frontend_replay(const Frontend& menu,double position,double duration,double rate,bool paused,int selected){
 for(const auto& c:menu.replay_commands(width,height,position,duration,rate,paused,selected)){
  if(c.kind==FrontendDraw::Text)text(c.x,c.y,c.value,c.scale,c.color,c.heading,true,c.clip_left,c.clip_right);
  else quad(c.kind==FrontendDraw::Image?get(c.value):nullptr,c.x,c.y,c.w,c.h,c.color,c.u,c.v,c.uw,c.vh);
 }
}
