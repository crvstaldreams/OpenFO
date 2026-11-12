#!/usr/bin/env python3
"""Actual Mesa pixels: native day preservation, moonlit receivers, lamps and sky."""
from pathlib import Path
base=Path(__file__).with_name('test_gl_effects_shaders.py').read_text()
checks=r'''
    real=function(gl,'glUniform1f',None,integer,C.c_float)
    matrix4=function(gl,'glUniformMatrix4fv',None,integer,integer,C.c_ubyte,C.POINTER(C.c_float))
    def scalar(name,value):real(location(material_program,name.encode()),value)
    def flag(name,value):uniform1(location(material_program,name.encode()),value)
    def matrix(name,value):matrix4(location(material_program,name.encode()),1,0,(C.c_float*16)(*value))
    native={mode:material(mode) for mode in (1,2,3,4,8,11,12,13,16)}
    flag('sceneSurface',1)
    assert all(material(mode)==value for mode,value in native.items()),'dry daytime native materials changed'
    w=list(identity);w[14]=10;matrix('world',w)
    v=list(identity);v[14]=-10;matrix('view',v)
    day=material(2);scalar('environmentNight',1);moon=material(2)
    assert .18<sum(moon[:3])/sum(day[:3])<.35,(day,moon,'moonlight must reveal native surface')
    assert moon[2]/day[2]>moon[0]/day[0],(day,moon,'cool ambient')
    authored_light=material(1);authored_shadow=material(1,vertex_rgb=(.25,.25,.25))
    assert sum(authored_shadow[:3])<sum(authored_light[:3])*.3,(authored_shadow,authored_light,'authored shadow contrast lost')
    flag('materialGlass',1);glass_dry=material(4,cracked=1)
    scalar('environmentWet',1);glass_wet=material(4,cracked=1)
    assert glass_wet[3]==glass_dry[3],'night wet layer changed fracture opacity'
    scalar('environmentWet',0);flag('materialGlass',0)
    scalar('environmentNight',0);emissive=material(13)
    scalar('environmentNight',1);assert material(13)==emissive,'native emissive darkened'
    flag('headlightCount',1);matrix('headlightWorldToCar[0]',identity)
    uniform4(location(material_program,b'headlightLamps[0]'),.65,.7,2,0)
    lit=material(2);assert sum(lit[:3])>sum(moon[:3])*3,(lit,moon,'lamp contrast')
    assert (lit[0]-moon[0])/128>(lit[2]-moon[2])/192,(lit,moon,'lamps must be warm')
    flag('headlightCount',0);flag('sceneSurface',0);flag('skySurface',1)
    # Generated cube faces carry their authored night exposure already.
    flag('skyNativeNight',1);generated=material(13)
    assert generated==emissive,(generated,emissive,'generated night sky darkened twice')
    flag('skyNativeNight',0);fallback=material(13)
    assert sum(fallback[:3])<sum(generated[:3])*.2,(fallback,generated)
    flag('skyNativeNight',1);scalar('environmentWet',1);rain_sky=material(13)
    assert .50<sum(rain_sky[:3])/sum(generated[:3])<.60,(rain_sky,generated)
    print('night070 GPU pixels: native day, cool readable moonlight, warm lamp contrast, emissive, generated/fallback/wet sky',day,moon,lit,generated,rain_sky)
    flag('skySurface',0);flag('skyNativeNight',0);scalar('environmentNight',0);scalar('environmentWet',0)
    matrix('world',identity);matrix('view',identity)
'''
marker='    assert all(abs(a-b)<=1 for a,b in zip(material(1)'
base=base.replace(marker,checks+'\n'+marker,1)
exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
