#!/usr/bin/env python3
"""Draw actual scene resolve: localized bloom, FXAA edges, source alpha/depth."""
from pathlib import Path
base=Path(__file__).with_name('test_gl_effects_shaders.py').read_text()
checks=r'''
    size=32
    # Resize both output attachments and scene fixtures without changing shader source.
    active_tex(0x84C0);bind_tex(0x0DE1,output)
    image(0x0DE1,0,0x8058,size,size,0,0x1908,0x1401,None)
    bind_tex(0x0DE1,output_depth);image(0x0DE1,0,0x81A6,size,size,0,0x1902,0x1406,None)
    bind_tex(0x0DE1,depth);image(0x0DE1,0,0x81A6,size,size,0,0x1902,0x1406,(C.c_float*(size*size))(*([.4]*(size*size))))
    function(gl,'glViewport',None,integer,integer,integer,integer)(0,0,size,size)
    program=programs['composite'];sources(program)
    uniform1(location(program,b'aoEnabled'),0);uniform4(location(program,b'weather'),0,0,0,0)
    function(gl,'glEnable',None,uint)(0x0B71);function(gl,'glDepthFunc',None,uint)(0x0207)
    active_tex(0x84C0);bind_tex(0x0DE1,scene)
    for key in (0x2800,0x2801):parameter(0x0DE1,key,0x2601)
    expected_alpha=[91]*(size*size)
    def fixture(values):
        expected_alpha[:]=values[3::4]
        active_tex(0x84C0);bind_tex(0x0DE1,scene)
        image(0x0DE1,0,0x8058,size,size,0,0x1908,0x1401,(C.c_ubyte*(size*size*4))(*values))
    def post(bloom,aa):
        uniform4(location(program,b'postControls'),bloom,aa,1/size,1/size)
        draw(4,0,3);values=(C.c_ubyte*(size*size*4))();read(0,0,size,size,0x1908,0x1401,values)
        depths=(C.c_float*(size*size))();read(0,0,size,size,0x1902,0x1406,depths)
        assert all(abs(v-.4)<1e-6 for v in depths),'resolve lost scene depth'
        assert values[3::4]==expected_alpha,'resolve lost source alpha'
        return list(values)
    values=[]
    for y in range(size):
        for x in range(size):values.extend(([255]*3 if 14<=x<18 and 14<=y<18 else [12]*3)+[91])
    fixture(values);zero=post(0,0);normal=post(1,0);strong=post(2,0)
    assert zero==values,'zero bloom must disable the gather'
    halo=[i*4 for i in range(size*size) if values[i*4]==12 and normal[i*4]>14]
    assert len(halo)>20,(len(halo),'bright emitter has no visible halo')
    assert sum(strong[i]-zero[i] for i in halo)>sum(normal[i]-zero[i] for i in halo)*1.5,'bloom intensity does not increase halo'
    assert normal[:4]==strong[:4]==zero[:4],'bloom changed distant dark background'
    fixture([64,80,96,91]*(size*size));assert post(2,1)==[64,80,96,91]*(size*size),'flat neutral surface changed'
    values=[]
    for y in range(size):
        for x in range(size):values.extend(([230]*3 if x>8+y//2 else [20]*3)+[91 if x%2 else 173])
    fixture(values);aliased=post(0,0);antialiased=post(0,1)
    changed=[i for i in range(0,len(values),4) if 20<antialiased[i]<230]
    assert len(changed)>20,(len(changed),'FXAA did not smooth stair edges')
    assert antialiased[:4]==aliased[:4] and antialiased[-4:]==aliased[-4:],'FXAA blurred flat interior'
    print('Post070 actual pixels: zero/normal/strong localized bloom, neutral flat preservation, staircase FXAA, exact source alpha/depth',len(halo),len(changed))
'''
marker="    function(gl, 'glDeleteVertexArrays'"
assert marker in base
base=base.replace(marker,checks+'\n'+marker,1)
exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
