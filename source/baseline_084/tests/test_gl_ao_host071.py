#!/usr/bin/env python3
"""Run the actual GLScreenEffects C++ host inside the Mesa fixture context.
Requires a C++17 compiler and Khronos glcorearb.h (GLCORE_HEADER overrides it).
Only WGL loading is replaced; State/Target/Program/begin/resolve are shipped code.
"""
import os,re,shutil,subprocess,tempfile,ctypes as C
from pathlib import Path
root=Path(__file__).resolve().parents[1]
header=Path(os.environ.get('GLCORE_HEADER','/usr/include/GL/glcorearb.h'))
if not header.exists():
    print('SKIP: GL host check requires Khronos glcorearb.h; set GLCORE_HEADER')
    raise SystemExit(0)
api=(root/'src/gl_api_win.hpp').read_text()
struct=api[api.index('struct GLApi {'):api.index('inline PROC gl_address')]
program=api[api.index('inline GLuint gl_program'):api.index('class GLWindow')]
initializers='\n'.join('gl.'+n+'=&gl'+n+';' for n in re.findall(r'PFN\w+PROC (\w+)=nullptr;',struct))
host=(root/'src/gl_screen_effects_win.hpp').read_text().replace('#include "gl_api_win.hpp"','').replace('#pragma once','')
adapter=(root/'src/gl_effects_adapter.hpp').read_text()
diagnostics=adapter[adapter.index(' Json diagnostics()const{')+len(' Json diagnostics()const{'):adapter.index('\n};',adapter.index(' Json diagnostics()const{'))].rstrip()[:-1]
diagnostics=diagnostics.replace('effects.','fx.')

