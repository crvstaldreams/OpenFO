#!/usr/bin/env python3
"""Shared GL/DX post-process ABI: intensity survives reset and targets resize."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
for file in ('gl_screen_effects_win.hpp','screen_effects_win.hpp'):
 source=(root/'src'/file).read_text()
 assert 'void set_bloom_intensity(float value)' in source
 assert 'std::isfinite(value)?std::clamp(value,0.f,2.f):1.f' in source
 assert 'post_controls_[4]{1,0,0,0}' in source
 # User preference is initialized once and assigned only by its setter.
 assert len(re.findall(r'post_controls_\[0\]\s*=',source))==1
 assert 'post_controls_[1]=detail>=1?1.f:0.f' in source
assert 'SetPixelShaderConstantF(24,post_controls_,1)' in (root/'src/screen_effects_win.hpp').read_text()
assert 'i==0?D3DTEXF_LINEAR:D3DTEXF_POINT' in (root/'src/screen_effects_win.hpp').read_text()
assert 'gl.Uniform4f(composite_.postControls,' in (root/'src/gl_screen_effects_win.hpp').read_text()
for file in ('gl_screen_effects_shaders.hpp','screen_effects_shaders.hpp'):
 source=(root/'src'/file).read_text()
 assert 'if(postControls.x<=0.)' in source
 assert 'if(postControls.y<.5)' in source
 assert 'for(int i=0;i<12;++i)' in source
 assert source.index('c+=sceneBloom(uv);')<source.index('if(weather.x>.5)') if file.startswith('gl_') else source.index('+sceneBloom(uv);')<source.index('if(weather.x>.5)')
 assert 'sceneAA(uv,' in source
assert 'postControls:register(c24)' in (root/'src/screen_effects_shaders.hpp').read_text()
print('Post070 GL/DX ABI: persistent sanitized bloom intensity, detail FXAA, linear color/point depth and independent weather pass')
