#pragma once
#include "gl_compat_win.hpp"
#include "gl_screen_effects_win.hpp"
namespace ofo {
struct PhotoPixels{unsigned width=0,height=0;std::vector<uint32_t>argb;};
class PhotoGpu {
 GLPhotoGpu effects;unsigned w=0,h=0;std::string error;
public:
 void reset(){effects.reset();w=h=0;error.clear();}
 bool begin(GLDevice*,unsigned width,unsigned height){w=width;h=height;return effects.begin(width,height);}
 bool finish(GLDevice*,const PhotoSettings&s){return effects.finish(s);}
 bool recover_ungraded(GLDevice*){PhotoSettings s;return effects.finish(s);}
 bool effects_available()const{return effects.effects_available();}const std::string&last_error()const{return error.empty()?effects.last_error():error;}
 bool capture(GLDevice*d,PhotoPixels&out){out.width=d->width;out.height=d->height;out.argb.resize(size_t(out.width)*out.height);std::vector<uint32_t>pixels(out.argb.size());glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);for(int i=0;i<16&&glGetError()!=GL_NO_ERROR;++i){}glReadPixels(0,0,out.width,out.height,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());if(glGetError()!=GL_NO_ERROR){error="OpenGL photo pixel readback failed";return false;}for(unsigned y=0;y<out.height;y++)std::memcpy(out.argb.data()+size_t(y)*out.width,pixels.data()+size_t(out.height-1-y)*out.width,size_t(out.width)*4);return true;}
};
class ScreenEffects {
 GLScreenEffects effects;uint64_t before=0;bool enabled=false;
public:
 void reset(GLDevice*){effects.reset();enabled=false;}
 bool begin(GLDevice*,GLBackendInfo*,unsigned w,unsigned h,int detail,bool active,bool ao=true,bool ssr=true){before=effects.ao_draws()+effects.ssr_draws();enabled=effects.begin(w,h,active?detail:0,active&&ao,active&&ssr);return enabled;}
 void configure_environment(bool night,bool rain,float sunIntensity,float rainStrength,Vec sunDirection,const Mat&view,const Mat&projection,float time,bool cockpit=false){effects.configure_environment(night,rain,sunIntensity,rainStrength,sunDirection,view,projection,time,cockpit);}
 void neutral_environment(){effects.neutral_environment();}
 void set_bloom_intensity(float value){effects.set_bloom_intensity(value);}
 float bloom_intensity()const{return effects.bloom_intensity();}
 bool touched()const{return effects.touched();}
 bool resolve(GLDevice*,const Mat&p){return effects.resolve(p);}
 void end(GLDevice*){effects.end();}
 bool begin_water(GLDevice*,const Mat&v,const Mat&p,float t){return effects.water_begin(v,p,t);}
 bool draw_water(GLDevice*d,const Mat&world,GLVertexBuffer*vb,GLIndexBuffer*ib,UINT,UINT count){if(!vb||!ib)return false;effects.water_mesh(world);d->vb=vb;d->ib=ib;d->offset=0;d->stride=sizeof(Vertex);d->fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX2;d->attributes();glDrawElements(GL_TRIANGLES,count,ib->type(),nullptr);return true;}
 void end_water(GLDevice*){effects.water_end();}
 unsigned extra_draws()const{return unsigned(effects.ao_draws()+effects.ssr_draws()-before);}
 Json diagnostics()const{return{{"backend","GLSL 330 framebuffer effects"},{"enabled",enabled},{"bloom_intensity",effects.bloom_intensity()},{"fxaa","quality-enabled scene composite; HUD excluded"},{"ao_enabled",enabled&&effects.ao_enabled()},{"ao_applied",effects.ao_applied()},{"ao_samples",effects.ao_samples()},{"ao_depth_reads",enabled&&effects.ao_enabled()?effects.ao_samples()+5:0},{"ao_radius_m",effects.ao_radius()},{"ao_strength",effects.ao_strength()},{"ao_resolution",{effects.ao_width(),effects.ao_height()}},{"ao_draws",effects.ao_draws()},{"ssr_meshes",effects.ssr_draws()},{"atmosphere","bounded highlight bloom; depth-gated sun; environment grading/rain before HUD"},{"frame_readbacks",0},{"error",effects.last_error()}};}
};
}
