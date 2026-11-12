#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "dx9_scene_shaders.hpp"
#include "screen_effects_shaders.hpp"
int main(){
 using Compile=HRESULT(WINAPI*)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
 HMODULE module=nullptr;Compile compile=nullptr;
 for(auto name:{L"d3dcompiler_47.dll",L"d3dcompiler_43.dll"}){module=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)continue;auto p=GetProcAddress(module,"D3DCompile");std::memcpy(&compile,&p,sizeof compile);if(compile)break;FreeLibrary(module);module=nullptr;}
 if(!compile){std::puts("FAIL: System D3DCompile unavailable");return 2;}
 struct Entry{const char*source,*entry,*profile;};Entry entries[]={{ofo::dx9_scene_hlsl(),"scene_vs","vs_3_0"},{ofo::dx9_scene_hlsl(),"scene_ps","ps_3_0"},{ofo::dx9_scene_hlsl(),"night_static_ps","ps_3_0"},{ofo::dx9_scene_hlsl(),"shadow_ps","ps_3_0"},{ofo::screen_effects_hlsl(),"ao_ps","ps_3_0"},{ofo::screen_effects_hlsl(),"composite_ps","ps_3_0"},{ofo::screen_effects_hlsl(),"water_vs","vs_3_0"},{ofo::screen_effects_hlsl(),"water_ps","ps_3_0"}};
 int failures=0;for(const auto&e:entries){ID3DBlob*code=nullptr,*messages=nullptr;auto h=compile(e.source,std::strlen(e.source),"OpenFO_D3D9_validation",nullptr,nullptr,e.entry,e.profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&messages);std::printf("%s %s %s (HRESULT 0x%08lx, %zu bytes)\n",FAILED(h)?"FAIL":"PASS",e.entry,e.profile,static_cast<unsigned long>(h),code?code->GetBufferSize():0);if(messages){std::fwrite(messages->GetBufferPointer(),1,messages->GetBufferSize(),stdout);messages->Release();}if(code)code->Release();failures+=FAILED(h)?1:0;}FreeLibrary(module);return failures?1:0;
}
