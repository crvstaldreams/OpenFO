#pragma once
#include "display_resolution_policy.hpp"
#include <d3d9.h>
#include <string>
namespace ofo {
// An offscreen scene only. HUD is drawn after the scaled result is copied to
// the native swapchain. Never change the game's physics or user-interface size.
class D3D9SceneScale {
 IDirect3DSurface9 *target_=nullptr,*depth_=nullptr,*old_target_=nullptr,*old_depth_=nullptr;
 D3DVIEWPORT9 old_viewport_{};
 int width_=0,height_=0,failed_w_=0,failed_h_=0;bool active_=false;std::string error_;
 static void release(IDirect3DSurface9*&p){if(p){p->Release();p=nullptr;}}
 void finish_resources(){release(old_target_);release(old_depth_);active_=false;}
public:
 ~D3D9SceneScale(){reset();}
 D3D9SceneScale(const D3D9SceneScale&)=delete;D3D9SceneScale&operator=(const D3D9SceneScale&)=delete;
 D3D9SceneScale()=default;
 void reset(){finish_resources();release(target_);release(depth_);width_=height_=failed_w_=failed_h_=0;}
 const std::string&error()const{return error_;}
 bool active()const{return active_;}
 int target_width()const{return width_;}int target_height()const{return height_;}
 bool begin(IDirect3DDevice9*d,int w,int h){
  if(!d||w<1||h<1||active_||(w==failed_w_&&h==failed_h_))return false;
  if(FAILED(d->GetRenderTarget(0,&old_target_))||FAILED(d->GetDepthStencilSurface(&old_depth_))||FAILED(d->GetViewport(&old_viewport_))){finish_resources();error_="Could not capture swapchain render targets";return false;}
  D3DSURFACE_DESC color{},z{};if(FAILED(old_target_->GetDesc(&color))||FAILED(old_depth_->GetDesc(&z))){finish_resources();return false;}
  if(!target_||w!=width_||h!=height_){release(target_);release(depth_);width_=height_=0;
   if(FAILED(d->CreateRenderTarget(w,h,color.Format,D3DMULTISAMPLE_NONE,0,FALSE,&target_,nullptr))||
      FAILED(d->CreateDepthStencilSurface(w,h,z.Format,D3DMULTISAMPLE_NONE,0,TRUE,&depth_,nullptr))){
    release(target_);release(depth_);finish_resources();failed_w_=w;failed_h_=h;error_="Scaled color/depth target not supported or out of video memory";return false;
   }width_=w;height_=h;
  }
  if(FAILED(d->SetRenderTarget(0,target_))||FAILED(d->SetDepthStencilSurface(depth_))){
   d->SetRenderTarget(0,old_target_);d->SetDepthStencilSurface(old_depth_);finish_resources();error_="Could not bind scaled render target";return false;
  }
  D3DVIEWPORT9 viewport{0,0,DWORD(w),DWORD(h),0.f,1.f};d->SetViewport(&viewport);active_=true;failed_w_=failed_h_=0;error_.clear();return true;
 }
 // D3D9 StretchRect must be called OUTSIDE the BeginScene/EndScene pair.
 bool composite(IDirect3DDevice9*d){
  if(!active_||!d)return false;
  d->SetRenderTarget(0,old_target_);d->SetDepthStencilSurface(old_depth_);d->SetViewport(&old_viewport_);
  HRESULT hr=d->StretchRect(target_,nullptr,old_target_,nullptr,D3DTEXF_LINEAR);
  if(FAILED(hr))hr=d->StretchRect(target_,nullptr,old_target_,nullptr,D3DTEXF_POINT);
  finish_resources();if(FAILED(hr))error_="GPU StretchRect scale composite failed";return SUCCEEDED(hr);
 }
};
}
