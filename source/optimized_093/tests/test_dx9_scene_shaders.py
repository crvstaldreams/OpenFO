"""Source/host ABI checks; shader runtime compilation is tested separately on Windows."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
hlsl=(root/'src/dx9_scene_shaders.hpp').read_text()
host=(root/'src/dx9_scene_win.hpp').read_text()
renderer=(root/'src/render_dx9.cpp').read_text()
fx=(root/'src/screen_effects_shaders.hpp').read_text()
cache=(root/'src/d3d_state_cache.hpp').read_text()
assert 'register(c32)' in hlsl and 'rows(poses[i],c+128+i*16)' in host
assert 'register(c64)' in hlsl and 'mounts[i].end(),c+256+i*4' in host
assert 'register(c72)' in hlsl and 'lo[i].end(),c+288+i*4' in host
assert 'register(c80)' in hlsl and 'hi[i].end(),c+320+i*4' in host
assert 'SetScenePixelConstants(c+48,c+128,count!=0)' in host
assert 'upload(12,material,8)' in cache and 'upload(32,lights,56)' in cache
assert 'float3 albedo=lightmapped?second.rgb:tex.rgb' in hlsl
assert 'poses[32]' in hlsl and 'mounts[8]' in hlsl and 'car<8;car++' in hlsl and 'k<8;k++' in hlsl
assert 'std::array<Mat,8>' in host and 'count>=8' in host and 'float c[88*4]' in host
assert 'SetPixelShaderConstantF(64,mounts[0].data(),1)' in host
assert 'emitted_cars++>=8' in renderer
assert 'VFACE' not in hlsl and 'gl_FrontFacing' not in hlsl
assert 'headlightDepth(headlightBeam(q).z)' in hlsl
assert 'headlightDepthGradient(beam,beamDx,beamDy)' in hlsl and 'headlightDistance(corrected)-bias' in hlsl
assert 'source+=float3' in hlsl and 'blocked(source,i.world,car)' in hlsl
assert 'D3DFMT_R32F' in host and 'budget.draws||triangles0+indices/3>budget.triangles' in renderer
assert 'c[71]=float(shadow_size_)' in host and 'shadowControl.w' in hlsl
assert 'CreateTexture(size,size,1,D3DUSAGE_RENDERTARGET' in host
assert 'float2(.70,.38)' in hlsl and 'smoothstep(105,150' in hlsl
assert hlsl.index('float staticVisibility=(car==0')<hlsl.index('if(cone>.001&&beam.z')
assert 'GetRenderTargetData' not in host
assert '#include "gl_' not in renderer
for slot in (21,22,23):
 assert f'register(c{slot})' in fx
 assert f'SetPixelShaderConstantF({slot},' in (root/'src/screen_effects_win.hpp').read_text()
assert 'sunVisibility()' in fx and 'float3 bloom' in fx and 'atmosphere.z' in fx
assert not re.search(r'\b(vec[234]|textureLod|mix|fract)\s*\(',fx)
# PS3 constants cannot use relative addressing. Selection branches must name
# literal register indices even while the light/occluder loops stay dynamic.
for array in ('poses','mounts','boundsLo','boundsHi'):
 assert not re.search(r'\b'+array+r'\[[^\]]*[A-Za-z_][^\]]*\]',hlsl),array+' dynamic constant indexing violates ps_3_0'
assert 'tex.rgb*diffuse+.3*spec*mask' in hlsl
assert 'mask*fresnel):lit,1)' in hlsl
print('DX9 scene and screen effects shader ABI/source checks passed (SM3 literal indices, opaque body/gloss alpha)')

# Native paint shader has no normal-map input; gloss/rust RGB is not height.
assert "if(material.z>.5&&mode!=3)" in hlsl

assert "else if(mode==7)" in hlsl and "mode==1||mode==9||mode==20" in hlsl

# Packed materials use the detail UV and only one sample per fragment; roughness
# modulates highlight width, never geometric LIGHTS072 road reception.
assert 'packed_surface?2:0,m.lightmap!=nullptr,packed_surface?m.surface_uv1:m.detail' in renderer
assert 'c[62]=float(normal)' in host and 'bool packed=material.z>1.5' in hlsl
assert hlsl.count('tex2D(normalTexture,normalUV)')==1
assert 'float3 dir=normalize(localDirection(n,car));float receive=' in hlsl
assert 'detailN=surfaceNormal(n,worldDx,worldDy,uvDx,uvDy' in hlsl and 'float roughness=clamp(surfaceSample.a' in hlsl

# Night track rendering survives failure of the full SM3 scene shader, while
# day/native rendering stays on the ordinary material path.
assert 'night_static_ps' in hlsl and 'build_pixel("night_static_ps",night_static_ps_)' in host
assert 'night_receiver&&night_static_ps_' in host
assert 'night_receiver=scene_shader.active&&scene_environment.night&&!m.car&&!m.water' in renderer
assert 'D3DTSS_COLORARG1,D3DTA_TFACTOR' in renderer and '0xff2d3b50' in renderer
assert 'ddx(i.world)' in hlsl[:hlsl.index('[loop]for(int car=0')]
assert 'ddx(' not in hlsl[hlsl.index('float staticShadow('):hlsl.index('// Compact SM3 receiver')]
assert host.index('SetRenderTarget(0,saved_target_)')<host.index('saved_states_->Apply()')
assert 'd->GetViewport(&saved_viewport_)' in host and 'd->SetViewport(&saved_viewport_)' in host
assert 'scene_shader.shadows_ready=complete' in renderer
assert 'id==13||id==17||id==23||id==24||(id>=33&&id<=39)' in renderer
assert 'if(scene_shader.active&&scene_environment.night&&night_auxiliary_material(m.shader))return;' in renderer
assert 'night_auxiliary_material(packet.shader)' in renderer
