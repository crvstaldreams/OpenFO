"""Guard host ABI for native slots; actual GL equations have framebuffer tests."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
h=(p/'src/dx9_scene_shaders.hpp').read_text()
r=(p/'src/render_dx9.cpp').read_text()
assert 'sampler2D dirtTexture:register(s5)' in h,'native Tex2 is not bound'
assert 'materialControl:register(c18)' in h
assert 'native_material_profile(m.shader)' in r
assert 'g->native_dirt' in r,'scene prune must retain Tex2'
assert 'm.native_second?m.native_second->p' in r
assert 'scene_shader.sky[i]=sky[i]?sky[i]->p:nullptr' in r
assert 'scene_shader.active=true' in r[r.index('void menu_scene'):r.index('void mesh(')]
assert 'materialControl.y' in h,'cloud UV offset needs time'
assert 'lerp(tex,tex2D(dirtTexture,i.uv),saturate(i.color.a))' in h
assert 'mode==2||mode==11||mode==14' in h,'native detail/lightmap/decal must use their own equations'

assert 'scene_bound=native_inputs_ready&&profile.scene' in r,'sunflare without mask must retain explicit fallback'
assert 'tex2D(lightTexture,(mode==2||mode==11||mode==14)?i.uv1:i.uv)' in h,'sunflare mask uses UV0, terrain/decal UV1'

print('D3D material binding ABI passed, including sunflare missing mask and UV0')
