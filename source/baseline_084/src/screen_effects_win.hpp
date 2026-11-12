#pragma once
#include "assets.hpp"
#include "screen_effects_policy.hpp"
#include "screen_effects_shaders.hpp"
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstring>
#include <string>
namespace ofo {
// Owns an opaque color + sampleable depth capture. Depth is sampled only after
// it is detached, and copied to the original depth surface by the composite.
// Thus water never samples a depth surface that is simultaneously bound for Z.
class ScreenEffects {
 template<class T>struct Com {T*p=nullptr;~Com(){reset();}void reset(){if(p)p->Release();p=nullptr;}T**put(){reset();return &p;}};
 using Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
 HMODULE compiler_=nullptr;Compile compile_=nullptr;
 Com<IDirect3DTexture9>color_,depth_,ao_;Com<IDirect3DSurface9>color_surface_,depth_surface_,ao_surface_,saved_target_,saved_depth_;
 Com<IDirect3DPixelShader9>ao_shader_,composite_shader_,water_shader_;Com<IDirect3DVertexShader9>water_vertex_;Com<IDirect3DVertexDeclaration9>water_declaration_;Com<IDirect3DStateBlock9>water_states_;
 unsigned width_=0,height_=0,ao_width_=0,ao_height_=0;D3DFORMAT color_format_=D3DFMT_UNKNOWN;
 bool touched_=false;bool capability_checked_=false,supported_=false,failed_=false,water_failed_=false,active_=false,resolved_=false;
 float weather_[4]{},sun_screen_[4]{},atmosphere_[4]{},post_controls_[4]{1,0,0,0};ScreenEffectsBudget budget_{};Mat water_view_=identity(),water_projection_=identity();uint64_t frames_=0;unsigned ao_draws_=0,ssr_draws_=0;std::string error_,capability_="not checked";
 bool fail(const std::string&s){error_=s;return false;}
 void clear_targets(){ao_surface_.reset();depth_surface_.reset();color_surface_.reset();ao_.reset();depth_.reset();color_.reset();width_=height_=ao_width_=ao_height_=0;}
 bool programs(IDirect3DDevice9*d){
  if(ao_shader_.p&&composite_shader_.p&&water_shader_.p&&water_vertex_.p)return true;
  if(!compile_){for(auto name:{L"d3dcompiler_47.dll",L"d3dcompiler_43.dll"}){compiler_=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!compiler_)continue;auto address=GetProcAddress(compiler_,"D3DCompile");std::memcpy(&compile_,&address,sizeof(compile_));if(compile_)break;FreeLibrary(compiler_);compiler_=nullptr;}}
  if(!compile_)return fail("Screen effects: system shader compiler unavailable");
  auto build=[&](const char*entry,const char*profile,ID3DBlob**out){Com<ID3DBlob>messages;auto source=screen_effects_hlsl();auto h=compile_(source,std::strlen(source),"openfo_screen_effects",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,out,messages.put());if(FAILED(h)){std::string detail=messages.p?std::string(static_cast<const char*>(messages.p->GetBufferPointer()),std::min<size_t>(messages.p->GetBufferSize(),2048)):"compiler rejected shader";return fail(std::string(entry)+": "+detail);}return true;};
  Com<ID3DBlob>code;
  if(!build("ao_ps","ps_3_0",code.put())||FAILED(d->CreatePixelShader(static_cast<const DWORD*>(code.p->GetBufferPointer()),ao_shader_.put())))return fail(error_.empty()?"AO shader creation failed":error_);
  if(!build("composite_ps","ps_3_0",code.put())||FAILED(d->CreatePixelShader(static_cast<const DWORD*>(code.p->GetBufferPointer()),composite_shader_.put())))return fail(error_.empty()?"Composite shader creation failed":error_);
  if(!build("water_vs","vs_3_0",code.put())||FAILED(d->CreateVertexShader(static_cast<const DWORD*>(code.p->GetBufferPointer()),water_vertex_.put())))return fail(error_.empty()?"SSR vertex shader creation failed":error_);
  if(!build("water_ps","ps_3_0",code.put())||FAILED(d->CreatePixelShader(static_cast<const DWORD*>(code.p->GetBufferPointer()),water_shader_.put())))return fail(error_.empty()?"SSR pixel shader creation failed":error_);
  D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},{0,28,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
  if(FAILED(d->CreateVertexDeclaration(elements,water_declaration_.put())))return fail("SSR vertex declaration creation failed");return true;
 }
 static void rows(const Mat&m,float*out){for(int r=0;r<4;r++)for(int c=0;c<4;c++)out[r*4+c]=m[c*4+r];}
 static void normal_rows(const Mat&m,float*out){
  Vec a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[8],m[9],m[10]};float det=dot(a,cross(b,c));
  if(std::abs(det)<1e-9f){out[0]=out[5]=out[10]=1;return;}
  Vec x=cross(b,c)/det,y=cross(c,a)/det,z=cross(a,b)/det;
  for(int r=0;r<3;r++){out[r*4]=x[r];out[r*4+1]=y[r];out[r*4+2]=z[r];}
 }
 static bool sample_state(IDirect3DDevice9*d,unsigned slot,D3DTEXTUREFILTERTYPE filter){bool ok=true;auto set=[&](D3DSAMPLERSTATETYPE key,DWORD value){ok=SUCCEEDED(d->SetSamplerState(slot,key,value))&&ok;};set(D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);set(D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);set(D3DSAMP_MINFILTER,filter);set(D3DSAMP_MAGFILTER,filter);set(D3DSAMP_MIPFILTER,D3DTEXF_NONE);set(D3DSAMP_SRGBTEXTURE,FALSE);return ok;}
 static bool fullscreen_state(IDirect3DDevice9*d){bool ok=SUCCEEDED(d->SetVertexShader(nullptr))&&SUCCEEDED(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));auto set=[&](D3DRENDERSTATETYPE key,DWORD value){ok=SUCCEEDED(d->SetRenderState(key,value))&&ok;};set(D3DRS_ZENABLE,FALSE);set(D3DRS_ZWRITEENABLE,FALSE);set(D3DRS_ALPHABLENDENABLE,FALSE);set(D3DRS_ALPHATESTENABLE,FALSE);set(D3DRS_CULLMODE,D3DCULL_NONE);set(D3DRS_SCISSORTESTENABLE,FALSE);set(D3DRS_STENCILENABLE,FALSE);set(D3DRS_FOGENABLE,FALSE);set(D3DRS_FILLMODE,D3DFILL_SOLID);set(D3DRS_COLORWRITEENABLE,15);set(D3DRS_SRGBWRITEENABLE,FALSE);set(D3DRS_WRAP0,0);return ok;}
 static bool fullscreen(IDirect3DDevice9*d,unsigned w,unsigned h){struct V{float x,y,z,rhw,u,v;};V v[]={{-.5f,-.5f,0,1,0,0},{float(w)-.5f,-.5f,0,1,1,0},{-.5f,float(h)-.5f,0,1,0,1},{float(w)-.5f,float(h)-.5f,0,1,1,1}};D3DVIEWPORT9 viewport{0,0,w,h,0,1};return SUCCEEDED(d->SetViewport(&viewport))&&SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(V)));}
 void unbind(IDirect3DDevice9*d){for(unsigned i=0;i<3;i++)d->SetTexture(i,nullptr);}
