#pragma once
#include "gl_compat_win.hpp"
#include "gl_screen_effects_win.hpp"
#include "scene_lighting.hpp"
namespace ofo {
inline const char*gl_headlight_shadow_vs(){return R"GLSL(#version 330 core
layout(location=0)in vec3 position;uniform mat4 lightWorld;
void main(){vec4 p=lightWorld*vec4(position,1.);p.z=2.*p.z-p.w;gl_Position=p;}
)GLSL";}
inline const char*gl_headlight_shadow_fs(){return R"GLSL(#version 330 core
void main(){}
)GLSL";}
// Two depth projections for the player's physical lamps. Separate light
// origins prevent a wall edge from switching both lamps off at once. All work
// remains GPU resident, with strict draw/triangle budgets supplied by renderer.
class GLHeadlightShadowGpu {
 gl_fx_detail::Target target_;gl_fx_detail::State saved_;GLuint program_=0,vao_=0;GLint matrix_=-1,array_buffer_=0;GLdouble clear_depth_=1;bool active_=false;int size_=headlight_shadow_map_size,failed_size_=0;
public:
 ~GLHeadlightShadowGpu(){reset();}
 void reset(){end();target_.reset();if(program_)gl.DeleteProgram(program_);if(vao_)gl.DeleteVertexArrays(1,&vao_);program_=vao_=0;failed_size_=0;}
 bool begin(int size=headlight_shadow_map_size){if(active_||failed_size_==size)return false;failed_size_=0;size_=size;saved_.save();glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array_buffer_);glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth_);
  if(!target_.ensure(size_*2,size_,true)){failed_size_=size_;saved_.restore();return false;}
  if(!program_){program_=gl_program(gl_headlight_shadow_vs(),gl_headlight_shadow_fs(),"headlight depth");matrix_=gl.GetUniformLocation(program_,"lightWorld");gl.GenVertexArrays(1,&vao_);}
  gl.BindFramebuffer(GL_FRAMEBUFFER,target_.fbo);glViewport(0,0,size_*2,size_);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_FRAMEBUFFER_SRGB);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glDepthRange(0,1);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);gl.UseProgram(program_);gl.BindVertexArray(vao_);gl.EnableVertexAttribArray(0);active_=true;return true;
 }
 void lamp(int side){glViewport(side*size_,0,size_,size_);}
 void draw(const Mat&light_world,GLVertexBuffer*vb,GLIndexBuffer*ib,UINT count){if(!active_||!vb||!ib||!count)return;gl.UniformMatrix4fv(matrix_,1,GL_FALSE,light_world.data());gl.BindBuffer(GL_ARRAY_BUFFER,vb->id);gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib->id);glDrawElements(GL_TRIANGLES,count,ib->type(),nullptr);}
 void end(){if(active_){saved_.restore();gl.BindBuffer(GL_ARRAY_BUFFER,array_buffer_);glClearDepth(clear_depth_);active_=false;}}
 GLuint texture()const{return target_.depth;}
};
}