cpp='''#define GL_GLEXT_PROTOTYPES 1
#include "'''+str(header)+'''"
#include "assets.hpp"
#include <algorithm>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstdio>
namespace ofo {'''+struct+program+'}\n'+host+r'''
extern "C" int host_check(){try{
 using namespace ofo;
 '''+initializers+r'''
 const int size=128;
 gl_fx_detail::Target destination;
 if(!destination.ensure(size,size,true))throw std::runtime_error("destination incomplete");
 // Seed program/textures/samplers and nondefault state to catch host leakage.
 GLuint sentinel=gl_program(gl_effects_fullscreen_vs(),gl_effects_photo_fs(),"sentinel");
 gl.UseProgram(sentinel);
 GLuint tex[3],samplers[3];glGenTextures(3,tex);gl.GenSamplers(3,samplers);
 for(int i=0;i<3;i++){gl.ActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,tex[i]);gl.BindSampler(i,samplers[i]);}
 gl.ActiveTexture(GL_TEXTURE2);glViewport(3,5,121,119);
 glDepthFunc(GL_GREATER);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glEnable(GL_SCISSOR_TEST);
 glScissor(0,0,size,size);glEnable(GL_DEPTH_TEST);
 GLScreenEffects fx;fx.set_bloom_intensity(0);
 bool enabled=false;
 auto adapter_diagnostics=[&]()->Json{'''+diagnostics+r'''};
 enabled=fx.begin(0,size,2,true,false);
 if(enabled)throw std::runtime_error("zero-sized target unexpectedly enabled");
 auto unavailable=adapter_diagnostics();
 if(unavailable.at("ao_enabled").get<bool>()||unavailable.at("ao_applied").get<bool>()||unavailable.at("ao_depth_reads").get<int>()!=0)throw std::runtime_error("failed AO host reports enabled or depth reads");
 auto frame=[&](bool ao){
 enabled=fx.begin(size,size,2,ao,false);if(!enabled)throw std::runtime_error(fx.last_error());
 GLint sceneDepth=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&sceneDepth);
 std::vector<float>depth(size*size);
 for(int y=0;y<size;y++)for(int x=0;x<size;x++){float rayY=2*(y+.5f)/size-1;float z=rayY<0?std::min(6.f,-1/rayY):6.f;depth[y*size+x]=1.001f-.1001f/z;}
 gl.ActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,sceneDepth);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,size,size,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(180/255.f,180/255.f,180/255.f,91/255.f);glClear(GL_COLOR_BUFFER_BIT);
 // Restore sentinel world state before resolving, as a real world draw does.
 gl.UseProgram(sentinel);for(int i=0;i<3;i++){gl.ActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,tex[i]);gl.BindSampler(i,samplers[i]);}gl.ActiveTexture(GL_TEXTURE2);
 auto p=identity();p[10]=1.001f;p[14]=-.1001f;
 if(!fx.resolve(p))throw std::runtime_error(fx.last_error());
 GLint value=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&value);if(value!=GL_TEXTURE2)throw std::runtime_error("active texture not restored");
 if(!glIsEnabled(GL_BLEND)||!glIsEnabled(GL_SCISSOR_TEST))throw std::runtime_error("world enable state not restored");
 glGetIntegerv(GL_CURRENT_PROGRAM,&value);if(GLuint(value)!=sentinel)throw std::runtime_error("program not restored");
 glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&value);if(GLuint(value)!=destination.fbo)throw std::runtime_error("destination FBO not restored");
 glGetIntegerv(GL_DEPTH_FUNC,&value);if(value!=GL_GREATER)throw std::runtime_error("depth state not restored");
 for(int i=0;i<3;i++){gl.ActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&value);if(GLuint(value)!=tex[i])throw std::runtime_error("texture not restored");glGetIntegerv(GL_SAMPLER_BINDING,&value);if(GLuint(value)!=samplers[i])throw std::runtime_error("sampler not restored");}gl.ActiveTexture(GL_TEXTURE2);
 std::vector<unsigned char>pixels(size*size*4);glReadPixels(0,0,size,size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
 int lowest=255;for(int i=0;i<size*size;i++){lowest=std::min(lowest,int(pixels[i*4]));if(pixels[i*4+3]!=91)throw std::runtime_error("source alpha not preserved");}
 std::vector<float>copied(size*size);glReadPixels(0,0,size,size,GL_DEPTH_COMPONENT,GL_FLOAT,copied.data());
 for(int i=0;i<size*size;i++)if(std::abs(copied[i]-depth[i])>1e-6f)throw std::runtime_error("depth not restored");
 if(ao&&lowest>=160)throw std::runtime_error("host AO contact invisible");
 if(!ao&&lowest!=180)throw std::runtime_error("host AO off still darkens");
 if(pixels[(10*size+10)*4]!=180)throw std::runtime_error("host globally darkens flat floor");
 if(fx.ao_applied()!=ao)throw std::runtime_error("AO applied diagnostic incorrect");
 auto report=adapter_diagnostics();if(report.at("ao_enabled").get<bool>()!=ao||report.at("ao_depth_reads").get<int>()!=(ao?fx.ao_samples()+5:0))throw std::runtime_error("successful AO adapter diagnostic incorrect");
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("host GL error");
 fx.end();return lowest;
 };
 int low=frame(true);frame(false);
 fx.reset();gl.DeleteProgram(sentinel);gl.DeleteSamplers(3,samplers);glDeleteTextures(3,tex);destination.reset();
 std::printf("Actual GLScreenEffects host: contact %d/180; AO off, FBO/program/texture/sampler/depth/alpha restoration passed\n",low);
 return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"GL host: %s\n",e.what());return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='ofo-ao-host-') as tmp:
    source=Path(tmp)/'host.cpp';source.write_text(cpp);library=Path(tmp)/'host.so'
    compiler=shutil.which('g++')
    assert compiler,'g++ required'
    # glcorearb.h includes KHR/khrplatform.h from its include root.
    subprocess.run([compiler,'-std=c++17','-shared','-fPIC','-O1','-I'+str(root/'src'),'-I'+str(root/'vendor'),'-idirafter',str(header.parent.parent),str(source),'-Wl,-l:libGL.so.1','-o',str(library)],check=True)
    base=(Path(__file__).with_name('test_gl_effects_shaders.py')).read_text()
    marker="    function(gl, 'glDeleteVertexArrays'"
    check="    assert C.CDLL("+repr(str(library))+ ").host_check()==0,'actual GLScreenEffects host'\n"
    base=base.replace(marker,check+marker,1)
    exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
