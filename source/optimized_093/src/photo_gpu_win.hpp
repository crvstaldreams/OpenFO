#pragma once
#include "photo_mode.hpp"
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

namespace ofo {
struct PhotoPixels {
 unsigned width=0,height=0;
 std::vector<uint32_t> argb;
};

// The renderer calls begin before BeginScene, finish before its HUD, and capture
// after EndScene. Invalidate its state cache after finish: this pass restores the
// D3D state block, but DrawPrimitiveUP also invalidates stream zero bindings.
class PhotoGpu {
 template<class T> struct Com {
  T* p=nullptr;
  ~Com(){reset();}
  Com()=default; Com(const Com&)=delete; Com&operator=(const Com&)=delete;
  void reset(){if(p){p->Release();p=nullptr;}}
  T** put(){reset();return &p;}
 };
 using Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
 HMODULE compiler_=nullptr;
 bool compiler_tried_=false,effects_failed_=false;
 Com<IDirect3DPixelShader9> shader_;
 Com<IDirect3DTexture9> texture_;
 Com<IDirect3DSurface9> surface_,backbuffer_;
 unsigned width_=0,height_=0;
 D3DFORMAT format_=D3DFMT_UNKNOWN;
 std::string error_;
 bool fail(const char*message){error_=message;return false;}
 bool finish_fail(const char*message){effects_failed_=true;return fail(message);}
 bool ensure_shader(IDirect3DDevice9*d){
  if(shader_.p)return true;
  if(compiler_tried_)return false;
  compiler_tried_=true;
  Compile compile=nullptr;
  for(auto name:{L"d3dcompiler_47.dll",L"d3dcompiler_43.dll"}){
   compiler_=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
   if(!compiler_)continue;
   auto address=GetProcAddress(compiler_,"D3DCompile");
   static_assert(sizeof(compile)==sizeof(address),"Windows function pointer size");
   std::memcpy(&compile,&address,sizeof(compile));
   if(compile)break;
   FreeLibrary(compiler_);compiler_=nullptr;
  }
  if(!compile)return fail("Photo effects unavailable: system shader compiler missing; capture remains available");
  static const char source[]=R"HLSL(
sampler2D scene:register(s0);
float4 controls:register(c0); // exposure multiplier, contrast, saturation, vignette
float4 choice:register(c1); // preset
float luminance(float3 c){return dot(c,float3(.2126,.7152,.0722));}
float4 main(float2 uv:TEXCOORD0):COLOR0{
 float3 c=tex2D(scene,uv).rgb;
 float l=luminance(c);
 if(choice.x>.5 && choice.x<1.5){c=lerp(l.xxx,c,1.25);c=(c-.5)*1.1+.5;}
 else if(choice.x<2.5 && choice.x>1.5)c=(c*.9+.05)*float3(1.04,1,.94);
 else if(choice.x<3.5 && choice.x>2.5)c=l.xxx;
 else if(choice.x<4.5 && choice.x>3.5)c=l*float3(1.12,1,.8);
 else if(choice.x>4.5)c*=float3(.9,1.02,1.12);
 c*=controls.x;
 c=(c-.5)*controls.y+.5;
 c=lerp(luminance(c).xxx,c,controls.z);
 float2 p=uv-.5;
 c*=1-controls.w*saturate(2*dot(p,p));
 return float4(saturate(c),1);
})HLSL";
  Com<ID3DBlob> code,diagnostics;
  D3DCAPS9 caps{};const char*profile=SUCCEEDED(d->GetDeviceCaps(&caps))&&caps.PixelShaderVersion>=D3DPS_VERSION(3,0)?"ps_3_0":"ps_2_0";
  HRESULT result=compile(source,sizeof(source)-1,"photo_effect",nullptr,nullptr,"main",profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code.put(),diagnostics.put());
  if(FAILED(result)||!code.p)return fail("Photo effects unavailable: shader compilation failed; capture remains available");
  if(FAILED(d->CreatePixelShader(static_cast<const DWORD*>(code.p->GetBufferPointer()),shader_.put())))return fail("Photo effects unavailable: pixel shader creation failed; capture remains available");
  return true;
 }
public:
 PhotoGpu()=default; PhotoGpu(const PhotoGpu&)=delete; PhotoGpu&operator=(const PhotoGpu&)=delete;
 ~PhotoGpu(){reset();if(compiler_)FreeLibrary(compiler_);}
 const std::string&last_error()const{return error_;}
 bool effects_available()const{return shader_.p!=nullptr&&!effects_failed_;}
 // Required before ResetEx; no saved backbuffer reference may survive a reset.
 void reset(){backbuffer_.reset();surface_.reset();texture_.reset();shader_.reset();width_=height_=0;format_=D3DFMT_UNKNOWN;compiler_tried_=false;effects_failed_=false;if(compiler_){FreeLibrary(compiler_);compiler_=nullptr;}}
 bool begin(IDirect3DDevice9*d,unsigned width,unsigned height){
  if(backbuffer_.p)return fail("Photo pass already active");
  if(!d||!width||!height)return fail("Photo render target has no dimensions");
  if(effects_failed_||!ensure_shader(d))return false;
  Com<IDirect3DSurface9> target;
  if(FAILED(d->GetRenderTarget(0,target.put())))return fail("Photo backbuffer unavailable");
  D3DSURFACE_DESC desc{};
  if(FAILED(target.p->GetDesc(&desc))||desc.Width!=width||desc.Height!=height||desc.MultiSampleType!=D3DMULTISAMPLE_NONE)return fail("Photo render target size or sample count unsupported");
  if(!texture_.p||width_!=width||height_!=height||format_!=desc.Format){
   surface_.reset();texture_.reset();
   if(FAILED(d->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,texture_.put(),nullptr))||FAILED(texture_.p->GetSurfaceLevel(0,surface_.put()))){surface_.reset();texture_.reset();return fail("Photo render target allocation failed");}
   width_=width;height_=height;format_=desc.Format;
  }
  // Unbind the previous frame's source before binding its surface for rendering.
  d->SetTexture(0,nullptr);
  if(FAILED(d->SetRenderTarget(0,surface_.p)))return fail("Photo render target binding failed");
  backbuffer_.p=target.p;target.p=nullptr;
  error_.clear();return true;
 }
 bool finish(IDirect3DDevice9*d,PhotoSettings settings){
  if(!backbuffer_.p)return false;
  // Restore the window target even when state capture or the grading draw fails.
  HRESULT restored=d->SetRenderTarget(0,backbuffer_.p);
  backbuffer_.reset();
  if(FAILED(restored))return finish_fail("Photo backbuffer restore failed");
  Com<IDirect3DStateBlock9> states;
  Com<IDirect3DSurface9> depth;
  if(FAILED(d->CreateStateBlock(D3DSBT_ALL,states.put())))return finish_fail("Photo render state capture failed");
  d->GetDepthStencilSurface(depth.put());
  photo_sanitize(settings);
  float constants[4]={std::exp2(settings.exposure),settings.contrast,settings.saturation,settings.vignette};
  float preset[4]={static_cast<float>(settings.preset),0,0,0};
  struct Vertex {float x,y,z,rhw,u,v;};
  const Vertex quad[]={{-.5f,-.5f,0,1,0,0},{float(width_)-.5f,-.5f,0,1,1,0},{-.5f,float(height_)-.5f,0,1,0,1},{float(width_)-.5f,float(height_)-.5f,0,1,1,1}};
  bool ok=true;
  auto check=[&](HRESULT h){ok=SUCCEEDED(h)&&ok;};
  check(d->SetDepthStencilSurface(nullptr));
  D3DVIEWPORT9 viewport{0,0,width_,height_,0,1};check(d->SetViewport(&viewport));
  check(d->SetVertexShader(nullptr));check(d->SetPixelShader(shader_.p));
  check(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));check(d->SetTexture(0,texture_.p));
  check(d->SetRenderState(D3DRS_ZENABLE,FALSE));check(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));
  check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
  check(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));check(d->SetRenderState(D3DRS_WRAP0,0));
  check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));check(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE));
  check(d->SetRenderState(D3DRS_STENCILENABLE,FALSE));check(d->SetRenderState(D3DRS_FOGENABLE,FALSE));
  check(d->SetRenderState(D3DRS_COLORWRITEENABLE,15));check(d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE));
  check(d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));check(d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
  check(d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT));check(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT));
  check(d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));check(d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE));
  check(d->SetPixelShaderConstantF(0,constants,1));check(d->SetPixelShaderConstantF(1,preset,1));
  if(ok)check(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex)));
  check(d->SetTexture(0,nullptr));
  check(states.p->Apply());check(d->SetDepthStencilSurface(depth.p));
  if(!ok)return finish_fail("Photo grading pass or state restore failed");
  error_.clear();return true;
 }
 // Called only AFTER EndScene when finish failed. Copy the current world
 // ungraded, disable effects until reset, and never save an old/blank backbuffer.
 bool recover_ungraded(IDirect3DDevice9*d){
  if(!d||!surface_.p)return fail("Photo fallback has no completed scene");
  Com<IDirect3DSurface9> target;
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,target.put()))||FAILED(d->SetRenderTarget(0,target.p))||FAILED(d->StretchRect(surface_.p,nullptr,target.p,nullptr,D3DTEXF_NONE)))return fail("Photo fallback copy failed; screenshot not saved");
  return true;
 }
 bool capture(IDirect3DDevice9*d,PhotoPixels&output){
  output={};
  if(!d||backbuffer_.p)return fail("Photo capture requested before scene completion");
  Com<IDirect3DSurface9> source,readback;
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,source.put())))return fail("Photo capture backbuffer unavailable");
  D3DSURFACE_DESC desc{};
  if(FAILED(source.p->GetDesc(&desc))||(desc.Format!=D3DFMT_X8R8G8B8&&desc.Format!=D3DFMT_A8R8G8B8)||desc.MultiSampleType!=D3DMULTISAMPLE_NONE)return fail("Photo capture backbuffer format unsupported");
  if(FAILED(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,readback.put(),nullptr))||FAILED(d->GetRenderTargetData(source.p,readback.p)))return fail("Photo GPU readback failed");
  // Allocate before locking so allocation failure cannot leave a locked surface.
  std::vector<uint32_t> pixels(size_t(desc.Width)*desc.Height);
  D3DLOCKED_RECT locked{};
  if(FAILED(readback.p->LockRect(&locked,nullptr,D3DLOCK_READONLY)))return fail("Photo readback lock failed");
  for(unsigned y=0;y<desc.Height;++y){
   auto*row=pixels.data()+size_t(y)*desc.Width;
   std::memcpy(row,static_cast<const unsigned char*>(locked.pBits)+size_t(y)*locked.Pitch,size_t(desc.Width)*4);
   for(unsigned x=0;x<desc.Width;++x)row[x]|=0xff000000u;
  }
  HRESULT unlocked=readback.p->UnlockRect();
  if(FAILED(unlocked))return fail("Photo readback unlock failed");
  output.width=desc.Width;output.height=desc.Height;output.argb=std::move(pixels);
  if(!effects_failed_)error_.clear();return true;
 }
};
}
