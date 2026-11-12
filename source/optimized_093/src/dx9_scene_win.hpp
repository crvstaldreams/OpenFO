#pragma once
#include "dx9_scene_shaders.hpp"
#include "scene_lighting.hpp"
#include "track_environment.hpp"
#include "vegetation.hpp"
#include <d3d9.h>
#include <d3dcompiler.h>
namespace ofo {
class Dx9Scene {
 IDirect3DVertexShader9*vs_=nullptr;IDirect3DPixelShader9*ps_=nullptr,*shadow_ps_=nullptr,*night_static_ps_=nullptr;IDirect3DTexture9*shadow_[2]{};IDirect3DSurface9*shadow_surface_[2]{},*shadow_depth_=nullptr,*saved_target_=nullptr,*saved_depth_=nullptr;D3DVIEWPORT9 saved_viewport_{};IDirect3DStateBlock9*saved_states_=nullptr;IDirect3DVertexDeclaration9*decl_=nullptr;HMODULE module_=nullptr;bool attempted_=false;UINT shadow_size_=0;
 using Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
 static void rows(const Mat&m,float*out){for(int r=0;r<4;r++)for(int c=0;c<4;c++)out[r*4+c]=m[c*4+r];}
public:
 std::array<IDirect3DTexture9*,6>sky{};bool sky_fullsphere=false;bool active=false;TrackEnvironment environment;std::array<Mat,8>poses{};std::array<std::array<float,4>,8>mounts{},lo{},hi{};int count=0;bool shadows_ready=false;std::string error;uint64_t draws=0;
 void reset_targets(){shadows_ready=false;shadow_size_=0;for(auto*&p:shadow_surface_){if(p)p->Release();p=nullptr;}for(auto*&p:shadow_){if(p)p->Release();p=nullptr;}if(shadow_depth_)shadow_depth_->Release();shadow_depth_=nullptr;}
 ~Dx9Scene(){reset_targets();if(shadow_ps_)shadow_ps_->Release();if(night_static_ps_)night_static_ps_->Release();if(vs_)vs_->Release();if(ps_)ps_->Release();if(decl_)decl_->Release();if(module_)FreeLibrary(module_);}
 void add(const Mat&pose,const VehicleConfig&cfg){if(count>=8)return;poses[count]=headlight_inverse_pose(pose);mounts[count]=headlight_mount(cfg);lo[count]={cfg.lo.x,cfg.lo.y,cfg.lo.z,0};hi[count]={cfg.hi.x,cfg.hi.y,cfg.hi.z,0};++count;}
 bool prepare(IDirect3DDevice9*d){if(attempted_)return vs_&&decl_;attempted_=true;Compile compile=nullptr;for(auto name:{L"d3dcompiler_47.dll",L"d3dcompiler_43.dll"}){module_=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module_)continue;auto address=GetProcAddress(module_,"D3DCompile");std::memcpy(&compile,&address,sizeof compile);if(compile)break;FreeLibrary(module_);module_=nullptr;}
 if(!compile){error="D3DCompile unavailable";return false;}auto build=[&](const char*entry,const char*profile,ID3DBlob**code){ID3DBlob*messages=nullptr;auto source=dx9_scene_hlsl();auto h=compile(source,std::strlen(source),"dx9_scene",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,code,&messages);if(messages){if(FAILED(h))error=std::string((char*)messages->GetBufferPointer(),messages->GetBufferSize());messages->Release();}return SUCCEEDED(h);};ID3DBlob*code=nullptr;if(!build("scene_vs","vs_3_0",&code))return false;auto h=d->CreateVertexShader((DWORD*)code->GetBufferPointer(),&vs_);code->Release();if(FAILED(h)){error="CreateVertexShader failed";return false;}auto build_pixel=[&](const char*entry,IDirect3DPixelShader9*& shader){
  ID3DBlob*bytecode=nullptr;
  if(!build(entry,"ps_3_0",&bytecode))return;
  auto result=d->CreatePixelShader((DWORD*)bytecode->GetBufferPointer(),&shader);
  bytecode->Release();
  if(FAILED(result))error=std::string("CreatePixelShader failed: ")+entry;
 };
 build_pixel("scene_ps",ps_);
 build_pixel("night_static_ps",night_static_ps_);
 build_pixel("shadow_ps",shadow_ps_);
 D3DVERTEXELEMENT9 e[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_D3DCOLOR,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0},{0,28,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},{0,36,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},D3DDECL_END()};if(FAILED(d->CreateVertexDeclaration(e,&decl_))){error="CreateVertexDeclaration failed";return false;}return true;}
 bool begin_shadow(IDirect3DDevice9*d,int side,UINT size=headlight_shadow_map_size){
  if(!prepare(d)||!shadow_ps_)return false;
  if(size!=shadow_size_){
   reset_targets();
   shadow_size_=size;
   bool ready=true;
   for(int i=0;i<2&&ready;++i)
    ready=SUCCEEDED(d->CreateTexture(size,size,1,D3DUSAGE_RENDERTARGET,D3DFMT_R32F,D3DPOOL_DEFAULT,&shadow_[i],nullptr))&&
          SUCCEEDED(shadow_[i]->GetSurfaceLevel(0,&shadow_surface_[i]));
   if(!ready||FAILED(d->CreateDepthStencilSurface(size,size,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&shadow_depth_,nullptr))){
    reset_targets();
    shadow_size_=size; // Avoid repeated allocation after an unsupported format/size.
    return false;
   }
  }
 if(!shadow_surface_[side]||!shadow_depth_)return false;
  if(FAILED(d->GetViewport(&saved_viewport_)))return false;
  if(FAILED(d->GetRenderTarget(0,&saved_target_)))return false;
  const auto depth_result=d->GetDepthStencilSurface(&saved_depth_);
  if((FAILED(depth_result)&&depth_result!=D3DERR_NOTFOUND)||FAILED(d->CreateStateBlock(D3DSBT_ALL,&saved_states_))){
   saved_target_->Release();saved_target_=nullptr;
   if(saved_depth_)saved_depth_->Release();saved_depth_=nullptr;
   return false;
  }
  bool ok=true;
  auto set=[&](HRESULT result){ok=SUCCEEDED(result)&&ok;};
  set(d->SetTexture(3,nullptr));set(d->SetTexture(4,nullptr));
  set(d->SetDepthStencilSurface(nullptr));set(d->SetRenderTarget(0,shadow_surface_[side]));set(d->SetDepthStencilSurface(shadow_depth_));
  const D3DVIEWPORT9 vp{0,0,shadow_size_,shadow_size_,0,1};
  set(d->SetViewport(&vp));set(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE));
  if(ok)set(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffffffff,1,0));
  set(d->SetRenderState(D3DRS_COLORWRITEENABLE,0xf));
  set(d->SetRenderState(D3DRS_ZENABLE,TRUE));set(d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE));set(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));
  set(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));set(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));set(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
  set(d->SetRenderState(D3DRS_FOGENABLE,FALSE));set(d->SetVertexShader(vs_));set(d->SetPixelShader(shadow_ps_));set(d->SetVertexDeclaration(decl_));
  float pose[16];rows(poses[0],pose);
  set(d->SetPixelShaderConstantF(32,pose,4));set(d->SetPixelShaderConstantF(64,mounts[0].data(),1));
  if(!ok)end_shadow(d);
  return ok;
 }
 bool shadow_mesh(IDirect3DDevice9*d,const Mat&world,const Mat&vp,IDirect3DVertexBuffer9*vb,IDirect3DIndexBuffer9*ib,unsigned vertices,unsigned indices){float c[32];rows(multiply(vp,world),c);rows(world,c+16);float ordinary[]={0,0,0,0};return SUCCEEDED(d->SetVertexShaderConstantF(18,ordinary,1))&&SUCCEEDED(d->SetVertexShaderConstantF(0,c,8))&&SUCCEEDED(d->SetStreamSource(0,vb,0,sizeof(Vertex)))&&SUCCEEDED(d->SetIndices(ib))&&SUCCEEDED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,vertices,0,indices/3));}
 bool end_shadow(IDirect3DDevice9*d){
  bool ok=true;
  auto restore=[&](HRESULT result){ok=SUCCEEDED(result)&&ok;};
  restore(d->SetDepthStencilSurface(nullptr));
  restore(d->SetRenderTarget(0,saved_target_));
  restore(d->SetDepthStencilSurface(saved_depth_));
  restore(saved_states_->Apply());
  restore(d->SetViewport(&saved_viewport_));
  saved_states_->Release();saved_states_=nullptr;
  saved_target_->Release();saved_target_=nullptr;
  if(saved_depth_)saved_depth_->Release();saved_depth_=nullptr;
  return ok;
 }
 bool bind(IDirect3DDevice9*d,D3DStateCache&state,const Mat&world,const Mat&view,const Mat&projection,Vec eye,Vec sun,bool body,bool glass,int normal,bool lightmap,bool detail,bool hasbase,int mode,bool hassecond,bool hasdirt,bool cracked,float time,bool scene_fog,const PlantWind*plant_wind=nullptr,float plant_far=150.f,float plant_fade_range=50.f,bool night_receiver=false,bool scene_material=true){
 if(!active||!prepare(d))return false;
 auto* pixel=scene_material?ps_:nullptr;
 if(!pixel&&night_receiver&&night_static_ps_)pixel=night_static_ps_;
 if(!pixel)return false;
 float c[88*4]{};rows(multiply(projection,multiply(view,world)),c);rows(world,c+16);Vec a{world[0],world[1],world[2]},b{world[4],world[5],world[6]},z{world[8],world[9],world[10]};float det=dot(a,cross(b,z));if(std::abs(det)>1e-9f){a=cross(b,z)/det;b=cross(z,Vec{world[0],world[1],world[2]})/det;z=cross(Vec{world[0],world[1],world[2]},Vec{world[4],world[5],world[6]})/det;}else{a={1,0,0};b={0,1,0};z={0,0,1};}for(int row=0;row<3;row++){c[32+row*4]=a[row];c[33+row*4]=b[row];c[34+row*4]=z[row];}
 c[48]=environment.night?1:0;c[49]=environment.rain?environment.rain_strength:0;c[50]=environment.sun_intensity;c[51]=float(count);c[52]=eye.x;c[53]=eye.y;c[54]=eye.z;c[56]=sun.x;c[57]=sun.y;c[58]=sun.z;c[60]=body?1:0;c[61]=glass?1:0;c[62]=float(normal);c[63]=lightmap?1:0;Vec fog=environment.night?Vec{.067f,.118f,.196f}:environment.rain?Vec{.39f,.47f,.49f}:Vec{.56f,.64f,.68f};c[64]=fog.x;c[65]=fog.y;c[66]=fog.z;c[67]=environment.fog_density;c[68]=detail?1:0;c[69]=hasbase?1:0;c[70]=shadows_ready?1:0;c[71]=float(shadow_size_);c[72]=float(mode);c[73]=time;c[74]=hassecond?1:0;c[75]=hasdirt?1:0;c[76]=std::all_of(sky.begin(),sky.begin()+5,[](auto*p){return p!=nullptr;})?1:0;c[77]=scene_fog?1:0;c[78]=cracked?1:0;c[79]=sky_fullsphere?1:0;
 for(int i=0;i<count;i++){rows(poses[i],c+128+i*16);std::copy(mounts[i].begin(),mounts[i].end(),c+256+i*4);std::copy(lo[i].begin(),lo[i].end(),c+288+i*4);std::copy(hi[i].begin(),hi[i].end(),c+320+i*4);}
 for(int slot=3;slot<5;slot++){state.SetTexture(slot,shadows_ready?shadow_[slot-3]:nullptr);state.SetSamplerState(slot,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);state.SetSamplerState(slot,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);state.SetSamplerState(slot,D3DSAMP_MINFILTER,D3DTEXF_POINT);state.SetSamplerState(slot,D3DSAMP_MAGFILTER,D3DTEXF_POINT);}
 for(int i=0;i<6;i++){unsigned slot=unsigned(i+6);state.SetTexture(slot,sky[i]);state.SetSamplerState(slot,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);state.SetSamplerState(slot,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);state.SetSamplerState(slot,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);state.SetSamplerState(slot,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);}
 // Keep camera expansion and wind in VS-only slots, preserving PS lamps.
 Vec right{view[0],view[4],view[8]},up{view[1],view[5],view[9]};
 if(mode==20){right=unit({right.x,0,right.z},{1,0,0});up={0,1,0};}
 float basis[]={right.x,right.y,right.z,0,up.x,up.y,up.z,0};
 bool vertex_ok=SUCCEEDED(state.SetVertexShaderConstantF(13,c+52,1))&&SUCCEEDED(state.SetVertexShaderConstantF(20,basis,2));
 if(mode==21){
  if(!plant_wind)return false;
  float wind[64]{};for(int i=0;i<16;i++)for(int j=0;j<3;j++)wind[i*4+j]=(*plant_wind)[i][j];
  float fade[]={std::max(0.f,plant_far),1.f/std::max(.001f,plant_fade_range),0,0};
  vertex_ok=SUCCEEDED(state.SetVertexShaderConstantF(96,wind,16))&&SUCCEEDED(state.SetVertexShaderConstantF(112,fade,1))&&vertex_ok;
 }
 bool ok=vertex_ok&&SUCCEEDED(state.SetVertexShader(vs_))&&SUCCEEDED(state.SetPixelShader(pixel))&&SUCCEEDED(state.SetVertexDeclaration(decl_))&&SUCCEEDED(state.SetVertexShaderConstantF(0,c,12))&&SUCCEEDED(state.SetVertexShaderConstantF(18,c+72,1))&&SUCCEEDED(state.SetScenePixelConstants(c+48,c+128,count!=0));if(ok)++draws;return ok;}
};
}
