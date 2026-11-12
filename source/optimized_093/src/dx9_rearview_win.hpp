#pragma once
#include <d3d9.h>
#include <array>
#include <string>
namespace ofo {
// Fixed GPU-only mirror. State blocks exclude render targets/depth surfaces,
// so those and the viewport are saved explicitly, including active MRT slots.
class Dx9RearviewGpu {
 IDirect3DTexture9*color_=nullptr;IDirect3DSurface9*surface_=nullptr,*depth_=nullptr,*saved_depth_=nullptr;IDirect3DStateBlock9*saved_=nullptr;
 std::array<IDirect3DSurface9*,4>saved_targets_{};D3DVIEWPORT9 viewport_{};IDirect3DDevice9*device_=nullptr;UINT targets_=1;bool active_=false;std::string error_;
 template<class T>static void drop(T*&p){if(p){p->Release();p=nullptr;}}
 void release_saved(){drop(saved_);drop(saved_depth_);for(auto&p:saved_targets_)drop(p);device_=nullptr;}
public:
 Dx9RearviewGpu()=default;Dx9RearviewGpu(const Dx9RearviewGpu&)=delete;Dx9RearviewGpu&operator=(const Dx9RearviewGpu&)=delete;
 ~Dx9RearviewGpu(){reset();}
 void reset(){end();drop(depth_);drop(surface_);drop(color_);error_.clear();}
 bool begin(IDirect3DDevice9*d){
  if(active_||!d)return false;
  if(!color_){HRESULT result=d->CreateTexture(512,128,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&color_,nullptr);
   if(SUCCEEDED(result))result=color_->GetSurfaceLevel(0,&surface_);
   if(SUCCEEDED(result)){result=E_FAIL;for(auto format:{D3DFMT_D24S8,D3DFMT_D24X8,D3DFMT_D16}){result=d->CreateDepthStencilSurface(512,128,format,D3DMULTISAMPLE_NONE,0,TRUE,&depth_,nullptr);if(SUCCEEDED(result))break;}}
   if(FAILED(result)){reset();error_="Rearview color/depth allocation failed";return false;}
  }
  device_=d;D3DCAPS9 caps{};d->GetDeviceCaps(&caps);targets_=caps.NumSimultaneousRTs<4?caps.NumSimultaneousRTs:4;if(!targets_)targets_=1;
  if(FAILED(d->GetRenderTarget(0,&saved_targets_[0]))||FAILED(d->GetViewport(&viewport_))||FAILED(d->CreateStateBlock(D3DSBT_ALL,&saved_))||FAILED(saved_->Capture())){release_saved();error_="Rearview state capture failed";return false;}
  for(UINT i=1;i<targets_;++i)d->GetRenderTarget(i,&saved_targets_[i]);d->GetDepthStencilSurface(&saved_depth_);
  active_=true;
  // Unbind the previous HUD's mirror before using its texture as a render target.
  for(DWORD i=0;i<16;++i)d->SetTexture(i,nullptr);
  d->SetDepthStencilSurface(nullptr);for(UINT i=1;i<targets_;++i)d->SetRenderTarget(i,nullptr);
  D3DVIEWPORT9 mirror{0,0,512,128,0,1};HRESULT result=d->SetRenderTarget(0,surface_);
  if(SUCCEEDED(result))result=d->SetDepthStencilSurface(depth_);if(SUCCEEDED(result))result=d->SetViewport(&mirror);
  if(FAILED(result)){end();error_="Rearview target activation failed";return false;}
  d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);d->SetRenderState(D3DRS_STENCILENABLE,FALSE);d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);error_.clear();return true;
 }
 void end(){if(active_){device_->SetDepthStencilSurface(nullptr);for(UINT i=1;i<targets_;++i)device_->SetRenderTarget(i,nullptr);device_->SetRenderTarget(0,saved_targets_[0]);for(UINT i=1;i<targets_;++i)device_->SetRenderTarget(i,saved_targets_[i]);device_->SetDepthStencilSurface(saved_depth_);if(saved_)saved_->Apply();device_->SetViewport(&viewport_);active_=false;release_saved();}}
 IDirect3DTexture9*texture()const{return color_;}const std::string&error()const{return error_;}
};
}
