#pragma once
#include "gl_compat_win.hpp"
#include "gl_screen_effects_win.hpp"
namespace ofo {
// A fixed, bounded color/depth target. The scene is drawn by the renderer;
// this helper never calls the normal frame path or runs screen effects.
class GLRearviewGpu {
 gl_fx_detail::Target target_;GLTexture*color_=nullptr;gl_fx_detail::State saved_;bool active_=false;GLfloat clear_color_[4]{};GLdouble clear_depth_=1;GLint array_buffer_=0,clear_stencil_=0;std::string error_;
public:
 GLRearviewGpu()=default;GLRearviewGpu(const GLRearviewGpu&)=delete;GLRearviewGpu&operator=(const GLRearviewGpu&)=delete;
 ~GLRearviewGpu(){reset();}
 void reset(){end();if(color_){color_->Release();color_=nullptr;}target_.reset();error_.clear();}
 bool begin(){
 if(active_)return false;saved_.save();glGetFloatv(GL_COLOR_CLEAR_VALUE,clear_color_);glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth_);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array_buffer_);glGetIntegerv(GL_STENCIL_CLEAR_VALUE,&clear_stencil_);
 if(!target_.fbo){
 if(!target_.ensure(512,128,true)){saved_.restore();error_="Rearview framebuffer incomplete";return false;}
 // Transfer color texture ownership to the normal texture adapter for HUD draws.
 color_=new GLTexture(512,128,1,D3DFMT_A8R8G8B8,true);color_->id=target_.color;color_->staging=false;color_->mips.clear();target_.color=0;
 glBindTexture(GL_TEXTURE_2D,color_->id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 }
 gl.BindFramebuffer(GL_FRAMEBUFFER,target_.fbo);glViewport(0,0,512,128);glDepthRange(0,1);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_FRAMEBUFFER_SRGB);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
 active_=true;error_.clear();return true;
 }
 void end(){if(active_){saved_.restore();glClearColor(clear_color_[0],clear_color_[1],clear_color_[2],clear_color_[3]);glClearDepth(clear_depth_);glClearStencil(clear_stencil_);gl.BindBuffer(GL_ARRAY_BUFFER,array_buffer_);active_=false;}}
 GLTexture*texture()const{return color_;}const std::string&error()const{return error_;}
};
}
