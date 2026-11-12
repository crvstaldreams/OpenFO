#!/usr/bin/env python3
"""Night shader contracts and optional real glslang validation of shipped HLSL.
Usage: python tests/test_night_shader_source070.py [path/to/glslangValidator]
SPIR-V validation supplements (does not replace) runtime D3D9 SM3 compilation.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
gl=(root/'src/gl_compat_win.hpp').read_text()
dx=(root/'src/dx9_scene_shaders.hpp').read_text()
assert 'sky_native_night=false' in gl and 'one("skyNativeNight",sky_native_night)' in gl
assert 'if(skyNativeNight==0)' in gl
assert '.45*reflectance' in gl and '.45*reflectance' in dx
for source in (gl,dx):
 assert '.205,.265,.36' in source and '.095,.13,.21' not in source
for file in ('gl_screen_effects_shaders.hpp','screen_effects_shaders.hpp'):
 source=(root/'src'/file).read_text()
 assert 'float streak=' not in source and 'float distant=' not in source
 assert 'float bead=' in source and 'atmosphere.z' in source
 assert 'if(raw>=.99999)c*=' not in source
assert max(map(int,re.findall(r'register\(s(\d+)\)',dx)))<16
for array in ('poses','mounts','boundsLo','boundsHi'):
 assert not re.search(r'\b'+array+r'\[[^\]]*[A-Za-z_][^\]]*\]',dx)
if len(sys.argv)>1:
 with tempfile.TemporaryDirectory(prefix='night070-') as tmp:
  for file,entries in (('dx9_scene_shaders.hpp',(('vert','scene_vs'),('frag','scene_ps'),('frag','night_static_ps'),('frag','shadow_ps'))),('screen_effects_shaders.hpp',(('frag','composite_ps'),))):
   source=(root/'src'/file).read_text()
   path=Path(tmp)/'shipped.hlsl';path.write_text(re.search(r'R"HLSL\((.*?)\)HLSL"',source,re.S)[1])
   for stage,entry in entries:
    subprocess.run([sys.argv[1],'-D','--hlsl-dx9-compatible','-V','-S',stage,'-e',entry,str(path),'-o',str(Path(tmp)/(entry+'.spv'))],check=True)
print('Night070 contracts passed: native sky exposure, material reflectance, no fullscreen streaks, literal SM3 constants and sampler budget')
