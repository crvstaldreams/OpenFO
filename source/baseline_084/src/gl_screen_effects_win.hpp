#pragma once
#include "gl_api_win.hpp"
#include "gl_screen_effects_shaders.hpp"
#include "screen_effects_policy.hpp"
#include "photo_mode.hpp"
#include <string>
#include <cstdint>
namespace ofo {
namespace gl_fx_detail {
// Save the state these passes modify, including all source texture bindings.
struct State {
 GLint draw=0,read=0,program=0,vao=0,active=0,textures[3]{},samplers[3]{},viewport[4]{},depthFunc=0,polygon[2]{},srcRGB=0,dstRGB=0,srcA=0,dstA=0,eqRGB=0,eqA=0;
 GLdouble depthRange[2]{};GLboolean srgb=0,depth=0,depthWrite=0,blend=0,cull=0,scissor=0,stencil=0,colorWrite[4]{};
 void save(){
 glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
 glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_VIEWPORT,viewport);
 glGetIntegerv(GL_ACTIVE_TEXTURE,&active);for(int i=0;i<3;++i){gl.ActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);}gl.ActiveTexture(active);
 glGetDoublev(GL_DEPTH_RANGE,depthRange);srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);glGetBooleanv(GL_COLOR_WRITEMASK,colorWrite);
 glGetIntegerv(GL_POLYGON_MODE,polygon);depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);cull=glIsEnabled(GL_CULL_FACE);scissor=glIsEnabled(GL_SCISSOR_TEST);stencil=glIsEnabled(GL_STENCIL_TEST);
 glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcA);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstA);
 glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqA);
 }
 static void enable(GLenum flag,GLboolean yes){if(yes)glEnable(flag);else glDisable(flag);}
 void restore(){gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);gl.BindFramebuffer(GL_READ_FRAMEBUFFER,read);gl.UseProgram(program);gl.BindVertexArray(vao);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
 for(int i=0;i<3;++i){gl.ActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);gl.BindSampler(i,samplers[i]);}gl.ActiveTexture(active);
 glDepthRange(depthRange[0],depthRange[1]);enable(GL_FRAMEBUFFER_SRGB,srgb);glDepthFunc(depthFunc);glDepthMask(depthWrite);glColorMask(colorWrite[0],colorWrite[1],colorWrite[2],colorWrite[3]);glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
 enable(GL_DEPTH_TEST,depth);enable(GL_BLEND,blend);enable(GL_CULL_FACE,cull);enable(GL_SCISSOR_TEST,scissor);enable(GL_STENCIL_TEST,stencil);
 gl.BlendFuncSeparate(srcRGB,dstRGB,srcA,dstA);gl.BlendEquationSeparate(eqRGB,eqA);
 }
};
inline void sources(GLuint color,GLuint depth=0,GLuint ao=0){GLuint textures[]={color,depth,ao};for(int i=0;i<3;++i){gl.ActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);gl.BindSampler(i,0);}gl.ActiveTexture(GL_TEXTURE0);}
inline void fullscreen(GLuint vao,int w,int h,bool depth){glViewport(0,0,w,h);glDepthRange(0.,1.);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);State::enable(GL_DEPTH_TEST,depth);glDepthMask(depth?GL_TRUE:GL_FALSE);glDepthFunc(GL_ALWAYS);gl.BindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);}
struct Target {
 GLuint fbo=0,color=0,depth=0;int w=0,h=0;
 Target()=default;Target(const Target&)=delete;Target&operator=(const Target&)=delete;
 void reset(){if(fbo)gl.DeleteFramebuffers(1,&fbo);if(color)glDeleteTextures(1,&color);if(depth)glDeleteTextures(1,&depth);fbo=color=depth=0;w=h=0;}
 void texture(GLuint&t,GLint internal,GLenum format,GLenum type){glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,format,type,nullptr);}
 bool ensure(int width,int height,bool withDepth){if(fbo&&width==w&&height==h)return true;reset();w=width;h=height;gl.ActiveTexture(GL_TEXTURE0);gl.GenFramebuffers(1,&fbo);gl.BindFramebuffer(GL_FRAMEBUFFER,fbo);texture(color,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE);gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 if(withDepth){texture(depth,GL_DEPTH_COMPONENT24,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT);gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);}
 bool ok=gl.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;if(!ok)reset();return ok;}
};
struct Program {
 GLuint id=0;GLint sceneColor=-1,sceneDepth=-1,aoTexture=-1,projection=-1,controls=-1,aoTexel=-1,aoEnabled=-1,world=-1,view=-1,projectionMatrix=-1,weather=-1,sunScreen=-1,atmosphere=-1,postControls=-1,viewUp=-1,viewX=-1,viewZAxis=-1,preset=-1;
 void reset(){if(id)gl.DeleteProgram(id);id=0;}
 bool make(const char*vs,const char*fs,const char*label){if(id)return true;id=gl_program(vs,fs,label);if(!id)return false;
 #define OFO_FX_UNIFORM(n) n=gl.GetUniformLocation(id,#n)
 OFO_FX_UNIFORM(sceneColor);OFO_FX_UNIFORM(sceneDepth);OFO_FX_UNIFORM(aoTexture);OFO_FX_UNIFORM(projection);OFO_FX_UNIFORM(controls);OFO_FX_UNIFORM(aoTexel);OFO_FX_UNIFORM(aoEnabled);OFO_FX_UNIFORM(world);OFO_FX_UNIFORM(view);OFO_FX_UNIFORM(projectionMatrix);OFO_FX_UNIFORM(viewUp);OFO_FX_UNIFORM(viewX);OFO_FX_UNIFORM(viewZAxis);OFO_FX_UNIFORM(preset);OFO_FX_UNIFORM(weather);OFO_FX_UNIFORM(sunScreen);OFO_FX_UNIFORM(atmosphere);OFO_FX_UNIFORM(postControls);
 #undef OFO_FX_UNIFORM
 gl.UseProgram(id);gl.Uniform1i(sceneColor,0);gl.Uniform1i(sceneDepth,1);gl.Uniform1i(aoTexture,2);return true;}
 void use(){gl.UseProgram(id);}void proj(const Mat&p){gl.Uniform4f(projection,p[0],p[5],p[10],p[14]);}
};
}
struct GLScreenEffectsCounters {std::uint64_t ao_passes=0,composite_passes=0,water_meshes=0,target_rebuilds=0;};
class GLScreenEffects {
 gl_fx_detail::Target scene_,ao_;gl_fx_detail::Program aoProgram_,composite_,water_;GLuint vao_=0;
 gl_fx_detail::State sceneState_,waterState_;ScreenEffectsBudget budget_{};Mat view_=identity();
 float weather_[4]{},sunScreen_[4]{},atmosphere_[4]{},post_controls_[4]{1,0,0,0};
 bool active_=false,resolved_=false,waterActive_=false,touched_=false,aoApplied_=false;std::string error_;GLScreenEffectsCounters counters_{};
 bool fail(const char*s){error_=s;return false;}
public:
 GLScreenEffects()=default;GLScreenEffects(const GLScreenEffects&)=delete;GLScreenEffects&operator=(const GLScreenEffects&)=delete;
 ~GLScreenEffects(){reset();}
 const std::string&last_error()const{return error_;}const GLScreenEffectsCounters&counters()const{return counters_;}
 bool ao_enabled()const{return budget_.enabled&&budget_.ao_samples>0;}bool ao_applied()const{return aoApplied_;}int ao_samples()const{return budget_.ao_samples;}int ao_width()const{return ao_.w;}int ao_height()const{return ao_.h;}float ao_radius()const{return budget_.ao_radius;}float ao_strength()const{return budget_.ao_strength;}
 bool touched()const{return touched_;}unsigned long long ao_draws()const{return counters_.ao_passes+counters_.composite_passes;}unsigned long long ssr_draws()const{return counters_.water_meshes;}
 void reset(){end();scene_.reset();ao_.reset();aoProgram_.reset();composite_.reset();water_.reset();if(vao_)gl.DeleteVertexArrays(1,&vao_);vao_=0;resolved_=false;aoApplied_=false;error_.clear();}
 void set_bloom_intensity(float value){post_controls_[0]=std::isfinite(value)?std::clamp(value,0.f,2.f):1.f;}
 float bloom_intensity()const{return post_controls_[0];}
 void neutral_environment(){for(float&v:weather_)v=0;for(float&v:sunScreen_)v=0;}
 void configure_environment(bool night,bool rain,float intensity,float rainStrength,Vec sunDirection,const Mat&view,const Mat&projection,float time,bool cockpit=false){
 weather_[0]=1;weather_[1]=night?1.f:0.f;weather_[2]=rain?std::clamp(rainStrength,0.f,1.f):0.f;weather_[3]=std::isfinite(time)?time:0.f;
 Vec sun=direction(view,sunDirection);bool front=sun.z>.01f;
 sunScreen_[0]=front?.5f+.5f*sun.x*projection[0]/sun.z:-1.f;
 sunScreen_[1]=front?.5f+.5f*sun.y*projection[5]/sun.z:-1.f;
 sunScreen_[2]=night?0.f:std::clamp(intensity,0.f,1.5f);sunScreen_[3]=front?1.f:0.f;atmosphere_[2]=cockpit?1.f:0.f;
 }
 bool begin(int w,int h,int detail,bool aoEnabled=true,bool ssrEnabled=true){
 end();touched_=false;resolved_=false;aoApplied_=false;budget_=screen_effects_budget(detail);if(!aoEnabled)budget_.ao_samples=0;if(!ssrEnabled)budget_.ssr_steps=0;
 if(!budget_.enabled)return false;
 post_controls_[1]=detail>=1?1.f:0.f;post_controls_[2]=1.f/std::max(1,w);post_controls_[3]=1.f/std::max(1,h);atmosphere_[0]=detail>=3?8.f:4.f;atmosphere_[1]=detail>=2?.16f:.10f;atmosphere_[3]=float(w)/std::max(1,h);
 if(w<=0||h<=0)return fail("OpenGL effects target has no dimensions");
 sceneState_.save();bool ok=false;
 try{ok=composite_.make(gl_effects_fullscreen_vs(),gl_effects_composite_fs(),"effects composite")&&(!budget_.ao_samples||aoProgram_.make(gl_effects_fullscreen_vs(),gl_effects_ao_fs(),"SSAO"))&&(!budget_.ssr_steps||water_.make(gl_effects_water_vs(),gl_effects_water_fs(),"water SSR"));}catch(const std::exception&e){error_=e.what();}
 if(!ok){sceneState_.restore();if(error_.empty())fail("OpenGL effects GLSL compilation failed");return false;}
 if(!vao_)gl.GenVertexArrays(1,&vao_);
 auto size=screen_effects_size(w,h,budget_.ao_divisor);bool rebuild=scene_.w!=w||scene_.h!=h||ao_.w!=size.first||ao_.h!=size.second;
 ok=scene_.ensure(w,h,true)&&(!budget_.ao_samples||ao_.ensure(size.first,size.second,false));if(ok){glBindTexture(GL_TEXTURE_2D,scene_.color);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}sceneState_.restore();
 if(!ok)return fail("OpenGL effects framebuffer allocation incomplete; native rendering remains available");
 if(rebuild)++counters_.target_rebuilds;
 gl_fx_detail::sources(0);gl.BindFramebuffer(GL_FRAMEBUFFER,scene_.fbo);glViewport(0,0,w,h);active_=touched_=true;error_.clear();return true;
 }
 bool resolve(const Mat&projection){if(!active_)return false;gl_fx_detail::State state;state.save();
 // Bind a different draw FBO before sampling scene depth: never sample attached output.
 if(budget_.ao_samples){gl.BindFramebuffer(GL_FRAMEBUFFER,ao_.fbo);gl_fx_detail::sources(0,scene_.depth);aoProgram_.use();aoProgram_.proj(projection);gl.Uniform4f(aoProgram_.controls,budget_.ao_radius,budget_.ao_strength,float(budget_.ao_samples),1.f);gl_fx_detail::fullscreen(vao_,ao_.w,ao_.h,false);++counters_.ao_passes;}
 gl.BindFramebuffer(GL_FRAMEBUFFER,sceneState_.draw);gl_fx_detail::sources(scene_.color,scene_.depth,ao_.color);composite_.use();composite_.proj(projection);gl.Uniform2f(composite_.aoTexel,1.f/std::max(1,ao_.w),1.f/std::max(1,ao_.h));gl.Uniform1i(composite_.aoEnabled,budget_.ao_samples>0);gl.Uniform4f(composite_.weather,weather_[0],weather_[1],weather_[2],weather_[3]);gl.Uniform4f(composite_.sunScreen,sunScreen_[0],sunScreen_[1],sunScreen_[2],sunScreen_[3]);gl.Uniform4f(composite_.atmosphere,atmosphere_[0],atmosphere_[1],atmosphere_[2],atmosphere_[3]);gl.Uniform4f(composite_.postControls,post_controls_[0],post_controls_[1],post_controls_[2],post_controls_[3]);gl_fx_detail::fullscreen(vao_,scene_.w,scene_.h,true);++counters_.composite_passes;
 // Keep current world pipeline, but return to the caller's original destination.
 state.draw=sceneState_.draw;state.read=sceneState_.read;state.restore();active_=false;resolved_=true;aoApplied_=budget_.ao_samples>0;return true;
 }
 bool water_begin(const Mat&view,const Mat&projection,float time){if(!resolved_||!budget_.ssr_steps||waterActive_)return false;waterState_.save();waterActive_=true;view_=view;
 water_.use();water_.proj(projection);gl.UniformMatrix4fv(water_.view,1,GL_FALSE,view.data());gl.UniformMatrix4fv(water_.projectionMatrix,1,GL_FALSE,projection.data());gl.Uniform4f(water_.controls,time,float(budget_.ssr_steps),budget_.ssr_distance,budget_.ssr_strength);
 Vec up=direction(view,{0,1,0}),x=direction(view,{1,0,0}),z=direction(view,{0,0,1});gl.Uniform3f(water_.viewUp,up.x,up.y,up.z);gl.Uniform3f(water_.viewX,x.x,x.y,x.z);gl.Uniform3f(water_.viewZAxis,z.x,z.y,z.z);
 gl_fx_detail::sources(scene_.color,scene_.depth);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glEnable(GL_BLEND);gl.BlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);gl.BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);return true;
 }
 void water_mesh(const Mat&world){if(waterActive_){gl.UniformMatrix4fv(water_.world,1,GL_FALSE,world.data());++counters_.water_meshes;}}
 void water_end(){if(waterActive_){waterState_.restore();waterActive_=false;}}
 void end(){water_end();if(active_){sceneState_.restore();active_=false;}resolved_=false;}
};
// Photo outer target includes all world passes and is graded exactly once before HUD.
// Explicit PNG capture remains renderer-owned; no normal frame path reads pixels.
class GLPhotoGpu {
 gl_fx_detail::Target scene_;gl_fx_detail::Program program_;gl_fx_detail::State state_;GLuint vao_=0;bool active_=false;std::string error_;
public:
 GLPhotoGpu()=default;GLPhotoGpu(const GLPhotoGpu&)=delete;GLPhotoGpu&operator=(const GLPhotoGpu&)=delete;~GLPhotoGpu(){reset();}
 const std::string&last_error()const{return error_;}bool effects_available()const{return program_.id!=0;}
 void reset(){if(active_){state_.restore();active_=false;}scene_.reset();program_.reset();if(vao_)gl.DeleteVertexArrays(1,&vao_);vao_=0;error_.clear();}
 bool begin(int w,int h){if(active_||w<=0||h<=0)return false;state_.save();bool ok=false;
 try{ok=program_.make(gl_effects_fullscreen_vs(),gl_effects_photo_fs(),"photo grading");}catch(const std::exception&e){error_=e.what();}
 if(ok){if(!vao_)gl.GenVertexArrays(1,&vao_);ok=scene_.ensure(w,h,true);}state_.restore();if(!ok){if(error_.empty())error_="OpenGL photo target or GLSL unavailable; native rendering remains available";return false;}
 gl_fx_detail::sources(0);gl.BindFramebuffer(GL_FRAMEBUFFER,scene_.fbo);glViewport(0,0,w,h);active_=true;error_.clear();return true;}
 bool finish(PhotoSettings settings){if(!active_)return false;photo_sanitize(settings);gl.BindFramebuffer(GL_FRAMEBUFFER,state_.draw);gl_fx_detail::sources(scene_.color);program_.use();gl.Uniform4f(program_.controls,std::exp2(settings.exposure),settings.contrast,settings.saturation,settings.vignette);gl.Uniform1i(program_.preset,settings.preset);gl_fx_detail::fullscreen(vao_,scene_.w,scene_.h,false);state_.restore();active_=false;return true;}
};
}
