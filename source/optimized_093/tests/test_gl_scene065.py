#!/usr/bin/env python3
"""Real Mesa fragment assertions for receiver headlights and environment materials.
Reuses the existing context/mesh fixture; it also executes its regression suite.
"""
from pathlib import Path
base=Path(__file__).with_name('test_gl_effects_shaders.py').read_text()
checks=r'''
    # 065 scene tests: preserve clip coordinates while moving the receiver in world.
    real=function(gl,'glUniform1f',None,integer,C.c_float)
    matrix4=function(gl,'glUniformMatrix4fv',None,integer,integer,C.c_ubyte,C.POINTER(C.c_float))
    def scalar(name,value):real(location(material_program,name.encode()),value)
    def flag(name,value):uniform1(location(material_program,name.encode()),value)
    def matrix(name,values):matrix4(location(material_program,name.encode()),1,0,(C.c_float*16)(*values))
    def receiver(x=0,z=10):
        w=list(identity);w[12]=x;w[14]=z;matrix('world',w)
        v=list(identity);v[12]=-x;v[14]=-z;matrix('view',v)
    flag('sceneSurface',1);receiver();day=material(2)
    scalar('environmentSun',0);shade=material(2)
    assert sum(shade[:3])<sum(day[:3]),(shade,day)
    scalar('environmentSun',1)
    scalar('environmentNight',1);night=material(2)
    assert sum(night[:3])<sum(day[:3])*.3,(day,night)
    flag('headlightCount',1);matrix('headlightWorldToCar[0]',identity)
    uniform4(location(material_program,b'headlightLamps[0]'),.65,.7,2,0)
    bright=material(2)
    assert sum(bright[:3])>sum(night[:3])+25,(bright,night)
    back=material(2,normal=(0,0,1))
    assert sum(back[:3])<sum(bright[:3])*.5,(back,bright)
    receiver(12);outside=material(2)
    assert sum(outside[:3])<sum(bright[:3])*.5,(outside,bright)
    # Second car translated to the receiver; first cone stays outside.
    flag('headlightCount',2);ai=list(identity);ai[12]=-12
    matrix('headlightWorldToCar[1]',ai)
    uniform4(location(material_program,b'headlightLamps[1]'),.65,.7,2,0)
    ai_lit=material(2)
    assert sum(ai_lit[:3])>sum(outside[:3])+25,(ai_lit,outside)
    # Eighth car, rotated 90 degrees, illuminates a world -X-facing receiver.
    receiver(10,0);flag('headlightCount',8)
    for i in range(8):
        matrix('headlightWorldToCar[%d]'%i,identity)
        uniform4(location(material_program,('headlightLamps[%d]'%i).encode()),.65,.7,2,0)
    turned=list(identity);turned[0]=turned[10]=0;turned[2]=1;turned[8]=-1
    matrix('headlightWorldToCar[7]',turned)
    rotated=material(2,normal=(-1,0,0))
    assert sum(rotated[:3])>sum(night[:3])+25,(rotated,night)
    receiver(0,170);far=material(2)
    assert sum(far[:3])<sum(bright[:3])*.5,(far,bright)
    receiver();flag('headlightCount',1);matrix('headlightWorldToCar[0]',identity)
    active_tex(0x84C5)
    black_cookie=texture(values=(C.c_ubyte*64)(*([0,0,0,255]*16)))
    active_tex(0x84C5);bind_tex(0x0DE1,black_cookie);flag('headlightCookie',5);flag('hasHeadlightCookie',1)
    blocked=material(2)
    assert sum(blocked[:3])<sum(bright[:3])*.5,(blocked,bright)
    flag('hasHeadlightCookie',0);receiver(0,110);dry=material(2)
    scalar('environmentWet',1);wet=material(2)
    assert max(dry[:3]+wet[:3])<255 and sum(dry[:3])>sum(night[:3])+25,(wet,dry,night)
    assert wet[:3]!=dry[:3],(wet,dry) # At 25m the new beam saturates all RGB channels.
    print('scene065: real pixels pass day/night, receiving normals, cone, translated/rotated eighth AI, range, cookie and wet response',day,night,bright,ai_lit,wet)
    scalar('environmentNight',0);scalar('environmentWet',0);flag('sceneSurface',0);flag('headlightCount',0)
    matrix('world',identity);matrix('view',identity)
    active_tex(0x84C0);bind_tex(0x0DE1,base_material)
    active_tex(0x84C1);bind_tex(0x0DE1,light_material)
'''
marker='    assert all(abs(a-b)<=1 for a,b in zip(material(1)'
assert marker in base
base=base.replace(marker,checks+'\n'+marker,1)
exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
