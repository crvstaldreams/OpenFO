#pragma once
#include "gl_screen_effects_win.hpp"
#include <string>
namespace ofo {
class GLSceneScale {
 gl_fx_detail::Target scene_;
 gl_fx_detail::State original_;
 bool active_=false;int failed_w_=0,failed_h_=0;std::string error_;
public:
 GLSceneScale()=default;GLSceneScale(const GLSceneScale&)=delete;GLSceneScale&operator=(const GLSceneScale&)=delete;
 ~GLSceneScale(){reset();}
 void reset(){if(active_){original_.restore();active_=false;}scene_.reset();failed_w_=failed_h_=0;}
 bool active()const{return active_;}
 int target_width()const{return scene_.w;}int target_height()const{return scene_.h;}
 const std::string&error()const{return error_;}
 bool begin(int w,int h){
  if(w<1||h<1||active_||(w==failed_w_&&h==failed_h_))return false;
  original_.save();bool ok=scene_.ensure(w,h,true);original_.restore();
  if(!ok){failed_w_=w;failed_h_=h;error_="Scaled OpenGL framebuffer allocation failed";return false;}
  gl.BindFramebuffer(GL_FRAMEBUFFER,scene_.fbo);glViewport(0,0,w,h);
  active_=true;failed_w_=failed_h_=0;error_.clear();return true;
 }
 bool composite(int output_w,int output_h){
  if(!active_)return false;
  gl.BindFramebuffer(GL_READ_FRAMEBUFFER,scene_.fbo);
  gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER,original_.draw);
  glDisable(GL_SCISSOR_TEST);
  gl.BlitFramebuffer(0,0,scene_.w,scene_.h,0,0,output_w,output_h,GL_COLOR_BUFFER_BIT,GL_LINEAR);
  const bool ok=glGetError()==GL_NO_ERROR;
  original_.restore();active_=false;
  if(!ok)error_="Scaled OpenGL framebuffer blit failed";
  return ok;
 }
};
}
