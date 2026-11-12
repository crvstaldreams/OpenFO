#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include "assets.hpp"
#include <string>
#include <stdexcept>
#include <vector>
#include <cstdio>
namespace ofo {
struct GLApi {
 PFNGLGENBUFFERSPROC GenBuffers=nullptr;
 PFNGLBINDBUFFERPROC BindBuffer=nullptr;
 PFNGLBUFFERDATAPROC BufferData=nullptr;
 PFNGLBUFFERSUBDATAPROC BufferSubData=nullptr;
 PFNGLDELETEBUFFERSPROC DeleteBuffers=nullptr;
 PFNGLGETBUFFERPARAMETERIVPROC GetBufferParameteriv=nullptr;
 PFNGLMAPBUFFERRANGEPROC MapBufferRange=nullptr;
 PFNGLUNMAPBUFFERPROC UnmapBuffer=nullptr;
 PFNGLGENVERTEXARRAYSPROC GenVertexArrays=nullptr;
 PFNGLBINDVERTEXARRAYPROC BindVertexArray=nullptr;
 PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays=nullptr;
 PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray=nullptr;
 PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray=nullptr;
 PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer=nullptr;
 PFNGLVERTEXATTRIB4FPROC VertexAttrib4f=nullptr;
 PFNGLVERTEXATTRIBDIVISORPROC VertexAttribDivisor=nullptr;
 PFNGLDRAWELEMENTSINSTANCEDPROC DrawElementsInstanced=nullptr;
 PFNGLDRAWARRAYSINSTANCEDPROC DrawArraysInstanced=nullptr;
 PFNGLDRAWELEMENTSBASEVERTEXPROC DrawElementsBaseVertex=nullptr;
 PFNGLCREATESHADERPROC CreateShader=nullptr;
 PFNGLSHADERSOURCEPROC ShaderSource=nullptr;
 PFNGLCOMPILESHADERPROC CompileShader=nullptr;
 PFNGLGETSHADERIVPROC GetShaderiv=nullptr;
 PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog=nullptr;
 PFNGLDELETESHADERPROC DeleteShader=nullptr;
 PFNGLCREATEPROGRAMPROC CreateProgram=nullptr;
 PFNGLATTACHSHADERPROC AttachShader=nullptr;
 PFNGLDETACHSHADERPROC DetachShader=nullptr;
 PFNGLLINKPROGRAMPROC LinkProgram=nullptr;
 PFNGLGETPROGRAMIVPROC GetProgramiv=nullptr;
 PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog=nullptr;
 PFNGLDELETEPROGRAMPROC DeleteProgram=nullptr;
 PFNGLUSEPROGRAMPROC UseProgram=nullptr;
 PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation=nullptr;
 PFNGLGETATTRIBLOCATIONPROC GetAttribLocation=nullptr;
 PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation=nullptr;
 PFNGLUNIFORM1IPROC Uniform1i=nullptr;
 PFNGLUNIFORM1FPROC Uniform1f=nullptr;
 PFNGLUNIFORM2IPROC Uniform2i=nullptr;
 PFNGLUNIFORM2FPROC Uniform2f=nullptr;
 PFNGLUNIFORM3IPROC Uniform3i=nullptr;
 PFNGLUNIFORM3FPROC Uniform3f=nullptr;
 PFNGLUNIFORM4IPROC Uniform4i=nullptr;
 PFNGLUNIFORM4FPROC Uniform4f=nullptr;
 PFNGLUNIFORM3FVPROC Uniform3fv=nullptr;
 PFNGLUNIFORM4FVPROC Uniform4fv=nullptr;
 PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv=nullptr;
 PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv=nullptr;
 PFNGLACTIVETEXTUREPROC ActiveTexture=nullptr;
 PFNGLGENERATEMIPMAPPROC GenerateMipmap=nullptr;
 PFNGLCOMPRESSEDTEXIMAGE2DPROC CompressedTexImage2D=nullptr;
 PFNGLGENFRAMEBUFFERSPROC GenFramebuffers=nullptr;
 PFNGLBINDFRAMEBUFFERPROC BindFramebuffer=nullptr;
 PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers=nullptr;
 PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D=nullptr;
 PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus=nullptr;
 PFNGLGENRENDERBUFFERSPROC GenRenderbuffers=nullptr;
 PFNGLBINDRENDERBUFFERPROC BindRenderbuffer=nullptr;
 PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers=nullptr;
 PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage=nullptr;
 PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer=nullptr;
 PFNGLBLITFRAMEBUFFERPROC BlitFramebuffer=nullptr;
 PFNGLDRAWBUFFERSPROC DrawBuffers=nullptr;
 PFNGLGETSTRINGIPROC GetStringi=nullptr;
 PFNGLGENSAMPLERSPROC GenSamplers=nullptr;
 PFNGLDELETESAMPLERSPROC DeleteSamplers=nullptr;
 PFNGLBINDSAMPLERPROC BindSampler=nullptr;
 PFNGLSAMPLERPARAMETERIPROC SamplerParameteri=nullptr;
 PFNGLSAMPLERPARAMETERFPROC SamplerParameterf=nullptr;
 PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate=nullptr;
 PFNGLBLENDEQUATIONPROC BlendEquation=nullptr;
 PFNGLBLENDEQUATIONSEPARATEPROC BlendEquationSeparate=nullptr;
};
inline GLApi gl;
inline PROC gl_address(const char*name){auto p=wglGetProcAddress(name);if(!p||p==reinterpret_cast<PROC>(1)||p==reinterpret_cast<PROC>(2)||p==reinterpret_cast<PROC>(3)||p==reinterpret_cast<PROC>(-1)){static HMODULE module=GetModuleHandleW(L"opengl32.dll");p=module?GetProcAddress(module,name):nullptr;}return p;}
inline void gl_load(){
 gl.GenBuffers=reinterpret_cast<PFNGLGENBUFFERSPROC>(gl_address("glGenBuffers"));if(!gl.GenBuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glGenBuffers");
 gl.BindBuffer=reinterpret_cast<PFNGLBINDBUFFERPROC>(gl_address("glBindBuffer"));if(!gl.BindBuffer)throw std::runtime_error("OpenGL3.3 entry point missing: glBindBuffer");
 gl.BufferData=reinterpret_cast<PFNGLBUFFERDATAPROC>(gl_address("glBufferData"));if(!gl.BufferData)throw std::runtime_error("OpenGL3.3 entry point missing: glBufferData");
 gl.BufferSubData=reinterpret_cast<PFNGLBUFFERSUBDATAPROC>(gl_address("glBufferSubData"));if(!gl.BufferSubData)throw std::runtime_error("OpenGL3.3 entry point missing: glBufferSubData");
 gl.DeleteBuffers=reinterpret_cast<PFNGLDELETEBUFFERSPROC>(gl_address("glDeleteBuffers"));if(!gl.DeleteBuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteBuffers");
 gl.GetBufferParameteriv=reinterpret_cast<PFNGLGETBUFFERPARAMETERIVPROC>(gl_address("glGetBufferParameteriv"));if(!gl.GetBufferParameteriv)throw std::runtime_error("OpenGL3.3 entry point missing: glGetBufferParameteriv");
 gl.MapBufferRange=reinterpret_cast<PFNGLMAPBUFFERRANGEPROC>(gl_address("glMapBufferRange"));if(!gl.MapBufferRange)throw std::runtime_error("OpenGL3.3 entry point missing: glMapBufferRange");
 gl.UnmapBuffer=reinterpret_cast<PFNGLUNMAPBUFFERPROC>(gl_address("glUnmapBuffer"));if(!gl.UnmapBuffer)throw std::runtime_error("OpenGL3.3 entry point missing: glUnmapBuffer");
 gl.GenVertexArrays=reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(gl_address("glGenVertexArrays"));if(!gl.GenVertexArrays)throw std::runtime_error("OpenGL3.3 entry point missing: glGenVertexArrays");
 gl.BindVertexArray=reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(gl_address("glBindVertexArray"));if(!gl.BindVertexArray)throw std::runtime_error("OpenGL3.3 entry point missing: glBindVertexArray");
 gl.DeleteVertexArrays=reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(gl_address("glDeleteVertexArrays"));if(!gl.DeleteVertexArrays)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteVertexArrays");
 gl.EnableVertexAttribArray=reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(gl_address("glEnableVertexAttribArray"));if(!gl.EnableVertexAttribArray)throw std::runtime_error("OpenGL3.3 entry point missing: glEnableVertexAttribArray");
 gl.DisableVertexAttribArray=reinterpret_cast<PFNGLDISABLEVERTEXATTRIBARRAYPROC>(gl_address("glDisableVertexAttribArray"));if(!gl.DisableVertexAttribArray)throw std::runtime_error("OpenGL3.3 entry point missing: glDisableVertexAttribArray");
 gl.VertexAttribPointer=reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(gl_address("glVertexAttribPointer"));if(!gl.VertexAttribPointer)throw std::runtime_error("OpenGL3.3 entry point missing: glVertexAttribPointer");
 gl.VertexAttrib4f=reinterpret_cast<PFNGLVERTEXATTRIB4FPROC>(gl_address("glVertexAttrib4f"));if(!gl.VertexAttrib4f)throw std::runtime_error("OpenGL3.3 entry point missing: glVertexAttrib4f");
 gl.VertexAttribDivisor=reinterpret_cast<PFNGLVERTEXATTRIBDIVISORPROC>(gl_address("glVertexAttribDivisor"));if(!gl.VertexAttribDivisor)throw std::runtime_error("OpenGL3.3 entry point missing: glVertexAttribDivisor");
 gl.DrawElementsInstanced=reinterpret_cast<PFNGLDRAWELEMENTSINSTANCEDPROC>(gl_address("glDrawElementsInstanced"));if(!gl.DrawElementsInstanced)throw std::runtime_error("OpenGL3.3 entry point missing: glDrawElementsInstanced");
 gl.DrawArraysInstanced=reinterpret_cast<PFNGLDRAWARRAYSINSTANCEDPROC>(gl_address("glDrawArraysInstanced"));if(!gl.DrawArraysInstanced)throw std::runtime_error("OpenGL3.3 entry point missing: glDrawArraysInstanced");
 gl.DrawElementsBaseVertex=reinterpret_cast<PFNGLDRAWELEMENTSBASEVERTEXPROC>(gl_address("glDrawElementsBaseVertex"));if(!gl.DrawElementsBaseVertex)throw std::runtime_error("OpenGL3.3 entry point missing: glDrawElementsBaseVertex");
 gl.CreateShader=reinterpret_cast<PFNGLCREATESHADERPROC>(gl_address("glCreateShader"));if(!gl.CreateShader)throw std::runtime_error("OpenGL3.3 entry point missing: glCreateShader");
 gl.ShaderSource=reinterpret_cast<PFNGLSHADERSOURCEPROC>(gl_address("glShaderSource"));if(!gl.ShaderSource)throw std::runtime_error("OpenGL3.3 entry point missing: glShaderSource");
 gl.CompileShader=reinterpret_cast<PFNGLCOMPILESHADERPROC>(gl_address("glCompileShader"));if(!gl.CompileShader)throw std::runtime_error("OpenGL3.3 entry point missing: glCompileShader");
 gl.GetShaderiv=reinterpret_cast<PFNGLGETSHADERIVPROC>(gl_address("glGetShaderiv"));if(!gl.GetShaderiv)throw std::runtime_error("OpenGL3.3 entry point missing: glGetShaderiv");
 gl.GetShaderInfoLog=reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(gl_address("glGetShaderInfoLog"));if(!gl.GetShaderInfoLog)throw std::runtime_error("OpenGL3.3 entry point missing: glGetShaderInfoLog");
 gl.DeleteShader=reinterpret_cast<PFNGLDELETESHADERPROC>(gl_address("glDeleteShader"));if(!gl.DeleteShader)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteShader");
 gl.CreateProgram=reinterpret_cast<PFNGLCREATEPROGRAMPROC>(gl_address("glCreateProgram"));if(!gl.CreateProgram)throw std::runtime_error("OpenGL3.3 entry point missing: glCreateProgram");
 gl.AttachShader=reinterpret_cast<PFNGLATTACHSHADERPROC>(gl_address("glAttachShader"));if(!gl.AttachShader)throw std::runtime_error("OpenGL3.3 entry point missing: glAttachShader");
 gl.DetachShader=reinterpret_cast<PFNGLDETACHSHADERPROC>(gl_address("glDetachShader"));if(!gl.DetachShader)throw std::runtime_error("OpenGL3.3 entry point missing: glDetachShader");
 gl.LinkProgram=reinterpret_cast<PFNGLLINKPROGRAMPROC>(gl_address("glLinkProgram"));if(!gl.LinkProgram)throw std::runtime_error("OpenGL3.3 entry point missing: glLinkProgram");
 gl.GetProgramiv=reinterpret_cast<PFNGLGETPROGRAMIVPROC>(gl_address("glGetProgramiv"));if(!gl.GetProgramiv)throw std::runtime_error("OpenGL3.3 entry point missing: glGetProgramiv");
 gl.GetProgramInfoLog=reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(gl_address("glGetProgramInfoLog"));if(!gl.GetProgramInfoLog)throw std::runtime_error("OpenGL3.3 entry point missing: glGetProgramInfoLog");
 gl.DeleteProgram=reinterpret_cast<PFNGLDELETEPROGRAMPROC>(gl_address("glDeleteProgram"));if(!gl.DeleteProgram)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteProgram");
 gl.UseProgram=reinterpret_cast<PFNGLUSEPROGRAMPROC>(gl_address("glUseProgram"));if(!gl.UseProgram)throw std::runtime_error("OpenGL3.3 entry point missing: glUseProgram");
 gl.GetUniformLocation=reinterpret_cast<PFNGLGETUNIFORMLOCATIONPROC>(gl_address("glGetUniformLocation"));if(!gl.GetUniformLocation)throw std::runtime_error("OpenGL3.3 entry point missing: glGetUniformLocation");
 gl.GetAttribLocation=reinterpret_cast<PFNGLGETATTRIBLOCATIONPROC>(gl_address("glGetAttribLocation"));if(!gl.GetAttribLocation)throw std::runtime_error("OpenGL3.3 entry point missing: glGetAttribLocation");
 gl.BindAttribLocation=reinterpret_cast<PFNGLBINDATTRIBLOCATIONPROC>(gl_address("glBindAttribLocation"));if(!gl.BindAttribLocation)throw std::runtime_error("OpenGL3.3 entry point missing: glBindAttribLocation");
 gl.Uniform1i=reinterpret_cast<PFNGLUNIFORM1IPROC>(gl_address("glUniform1i"));if(!gl.Uniform1i)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform1i");
 gl.Uniform1f=reinterpret_cast<PFNGLUNIFORM1FPROC>(gl_address("glUniform1f"));if(!gl.Uniform1f)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform1f");
 gl.Uniform2i=reinterpret_cast<PFNGLUNIFORM2IPROC>(gl_address("glUniform2i"));if(!gl.Uniform2i)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform2i");
 gl.Uniform2f=reinterpret_cast<PFNGLUNIFORM2FPROC>(gl_address("glUniform2f"));if(!gl.Uniform2f)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform2f");
 gl.Uniform3i=reinterpret_cast<PFNGLUNIFORM3IPROC>(gl_address("glUniform3i"));if(!gl.Uniform3i)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform3i");
 gl.Uniform3f=reinterpret_cast<PFNGLUNIFORM3FPROC>(gl_address("glUniform3f"));if(!gl.Uniform3f)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform3f");
 gl.Uniform4i=reinterpret_cast<PFNGLUNIFORM4IPROC>(gl_address("glUniform4i"));if(!gl.Uniform4i)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform4i");
 gl.Uniform4f=reinterpret_cast<PFNGLUNIFORM4FPROC>(gl_address("glUniform4f"));if(!gl.Uniform4f)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform4f");
 gl.Uniform3fv=reinterpret_cast<PFNGLUNIFORM3FVPROC>(gl_address("glUniform3fv"));if(!gl.Uniform3fv)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform3fv");
 gl.Uniform4fv=reinterpret_cast<PFNGLUNIFORM4FVPROC>(gl_address("glUniform4fv"));if(!gl.Uniform4fv)throw std::runtime_error("OpenGL3.3 entry point missing: glUniform4fv");
 gl.UniformMatrix3fv=reinterpret_cast<PFNGLUNIFORMMATRIX3FVPROC>(gl_address("glUniformMatrix3fv"));if(!gl.UniformMatrix3fv)throw std::runtime_error("OpenGL3.3 entry point missing: glUniformMatrix3fv");
 gl.UniformMatrix4fv=reinterpret_cast<PFNGLUNIFORMMATRIX4FVPROC>(gl_address("glUniformMatrix4fv"));if(!gl.UniformMatrix4fv)throw std::runtime_error("OpenGL3.3 entry point missing: glUniformMatrix4fv");
 gl.ActiveTexture=reinterpret_cast<PFNGLACTIVETEXTUREPROC>(gl_address("glActiveTexture"));if(!gl.ActiveTexture)throw std::runtime_error("OpenGL3.3 entry point missing: glActiveTexture");
 gl.GenerateMipmap=reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(gl_address("glGenerateMipmap"));if(!gl.GenerateMipmap)throw std::runtime_error("OpenGL3.3 entry point missing: glGenerateMipmap");
 gl.CompressedTexImage2D=reinterpret_cast<PFNGLCOMPRESSEDTEXIMAGE2DPROC>(gl_address("glCompressedTexImage2D"));if(!gl.CompressedTexImage2D)throw std::runtime_error("OpenGL3.3 entry point missing: glCompressedTexImage2D");
 gl.GenFramebuffers=reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(gl_address("glGenFramebuffers"));if(!gl.GenFramebuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glGenFramebuffers");
 gl.BindFramebuffer=reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(gl_address("glBindFramebuffer"));if(!gl.BindFramebuffer)throw std::runtime_error("OpenGL3.3 entry point missing: glBindFramebuffer");
 gl.DeleteFramebuffers=reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(gl_address("glDeleteFramebuffers"));if(!gl.DeleteFramebuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteFramebuffers");
 gl.FramebufferTexture2D=reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(gl_address("glFramebufferTexture2D"));if(!gl.FramebufferTexture2D)throw std::runtime_error("OpenGL3.3 entry point missing: glFramebufferTexture2D");
 gl.CheckFramebufferStatus=reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(gl_address("glCheckFramebufferStatus"));if(!gl.CheckFramebufferStatus)throw std::runtime_error("OpenGL3.3 entry point missing: glCheckFramebufferStatus");
 gl.GenRenderbuffers=reinterpret_cast<PFNGLGENRENDERBUFFERSPROC>(gl_address("glGenRenderbuffers"));if(!gl.GenRenderbuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glGenRenderbuffers");
 gl.BindRenderbuffer=reinterpret_cast<PFNGLBINDRENDERBUFFERPROC>(gl_address("glBindRenderbuffer"));if(!gl.BindRenderbuffer)throw std::runtime_error("OpenGL3.3 entry point missing: glBindRenderbuffer");
 gl.DeleteRenderbuffers=reinterpret_cast<PFNGLDELETERENDERBUFFERSPROC>(gl_address("glDeleteRenderbuffers"));if(!gl.DeleteRenderbuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteRenderbuffers");
 gl.RenderbufferStorage=reinterpret_cast<PFNGLRENDERBUFFERSTORAGEPROC>(gl_address("glRenderbufferStorage"));if(!gl.RenderbufferStorage)throw std::runtime_error("OpenGL3.3 entry point missing: glRenderbufferStorage");
 gl.FramebufferRenderbuffer=reinterpret_cast<PFNGLFRAMEBUFFERRENDERBUFFERPROC>(gl_address("glFramebufferRenderbuffer"));if(!gl.FramebufferRenderbuffer)throw std::runtime_error("OpenGL3.3 entry point missing: glFramebufferRenderbuffer");
 gl.BlitFramebuffer=reinterpret_cast<PFNGLBLITFRAMEBUFFERPROC>(gl_address("glBlitFramebuffer"));if(!gl.BlitFramebuffer)throw std::runtime_error("OpenGL3.3 entry point missing: glBlitFramebuffer");
 gl.DrawBuffers=reinterpret_cast<PFNGLDRAWBUFFERSPROC>(gl_address("glDrawBuffers"));if(!gl.DrawBuffers)throw std::runtime_error("OpenGL3.3 entry point missing: glDrawBuffers");
 gl.GetStringi=reinterpret_cast<PFNGLGETSTRINGIPROC>(gl_address("glGetStringi"));if(!gl.GetStringi)throw std::runtime_error("OpenGL3.3 entry point missing: glGetStringi");
 gl.GenSamplers=reinterpret_cast<PFNGLGENSAMPLERSPROC>(gl_address("glGenSamplers"));if(!gl.GenSamplers)throw std::runtime_error("OpenGL3.3 entry point missing: glGenSamplers");
 gl.DeleteSamplers=reinterpret_cast<PFNGLDELETESAMPLERSPROC>(gl_address("glDeleteSamplers"));if(!gl.DeleteSamplers)throw std::runtime_error("OpenGL3.3 entry point missing: glDeleteSamplers");
 gl.BindSampler=reinterpret_cast<PFNGLBINDSAMPLERPROC>(gl_address("glBindSampler"));if(!gl.BindSampler)throw std::runtime_error("OpenGL3.3 entry point missing: glBindSampler");
 gl.SamplerParameteri=reinterpret_cast<PFNGLSAMPLERPARAMETERIPROC>(gl_address("glSamplerParameteri"));if(!gl.SamplerParameteri)throw std::runtime_error("OpenGL3.3 entry point missing: glSamplerParameteri");
 gl.SamplerParameterf=reinterpret_cast<PFNGLSAMPLERPARAMETERFPROC>(gl_address("glSamplerParameterf"));if(!gl.SamplerParameterf)throw std::runtime_error("OpenGL3.3 entry point missing: glSamplerParameterf");
 gl.BlendFuncSeparate=reinterpret_cast<PFNGLBLENDFUNCSEPARATEPROC>(gl_address("glBlendFuncSeparate"));if(!gl.BlendFuncSeparate)throw std::runtime_error("OpenGL3.3 entry point missing: glBlendFuncSeparate");
 gl.BlendEquation=reinterpret_cast<PFNGLBLENDEQUATIONPROC>(gl_address("glBlendEquation"));if(!gl.BlendEquation)throw std::runtime_error("OpenGL3.3 entry point missing: glBlendEquation");
 gl.BlendEquationSeparate=reinterpret_cast<PFNGLBLENDEQUATIONSEPARATEPROC>(gl_address("glBlendEquationSeparate"));if(!gl.BlendEquationSeparate)throw std::runtime_error("OpenGL3.3 entry point missing: glBlendEquationSeparate");
}
inline GLuint gl_program(const char*vertex,const char*fragment,const char*label){
 auto compile=[&](GLenum type,const char*source){GLuint shader=gl.CreateShader(type);gl.ShaderSource(shader,1,&source,nullptr);gl.CompileShader(shader);GLint ok=0;gl.GetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){GLint count=0;gl.GetShaderiv(shader,GL_INFO_LOG_LENGTH,&count);std::string log(size_t(std::max(1,count)),char(0));gl.GetShaderInfoLog(shader,count,nullptr,log.data());gl.DeleteShader(shader);throw std::runtime_error(std::string(label)+": "+log);}return shader;};
 GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=0,program=0;try{fs=compile(GL_FRAGMENT_SHADER,fragment);program=gl.CreateProgram();gl.AttachShader(program,vs);gl.AttachShader(program,fs);gl.LinkProgram(program);GLint ok=0;gl.GetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){GLint count=0;gl.GetProgramiv(program,GL_INFO_LOG_LENGTH,&count);std::string log(size_t(std::max(1,count)),char(0));gl.GetProgramInfoLog(program,count,nullptr,log.data());throw std::runtime_error(std::string(label)+": "+log);}gl.DetachShader(program,vs);gl.DetachShader(program,fs);gl.DeleteShader(vs);gl.DeleteShader(fs);return program;}catch(...){gl.DeleteShader(vs);if(fs)gl.DeleteShader(fs);if(program)gl.DeleteProgram(program);throw;}
}
class GLWindow {
 HWND window_=nullptr;HDC dc_=nullptr;HGLRC context_=nullptr;int width_=1,height_=1;std::string version_,vendor_,renderer_;bool vsync_=false,vsync_supported_=false;
 void close(){if(context_){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context_);context_=nullptr;}if(dc_){ReleaseDC(window_,dc_);dc_=nullptr;}}
public:
 explicit GLWindow(HWND window):window_(window){try{dc_=GetDC(window);if(!dc_)throw std::runtime_error("OpenGL window DC unavailable");PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof(p);p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;p.cAlphaBits=8;p.cDepthBits=24;p.cStencilBits=8;p.iLayerType=PFD_MAIN_PLANE;int format=ChoosePixelFormat(dc_,&p);if(!format||!SetPixelFormat(dc_,format,&p))throw std::runtime_error("OpenGL pixel format unavailable");context_=wglCreateContext(dc_);if(!context_||!wglMakeCurrent(dc_,context_))throw std::runtime_error("OpenGL context creation failed");
 using Create=HGLRC(WINAPI*)(HDC,HGLRC,const int*);auto create=reinterpret_cast<Create>(gl_address("wglCreateContextAttribsARB"));if(create){int attrs[]={0x2091,3,0x2092,3,0x9126,1,0};HGLRC modern=create(dc_,nullptr,attrs);if(modern){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context_);context_=modern;if(!wglMakeCurrent(dc_,context_))throw std::runtime_error("OpenGL3.3 context binding failed");}}
 const auto*v=glGetString(GL_VERSION);version_=v?reinterpret_cast<const char*>(v):"unknown";int major=0,minor=0;std::sscanf(version_.c_str(),"%d.%d",&major,&minor);if(major<3||(major==3&&minor<3))throw std::runtime_error("OpenGL3.3 required; reported "+version_+". Install the GPU driver.");gl_load();auto str=[](GLenum e){auto*p=glGetString(e);return p?std::string(reinterpret_cast<const char*>(p)):std::string();};vendor_=str(GL_VENDOR);renderer_=str(GL_RENDERER);set_vsync(false);RECT r{};GetClientRect(window,&r);resize(r.right-r.left,r.bottom-r.top);}catch(...){close();throw;}}
 ~GLWindow(){close();}GLWindow(const GLWindow&)=delete;GLWindow&operator=(const GLWindow&)=delete;
 bool set_vsync(bool enabled){if(enabled==vsync_&&vsync_supported_)return true;using Swap=BOOL(WINAPI*)(int);auto swap=reinterpret_cast<Swap>(gl_address("wglSwapIntervalEXT"));vsync_supported_=swap!=nullptr;if(!swap)return false;if(!swap(enabled?1:0))return false;vsync_=enabled;return true;}
 void resize(int w,int h){width_=std::max(1,w);height_=std::max(1,h);glViewport(0,0,width_,height_);}int width()const{return width_;}int height()const{return height_;}bool present(){return SwapBuffers(dc_)!=FALSE;}
 Json diagnostics()const{return{{"backend","OpenGL3.3 / WGL"},{"version",version_},{"vendor",vendor_},{"adapter",renderer_},{"backbuffer",{width_,height_}},{"pixel_format","RGBA8 / depth24 / stencil8 / double buffer"},{"vsync",vsync_},{"vsync_supported",vsync_supported_}};}
};
}
