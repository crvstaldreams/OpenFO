#pragma once
#include "state_cache.hpp"
#include "gl_compat_win.hpp"
namespace ofo {
// One cache per device, shared by every state writer. Call invalidate() before
// Reset, Draw*UP (which clears stream/index bindings), external state-block
// application, or releasing/replacing any tracked resource. attach invalidates.
// begin_frame resets counters only; cached state persists between frames.
class GLStateCache : public StateCache {
 GLDevice* device=nullptr;
 template<class T,class Submit>HRESULT write(Key key,const T& value,Submit submit){
  HRESULT result=D3D_OK;set(key,&value,sizeof(value),[&]{result=submit();return SUCCEEDED(result);});return result;
 }
public:
 void attach(GLDevice* value){device=value;invalidate();}
 HRESULT SetRenderState(D3DRENDERSTATETYPE type,DWORD value){return write({0,DWORD(type),0},value,[&]{return device->SetRenderState(type,value);});}
 HRESULT GetRenderState(D3DRENDERSTATETYPE type,DWORD* value){
  if(!value){++calls;return device->GetRenderState(type,value);}if(read({0,DWORD(type),0},value,sizeof(*value))){++skipped;return D3D_OK;}
  ++calls;auto result=device->GetRenderState(type,value);if(SUCCEEDED(result))remember({0,DWORD(type),0},value,sizeof(*value));return result;
 }
 HRESULT SetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD value){return write({1,stage,DWORD(type)},value,[&]{return device->SetTextureStageState(stage,type,value);});}
 HRESULT SetSamplerState(DWORD sampler,D3DSAMPLERSTATETYPE type,DWORD value){return write({2,sampler,DWORD(type)},value,[&]{return device->SetSamplerState(sampler,type,value);});}
 HRESULT SetTexture(DWORD stage,GLTexture* value){return write({3,stage,0},value,[&]{return device->SetTexture(stage,value);});}
 HRESULT SetTransform(D3DTRANSFORMSTATETYPE type,const D3DMATRIX* value){if(!value){++calls;return device->SetTransform(type,value);}return write({4,DWORD(type),0},*value,[&]{return device->SetTransform(type,value);});}
 HRESULT SetFVF(DWORD value){auto result=write({5,0,0},value,[&]{return device->SetFVF(value);});if(SUCCEEDED(result))erase({6,0,0});return result;}
 HRESULT SetVertexDeclaration(GLDeclaration* value){auto result=write({6,0,0},value,[&]{return device->SetVertexDeclaration(value);});if(SUCCEEDED(result))erase({5,0,0});return result;}
 HRESULT SetVertexShader(GLShader* value){return write({7,0,0},value,[&]{return device->SetVertexShader(value);});}
 HRESULT SetPixelShader(GLShader* value){return write({8,0,0},value,[&]{return device->SetPixelShader(value);});}
 HRESULT SetStreamSource(UINT stream,GLVertexBuffer* buffer,UINT offset,UINT stride){
  // Avoid struct padding in the cache's byte comparison.
  std::array<std::uintptr_t,3> value={reinterpret_cast<std::uintptr_t>(buffer),offset,stride};
  return write({9,stream,0},value,[&]{return device->SetStreamSource(stream,buffer,offset,stride);});
 }
 HRESULT SetIndices(GLIndexBuffer* value){return write({10,0,0},value,[&]{return device->SetIndices(value);});}
 HRESULT SetVertexShaderConstantF(UINT first,const float* data,UINT count){
  if(!data||first>256||count>256-first){++calls;return device->SetVertexShaderConstantF(first,data,count);}
  HRESULT result=D3D_OK;constants_trimmed(first,data,count,[&](UINT start,const float*values,UINT length){result=device->SetVertexShaderConstantF(start,values,length);return SUCCEEDED(result);});return result;
 }
};
}
