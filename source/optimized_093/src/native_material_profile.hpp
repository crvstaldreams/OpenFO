#pragma once
#include <cstdint>
namespace ofo {
// Modes match the established GLSL material ABI. IDs are zero-based BGM/W32
// values, while shaderlib_pro.ini keys are one-based. Auxiliary shaders need
// dedicated targets/inputs and must never be advertised as ordinary materials.
struct NativeMaterialProfile {int mode;const char*role;bool scene=true;};
inline NativeMaterialProfile native_material_profile(uint32_t id){
 switch(id){
 case 0xfffffffeu:return{8,"CPU posed driver diffuse"};
 case 65536:return{20,"camera-facing native tree sprite"};
 case 65537:return{21,"native plant wind expansion"};
 case 0:case 18:case 19:case 21:return{1,"prelit static 2x"};
 case 1:return{2,"lightmap UV0 times detail UV1"};
 case 2:return{11,"lightmap/detail with masked specular"};
 case 3:case 4:case 7:case 8:case 9:return{8,"dynamic hemisphere and directional diffuse"};
 case 5:case 30:return{3,"paint, gloss, damage Tex2, Fresnel reflection"};
 case 6:return{4,"clean glass dirt/Fresnel"};
 case 10:return{16,"lamp lit base plus additive Tex2"};
 case 11:return{8,"shear dynamic; CPU damage geometry, neutral shader shear"};
 case 12:return{8,"scale dynamic; CPU damage geometry, neutral shader scale"};
 case 13:return{0,"shadow capture target",false};
 case 14:return{13,"unlit lamp"};
 case 15:case 22:case 27:return{10,"texture times vertex color"};
 case 16:case 31:return{12,"vertex color only"};
 case 17:return{0,"projected shadow sampling pass",false};
 case 20:return{9,"prelit tree branch custom color"};
 case 23:return{5,"sun flare visibility mask"};
 case 24:return{0,"screen intensity target",false};
 case 25:return{7,"animated water reflection/flow"};
 case 26:return{8,"skin diffuse after CPU bone palette"};
 case 28:case 29:return{6,"cloud UV scroll"};
 case 32:return{15,"fractured glass texture alpha"};
 case 33:return{0,"skinned shadow target",false};
 case 34:case 35:case 36:case 37:case 38:case 39:return{0,"sun occlusion mask target",false};
 default:return{0,"unknown shader ID",false};
 }
}
}
