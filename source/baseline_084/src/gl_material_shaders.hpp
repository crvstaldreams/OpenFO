#pragma once
#include "gl_compat_win.hpp"
#include "gl_state_cache.hpp"
#include "native_shader_source.hpp"
#include "native_material_profile.hpp"
namespace ofo {
// Native shader IDs and captured material inputs are retained. Old assembly is
// adapted by named semantic GLSL programs; original assembly is never sent to GL.
class GLMaterialShaders {
 struct Program{std::string file;int mode=0;NativeConstantCache constants;};
 std::map<uint32_t,std::shared_ptr<Program>>programs;std::map<std::string,std::shared_ptr<Program>>files;GLStateCache*state=nullptr;
public:
 Json report={{"programs",Json::object()},{"fallbacks",Json::object()},{"lighting","GLSL semantic adapters for native EMU/PRO material IDs; adapted hemisphere/sun constants"},{"limits","Native .sha material equations with contextual directional lookup. No exact DirectX assembly translation or shader-specific PRO deformation. Static original sky reflection cube; no scene captures. Unsupported IDs use texture/vertex-color fallback; original content bindings remain visible."}};uint64_t draws=0;
 void initialize(GLDevice*,GLStateCache*cache=nullptr){state=cache;report["assembler_available"]=false;report["language"]="GLSL 330 core";}
 void prepare(GLDevice*,const Resources&r,uint32_t id){if(programs.count(id))return;programs[id]=nullptr;try{auto profile=native_material_profile(id);auto file=native_effect(r,id);if(file.empty()&&profile.scene)file="openfo/semantic/"+std::to_string(id);if(file.empty())throw std::runtime_error(native_effect_limit(r,id));auto key=id==tree_billboard_shader?"openfo/tree_billboard":id==plant_wind_shader?"openfo/plant_wind":id==32?"openfo/window_cracked":file;if(files.count(key)){programs[id]=files[key];return;}auto p=std::make_shared<Program>();p->file=key;
 if(id==tree_billboard_shader)p->mode=20;else if(id==plant_wind_shader)p->mode=21;else p->mode=profile.mode;

 files[key]=p;programs[id]=p;report["programs"][key]=profile.role;
 }catch(const std::exception&e){report["fallbacks"][std::to_string(id)]=e.what();}}
 bool bind(GLDevice*d,uint32_t id,const Mat&world,const Mat&view,const Mat&projection,Vec sun,float time=0){auto it=programs.find(id);if(it==programs.end()||!it->second)return false;auto&p=*it->second;auto&values=p.constants.get(p.file,world,view,projection,sun,time,state&&state->enabled);if(state){state->SetVertexShaderConstantF(0,values.data(),96);state->erase({7,0,0});state->erase({5,0,0});state->erase({6,0,0});state->erase({1,0,D3DTSS_TEXCOORDINDEX});state->erase({1,1,D3DTSS_TEXCOORDINDEX});state->erase({1,0,D3DTSS_TEXTURETRANSFORMFLAGS});}else d->SetVertexShaderConstantF(0,values.data(),96);d->world=world;d->view=view;d->projection=projection;d->sun=sun;d->time=time;d->material_mode=p.mode;d->fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX2;
 // Native programs define their own Tex0/Tex1 coordinates. Lightmapped and
 // decalpaint materials consume UV1; car base and damage both consume UV0.
 d->stages[0][D3DTSS_TEXCOORDINDEX]=0;d->stages[1][D3DTSS_TEXCOORDINDEX]=(p.mode==2||p.mode==11||p.mode==14)?1:0;d->stages[0][D3DTSS_TEXTURETRANSFORMFLAGS]=0;++draws;return true;}
 Json constant_cache_stats()const{uint64_t builds=0,hits=0;for(auto&p:files){builds+=p.second->constants.builds;hits+=p.second->constants.hits;}return{{"builds",builds},{"hits",hits}};}
 void shutdown(){if(state)state->invalidate();programs.clear();files.clear();}
};
}