public:
 ScreenEffects()=default;ScreenEffects(const ScreenEffects&)=delete;ScreenEffects&operator=(const ScreenEffects&)=delete;
 ~ScreenEffects(){reset(nullptr);if(compiler_)FreeLibrary(compiler_);}
 void reset(IDirect3DDevice9*d){if(d)end(d);water_states_.reset();saved_target_.reset();saved_depth_.reset();clear_targets();ao_shader_.reset();composite_shader_.reset();water_shader_.reset();water_vertex_.reset();water_declaration_.reset();active_=resolved_=failed_=water_failed_=capability_checked_=supported_=false;}
 void neutral_environment(){std::fill(weather_,weather_+4,0);std::fill(sun_screen_,sun_screen_+4,0);}
 void set_bloom_intensity(float value){post_controls_[0]=std::isfinite(value)?std::clamp(value,0.f,2.f):1.f;}
 float bloom_intensity()const{return post_controls_[0];}
 void configure_environment(bool night,bool rain,float intensity,float strength,Vec sun,const Mat&view,const Mat&projection,float time,bool cockpit=false){weather_[0]=1;weather_[1]=night?1:0;weather_[2]=rain?strength:0;weather_[3]=time;sun=direction(view,sun);bool front=sun.z>.01f;sun_screen_[0]=front?.5f+.5f*sun.x*projection[0]/sun.z:-1;sun_screen_[1]=front?.5f-.5f*sun.y*projection[5]/sun.z:-1;sun_screen_[2]=night?0:intensity;sun_screen_[3]=front?1:0;atmosphere_[2]=cockpit?1:0;}
 bool begin(IDirect3DDevice9*d,IDirect3D9Ex*api,unsigned w,unsigned h,int detail,bool enabled,bool ao_enabled=true,bool ssr_enabled=true){
  post_controls_[1]=detail>=1?1.f:0.f;post_controls_[2]=1.f/std::max(1u,w);post_controls_[3]=1.f/std::max(1u,h);atmosphere_[0]=detail>=3?8.f:4.f;atmosphere_[1]=detail>=2?.16f:.10f;atmosphere_[3]=float(w)/std::max(1u,h);touched_=false;ao_draws_=ssr_draws_=0;budget_=screen_effects_budget(detail);if(!ao_enabled)budget_.ao_samples=0;if(!ssr_enabled)budget_.ssr_steps=0;resolved_=false;
  if(active_)end(d);
  if(!enabled||!budget_.enabled){clear_targets();return false;}
  if(failed_)return false;
  if(!capability_checked_){capability_checked_=true;D3DCAPS9 caps{};D3DDISPLAYMODE mode{};constexpr D3DFORMAT intz=D3DFORMAT(unsigned('I')|(unsigned('N')<<8)|(unsigned('T')<<16)|(unsigned('Z')<<24));
   supported_=SUCCEEDED(d->GetDeviceCaps(&caps))&&caps.PixelShaderVersion>=D3DPS_VERSION(3,0)&&caps.VertexShaderVersion>=D3DVS_VERSION(3,0)&&SUCCEEDED(api->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&mode))&&SUCCEEDED(api->CheckDeviceFormat(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,mode.Format,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_TEXTURE,intz));capability_=supported_?"INTZ depth texture + shader model 3":"INTZ or shader model 3 unsupported";}
  if(!supported_)return fail("AO/SSR unavailable: sampleable INTZ depth or shader model 3 unsupported; use OpenGL for depth effects on this device");
  if(!programs(d)){failed_=true;return false;}
  if(FAILED(d->GetRenderTarget(0,saved_target_.put()))||FAILED(d->GetDepthStencilSurface(saved_depth_.put()))){saved_target_.reset();saved_depth_.reset();return fail("Screen target capture failed");}
  D3DSURFACE_DESC desc{};if(FAILED(saved_target_.p->GetDesc(&desc))||desc.Width!=w||desc.Height!=h||desc.MultiSampleType!=D3DMULTISAMPLE_NONE){saved_target_.reset();saved_depth_.reset();return fail("Screen target dimensions/MSAA unsupported");}
  auto size=screen_effects_size(int(w),int(h),budget_.ao_divisor);
  if(!color_.p||width_!=w||height_!=h||color_format_!=desc.Format||ao_width_!=unsigned(size.first)||ao_height_!=unsigned(size.second)){
   clear_targets();width_=w;height_=h;ao_width_=unsigned(size.first);ao_height_=unsigned(size.second);color_format_=desc.Format;
   constexpr D3DFORMAT intz=D3DFORMAT(unsigned('I')|(unsigned('N')<<8)|(unsigned('T')<<16)|(unsigned('Z')<<24));
   bool ok=SUCCEEDED(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,color_.put(),nullptr))&&SUCCEEDED(color_.p->GetSurfaceLevel(0,color_surface_.put()))&&SUCCEEDED(d->CreateTexture(w,h,1,D3DUSAGE_DEPTHSTENCIL,intz,D3DPOOL_DEFAULT,depth_.put(),nullptr))&&SUCCEEDED(depth_.p->GetSurfaceLevel(0,depth_surface_.put()))&&SUCCEEDED(d->CreateTexture(ao_width_,ao_height_,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,ao_.put(),nullptr))&&SUCCEEDED(ao_.p->GetSurfaceLevel(0,ao_surface_.put()));
   if(!ok){failed_=true;saved_target_.reset();saved_depth_.reset();clear_targets();return fail("Screen effect allocation failed; original renderer retained");}
  }
  touched_=true;unbind(d);if(FAILED(d->SetRenderTarget(0,color_surface_.p))||FAILED(d->SetDepthStencilSurface(depth_surface_.p))){d->SetRenderTarget(0,saved_target_.p);d->SetDepthStencilSurface(saved_depth_.p);saved_target_.reset();saved_depth_.reset();failed_=true;return fail("INTZ target binding failed; original renderer retained");}
  active_=true;if(!water_failed_)error_.clear();return true;
 }
 bool resolve(IDirect3DDevice9*d,const Mat&projection){
  if(!active_)return false;if(resolved_)return true;
  Com<IDirect3DStateBlock9>states;if(FAILED(d->CreateStateBlock(D3DSBT_ALL,states.put()))){failed_=true;end(d);return fail("Screen effects state capture failed");}
  bool ok=true;auto check=[&](HRESULT h){ok=SUCCEEDED(h)&&ok;};unbind(d);check(d->SetDepthStencilSurface(nullptr));check(d->SetRenderTarget(0,ao_surface_.p));ok=fullscreen_state(d)&&ok;check(d->SetPixelShader(ao_shader_.p));check(d->SetTexture(1,depth_.p));ok=sample_state(d,1,D3DTEXF_POINT)&&ok;
  float constants[]={projection[0],projection[5],projection[10],projection[14],1.f/width_,1.f/height_,1.f/ao_width_,1.f/ao_height_,budget_.ao_radius,budget_.ao_strength,float(budget_.ao_samples),1};check(d->SetPixelShaderConstantF(0,constants,3));if(ok&&budget_.ao_samples>0){ok=fullscreen(d,ao_width_,ao_height_);if(ok)++ao_draws_;}
  // Restore both opaque color and its depth in one fullscreen pass. No depth
  // resource is simultaneously sampled and bound as a depth-stencil surface.
  bool ao_ok=ok;ok=true;unbind(d);check(d->SetRenderTarget(0,saved_target_.p));check(d->SetDepthStencilSurface(saved_depth_.p));ok=fullscreen_state(d)&&ok;check(d->SetRenderState(D3DRS_ZENABLE,TRUE));check(d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE));check(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_ALWAYS));check(d->SetPixelShader(composite_shader_.p));check(d->SetTexture(0,color_.p));check(d->SetTexture(1,depth_.p));check(d->SetTexture(2,ao_.p));for(unsigned i=0;i<3;i++)ok=sample_state(d,i,i==0?D3DTEXF_LINEAR:D3DTEXF_POINT)&&ok;constants[11]=ao_ok&&budget_.ao_samples>0?1.f:0.f;check(d->SetPixelShaderConstantF(0,constants,3));check(d->SetPixelShaderConstantF(21,weather_,1));check(d->SetPixelShaderConstantF(22,sun_screen_,1));check(d->SetPixelShaderConstantF(23,atmosphere_,1));check(d->SetPixelShaderConstantF(24,post_controls_,1));if(ok)ok=fullscreen(d,width_,height_);if(ok)++ao_draws_;
  unbind(d);bool restored=SUCCEEDED(states.p->Apply());restored=SUCCEEDED(d->SetRenderTarget(0,saved_target_.p))&&restored;restored=SUCCEEDED(d->SetDepthStencilSurface(saved_depth_.p))&&restored;
  if(!ok||!restored){failed_=true;return fail("AO composite/depth restoration failed; effects disabled until reset");}
  resolved_=true;++frames_;if(!ao_ok){water_failed_=true;return fail("AO draw failed; unshaded opaque color/depth restored");}return true;
 }
 bool begin_water(IDirect3DDevice9*d,const Mat&view,const Mat&projection,float time){
  if(!active_||!resolved_||water_failed_||budget_.ssr_steps<=0)return false;
  if(FAILED(d->CreateStateBlock(D3DSBT_ALL,water_states_.put())))return false;
  water_view_=view;water_projection_=projection;bool ok=true;auto check=[&](HRESULT h){ok=SUCCEEDED(h)&&ok;};
  check(d->SetVertexShader(water_vertex_.p));check(d->SetPixelShader(water_shader_.p));check(d->SetVertexDeclaration(water_declaration_.p));check(d->SetTexture(0,color_.p));check(d->SetTexture(1,depth_.p));check(d->SetTexture(2,nullptr));ok=sample_state(d,0,D3DTEXF_LINEAR)&&ok;ok=sample_state(d,1,D3DTEXF_POINT)&&ok;
  check(d->SetRenderState(D3DRS_ZENABLE,TRUE));check(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));check(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));check(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));check(d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD));check(d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));check(d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));check(d->SetRenderState(D3DRS_FOGENABLE,FALSE));check(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
  float c[]={projection[0],projection[5],projection[10],projection[14],time,float(budget_.ssr_steps),budget_.ssr_distance,budget_.ssr_strength,float(width_),float(height_),1.f/width_,1.f/height_,view[0],view[1],view[2],0,view[8],view[9],view[10],0};
  float up[]={view[4],view[5],view[6],0};check(d->SetPixelShaderConstantF(11,up,1));check(d->SetPixelShaderConstantF(12,c,5));if(!ok){end_water(d);water_failed_=true;return fail("SSR state binding failed");}return true;
 }
 bool draw_water(IDirect3DDevice9*d,const Mat&world,IDirect3DVertexBuffer9*vb,IDirect3DIndexBuffer9*ib,unsigned vertices,unsigned indices){
  if(!water_states_.p||!indices)return false;float c[21*4]{};rows(multiply(water_projection_,multiply(water_view_,world)),c);auto world_view=multiply(water_view_,world);rows(world_view,c+16);normal_rows(world_view,c+32);Vec up=direction(water_view_,{0,1,0});c[44]=up.x;c[45]=up.y;c[46]=up.z;rows(world,c+68);
  bool ok=SUCCEEDED(d->SetVertexShaderConstantF(0,c,12))&&SUCCEEDED(d->SetVertexShaderConstantF(17,c+68,4))&&SUCCEEDED(d->SetStreamSource(0,vb,0,sizeof(Vertex)))&&SUCCEEDED(d->SetIndices(ib))&&SUCCEEDED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,vertices,0,indices/3));
  if(ok)++ssr_draws_;else{water_failed_=true;fail("SSR water submission failed");}return ok;
 }
 void end_water(IDirect3DDevice9*d){if(water_states_.p){if(FAILED(water_states_.p->Apply())){water_failed_=true;fail("SSR state restore failed");}water_states_.reset();}}
 void end(IDirect3DDevice9*d){if(!d)return;end_water(d);if(active_){unbind(d);d->SetRenderTarget(0,saved_target_.p);d->SetDepthStencilSurface(saved_depth_.p);}saved_target_.reset();saved_depth_.reset();active_=false;}
 bool touched()const{return touched_;}
 unsigned extra_draws()const{return ao_draws_+ssr_draws_;}
 Json diagnostics()const{return{{"capability",capability_},{"quality_enabled",budget_.enabled},{"ao_resolution",{ao_width_,ao_height_}},{"ao_requested",budget_.enabled&&budget_.ao_samples>0},{"ao_enabled",supported_&&!failed_&&budget_.enabled&&budget_.ao_samples>0},{"ao_samples",budget_.ao_samples},{"ao_depth_reads",supported_&&budget_.ao_samples>0?budget_.ao_samples+5:0},{"ao_radius_m",budget_.ao_radius},{"ao_strength",budget_.ao_strength},{"ao_applied",resolved_&&ao_draws_==2&&!failed_},{"ssr_steps",budget_.ssr_steps},{"ao_composite_draws",ao_draws_},{"ssr_water_draws",ssr_draws_},{"completed_frames",frames_},{"additional_gpu_bytes",uint64_t(width_)*height_*8+uint64_t(ao_width_)*ao_height_*4},{"opaque_geometry_redraws",0},{"cpu_readback",false},{"disabled_after_failure",failed_},{"ssr_disabled_after_failure",water_failed_},{"error",error_},{"scope","reduced SSAO; current-frame opaque screen-space water reflection; no offscreen/transparent reflection or temporal history"}};}
};
}
