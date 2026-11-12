#pragma once
#include "assets.hpp"
#include <regex>
#include <cstring>
namespace ofo {
constexpr uint32_t tree_billboard_shader=65536;
constexpr uint32_t plant_wind_shader=65537;
inline std::string tree_billboard_assembly(){return R"(vs.1.1
 dcl_position v0
 dcl_normal v1
 dcl_color v2
 dcl_texcoord v3
 mov r0, v0
 sub r0.xyz, r0, v1
 mad r0.xyz, v1.x, c10, r0
 mad r0.xyz, v1.y, c11, r0
 mov r0.w, c15.z
 m4x4 oPos, r0, c0
 mov oD0, v2
 mov oT0.xy, v3.xy
)";}

// c64..79 hold the sixteen plant_wind phases. c8 is the camera position;
// c32 is (far clip, inverse fade range, phase constant base, top epsilon).
inline std::string plant_wind_assembly(){return R"(vs.1.1
 dcl_position v0
 dcl_normal v1
 dcl_color v2
 dcl_texcoord0 v3
 dcl_texcoord1 v4
 mov r0, v0
 mov r1, v1
 mov r2, v4
 mad r0.x, r1.x, r2.x, r0.x
 mad r0.z, r1.y, r2.x, r0.z
 max r3.x, r2.x, -r2.x
 sge r3.y, r2.y, c32.w
 add r3.z, r1.z, c32.z
 mov a0.x, r3.z
 mov r4, c[a0.x]
 mul r5.x, r2.y, r4.x
 mul r5.y, r2.y, r4.y
 mul r5.z, r3.x, r4.z
 mul r5.z, r5.z, r3.y
 mad r0.x, r1.y, r5.x, r0.x
 mad r0.x, r1.x, r5.z, r0.x
 mad r0.z, -r1.x, r5.x, r0.z
 mad r0.z, r1.y, r5.z, r0.z
 add r0.y, r0.y, r5.y
 mov r0.w, c15.z
 m4x4 oPos, r0, c0
 sub r6.xyz, v0, c8
 dp3 r6.x, r6, r6
 rsq r6.x, r6.x
 rcp r6.x, r6.x
 sub r6.x, c32.x, r6.x
 mul r6.x, r6.x, c32.y
 max r6.x, r6.x, c15.x
 min r6.x, r6.x, c15.z
 mov r7, v2
 mov r7.w, r6.x
 mov oD0, r7
 mov oT0.xy, v3.xy
)";}

inline std::string shader_without_comments(std::string s){s=std::regex_replace(s,std::regex(R"(/\*[\s\S]*?\*/)"),"");return std::regex_replace(s,std::regex(R"(//[^\r\n]*|--[^\r\n]*)"),"");}
inline std::pair<std::string,std::string> native_assembly(const std::string&source){auto s=shader_without_comments(source);std::regex block(R"(asm\s*\{([^}]*)\})",std::regex::icase);std::pair<std::string,std::string>out;for(auto i=std::sregex_iterator(s.begin(),s.end(),block);i!=std::sregex_iterator();++i){auto text=(*i)[1].str();if(text.find("vs.1.1")!=text.npos){if(!out.first.empty())throw std::runtime_error("Multiple native vertex shaders require explicit pass selection");out.first=text;}else if(text.find("ps.1.1")!=text.npos){if(!out.second.empty())throw std::runtime_error("Multiple native pixel shaders require explicit pass selection");out.second=text;}else throw std::runtime_error("Unsupported native shader model");}if(out.first.empty())throw std::runtime_error("Native vertex assembly missing");return out;}
inline std::string native_effect(const Resources&r,uint32_t id){if(id==plant_wind_shader)return "shader/pro_default_static.sha";if(id==tree_billboard_shader)return "shader/pro_static2x.sha";auto pro=bed_file(r,"shader/shaderlib_pro.ini").at("Shaders");auto key=std::to_string(id+1);if(!pro.contains(key))return{};auto file=pro.at(key).at("EffectFile").get<std::string>();if(file=="pro_static2x.sha"||file=="pro_default_static.sha"||file=="pro_tree_branch.sha"||file=="pro_vertexcolor.sha"||file=="pro_lightmapped.sha"||file=="pro_lightmapped_spec.sha"||file=="pro_car_lights_unlit.sha"||file=="pro_clouds.sha"||file=="pro_sunflare.sha"||file=="pro_decalpaint.sha")return "shader/"+file;auto emu=bed_file(r,"shader/shaderlib_emu.ini").at("Shaders");if(emu.contains(key)){auto adapted=emu[key].at("EffectFile").get<std::string>();if(((file=="pro_default_dynamic.sha"||file=="pro_shear_dynamic.sha"||file=="pro_scale_dynamic.sha")&&adapted=="emu_default_dynamic.sha")||(file=="pro_water.sha"&&adapted=="emu_water.sha")||(file=="pro_car_body.sha"&&adapted=="emu_car_body.sha")||(file=="pro_car_window.sha"&&adapted=="emu_car_window.sha"))return "shader/"+adapted;}return{};}
// Fallbacks describe the missing binding, rather than implying that every
// original effect with valid assembly is usable by the mesh renderer.
inline std::string native_effect_limit(const Resources&r,uint32_t id){
 auto pro=bed_file(r,"shader/shaderlib_pro.ini").at("Shaders");auto key=std::to_string(id+1);
 if(!pro.contains(key))return "Unknown original material shader ID";
 auto file=pro.at(key).at("EffectFile").get<std::string>();
 if(file=="pro_car_lights.sha")return "Original car lights require base + additive glow passes and texture rebinding";
 if(file=="pro_skin.sha"||file=="pro_skin_shadow.sha")return "Original skin shader requires blend weights, bone indices and bone palette";
 if(file=="pro_intensitymap.sha")return "Original screen-space effect requires projected four-component position inputs and dedicated render states";
 if(file=="pro_shadow_dynamic.sha")return "Original shadow sampler requires generated shadow map, four texture bindings, pixel c7 and offset constants c32..c35";
 if(file=="pro_rendertarget_shadow.sha")return "Original shadow capture requires an offscreen shadow target and shadow-camera transform";
 if(file.find("sunmap")!=file.npos)return "Original sunmap mask requires a dedicated occlusion render-target pass and mask constants";
 if(file=="pro_shear_dynamic.sha"||file=="pro_scale_dynamic.sha")return "Original deformation shader requires per-material deformation constants and diffuse lookup cubemap";
 return "Original effect requires material bindings not implemented: "+file;
}
inline bool native_uses_time(const std::string&file){return file=="shader/emu_water.sha"||file=="shader/pro_clouds.sha";}
// Constants are rows for the original m4x4/m3x3 instructions.
inline std::array<float,96*4> native_constants(const std::string&file,const Mat&world,const Mat&view,const Mat&projection,Vec sun,float time=0){
 Mat wvp=multiply(projection,multiply(view,world));std::array<float,96*4> constants{};
 for(int row=0;row<4;row++)for(int c=0;c<4;c++){constants[row*4+c]=wvp[c*4+row];constants[16+row*4+c]=world[c*4+row];}
 constants[60]=0;constants[61]=.5f;constants[62]=1;constants[63]=2;
 for(int c=0;c<3;c++){constants[18*4+c]=.12f;constants[19*4+c]=.55f;constants[20*4+c]=-sun[c];constants[21*4+c]=.55f;}
 for(int c=0;c<4;c++)constants[32*4+c]=1;
 if(file=="shader/pro_lightmapped_spec.sha"){
  // View is the orthonormal look_at matrix used by the renderer. The original
  // specular shader keeps v0/v1 in object space rather than applying c4.
  Vec t{view[12],view[13],view[14]};
  Vec eye{-(view[0]*t.x+view[1]*t.y+view[2]*t.z),-(view[4]*t.x+view[5]*t.y+view[6]*t.z),-(view[8]*t.x+view[9]*t.y+view[10]*t.z)};
  Vec a{world[0],world[1],world[2]},b{world[4],world[5],world[6]},c{world[8],world[9],world[10]};
  float det=dot(a,cross(b,c));if(std::abs(det)<1e-10f)throw std::runtime_error("Singular native shader world transform");
  auto local=[&](Vec p){return Vec{dot(cross(b,c),p)/det,dot(cross(c,a),p)/det,dot(cross(a,b),p)/det};};
  eye=local(eye-Vec{world[12],world[13],world[14]});
  auto forward=unit(local({view[2],view[6],view[10]}));auto light=unit(local(sun*-1));
  for(int i=0;i<3;i++){constants[8*4+i]=eye[i];constants[9*4+i]=forward[i];constants[20*4+i]=light[i];constants[22*4+i]=.3f;}
  constants[8*4+3]=1;
  constants[32*4]=5;constants[32*4+1]=constants[32*4+2]=constants[32*4+3]=0;
 }
 if(file=="shader/emu_car_window.sha"){
  // The original EMU T0 uses world-space eye, ambient c17 and Fresnel
  // c32=(R0, 1-R0, 1, 0). Its second technique changes alpha only.
  Vec t{view[12],view[13],view[14]};
  constants[8*4]=-(view[0]*t.x+view[1]*t.y+view[2]*t.z);
  constants[8*4+1]=-(view[4]*t.x+view[5]*t.y+view[6]*t.z);
  constants[8*4+2]=-(view[8]*t.x+view[9]*t.y+view[10]*t.z);
  constants[8*4+3]=1;
  for(int i=0;i<3;i++)constants[17*4+i]=.55f;
  constants[32*4]=.375f;constants[32*4+1]=.625f;constants[32*4+2]=1;constants[32*4+3]=0;
 }
 if(file=="shader/pro_clouds.sha"){
  // Original c32 is an externally supplied UV offset; no captured engine
  // scroll rate is available. Keep the adapted rate explicit and bounded.
  constants[32*4]=std::fmod(time*.002f,1.f);constants[32*4+1]=std::fmod(time*.0005f,1.f);
  constants[32*4+2]=constants[32*4+3]=0;
 }
 if(file=="openfo/tree_billboard"){
  for(int c=0;c<3;c++){constants[10*4+c]=view[c*4];constants[11*4+c]=view[c*4+1];}
 }
 if(file=="shader/emu_water.sha"){
  for(int i=0;i<4;i++){constants[14*4+i]=time;constants[94*4+i]=2;}
  constants[64*4]=.06f;constants[64*4+1]=.06f;constants[64*4+2]=.01f;constants[64*4+3]=-.008f;
  constants[95*4]=0;constants[95*4+1]=.4f;constants[95*4+2]=.4f;constants[95*4+3]=.5f;
 }
 return constants;
}

// One entry per assembled program: static batches commonly share a world
// transform. Return the stored array directly so a hit avoids both matrix
// products, all register initialization, and the 384-float draw-local copy.
// Bitwise keys preserve signed-zero/NaN distinctions in the original builder.
class NativeConstantCache {
 std::string file_;
 Mat world_{},view_{},projection_{};
 std::array<float,3> sun_{};
 float time_=0;
 bool valid_=false;
 std::array<float,96*4> constants_{};
 template<class T> static bool same(const T&a,const T&b){return std::memcmp(&a,&b,sizeof(T))==0;}
 public:
 uint64_t builds=0,hits=0;
 const std::array<float,96*4>& get(const std::string&file,const Mat&world,const Mat&view,const Mat&projection,Vec sun,float time=0,bool enabled=true){
  const std::array<float,3> light{sun.x,sun.y,sun.z};
  // Invalidate animated programs only; static draws remain cacheable across frames.
  const float shader_time=native_uses_time(file)?time:0.f;
  if(enabled&&valid_&&file_==file&&same(world_,world)&&same(view_,view)&&same(projection_,projection)&&same(sun_,light)&&same(time_,shader_time)){++hits;return constants_;}
  auto next=native_constants(file,world,view,projection,sun,shader_time);
  // Commit only after the builder succeeds (singular specular worlds throw).
  file_=file;constants_=next;world_=world;view_=view;projection_=projection;sun_=light;time_=shader_time;valid_=enabled;++builds;
  return constants_;
 }
};

}
