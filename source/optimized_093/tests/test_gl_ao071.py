#!/usr/bin/env python3
"""Actual reduced SSAO pixels from ray-cast floor/wall and narrow pole depth.
Exercises depth texture binding and the real fullscreen shader, then composite.
Mesa validation does not claim Windows driver coverage.
"""
from pathlib import Path
base=Path(__file__).with_name('test_gl_effects_shaders.py').read_text()
checks=r'''
    policy=(Path(__file__).resolve().parents[1]/'src/screen_effects_policy.hpp').read_text()
    radius,strength=map(float,re.search(r'ao_radius=([.0-9]+)f,ao_strength=([.0-9]+)f',policy).groups())
    size=128; reduced=64
    A=1.001;B=-.1001
    def fixture(pole=False,wall=True):
        depths=[]
        for y in range(size):
            for x in range(size):
                ray=((x+.5)/size*2-1,(y+.5)/size*2-1,1.)
                t=6. if wall else 50.
                if ray[1]<0:t=min(t,-1/ray[1])
                if pole:
                    lo=0.;hi=100.
                    for r,mn,mx in zip(ray,(-.16,-1.,3.8),(.16,1.,4.15)):
                        if abs(r)<1e-8:
                            if mn>0 or mx<0:hi=-1
                        else:
                            a,b=mn/r,mx/r;lo=max(lo,min(a,b));hi=min(hi,max(a,b))
                    if hi>=lo and lo>0:t=min(t,lo)
                depths.append(A+B/t)
        active_tex(0x84C1);bind_tex(0x0DE1,depth)
        image(0x0DE1,0,0x81A6,size,size,0,0x1902,0x1406,(C.c_float*len(depths))(*depths))
        return depths
    active_tex(0x84C0);bind_tex(0x0DE1,output)
    image(0x0DE1,0,0x8058,reduced,reduced,0,0x1908,0x1401,None)
    bind_tex(0x0DE1,output_depth);image(0x0DE1,0,0x81A6,reduced,reduced,0,0x1902,0x1406,None)
    function(gl,'glViewport',None,integer,integer,integer,integer)(0,0,reduced,reduced)
    function(gl,'glDisable',None,uint)(0x0B71)
    program=programs['ao'];sources(program)
    uniform4(location(program,b'projection'),1,1,A,B)
    def ao_pixels(pole=False,wall=True,enabled=1):
        fixture(pole,wall);use(program)
        uniform4(location(program,b'controls'),radius,strength,6,enabled)
        draw(4,0,3);values=(C.c_ubyte*(reduced*reduced*4))()
        read(0,0,reduced,reduced,0x1908,0x1401,values)
        return list(values)[::4]
    # A point sample reads depth at the center of the selected source texel.
    # Test an oblique plane through the actual reduced shader to catch bands.
    angled=[]
    for y in range(size):
        for x in range(size):
            rx=(x+.5)/size*2-1; ry=(y+.5)/size*2-1
            d=.4-ry-.1*rx
            angled.append(A+B/(2/d if d>0 else 100.1))
    active_tex(0x84C1);bind_tex(0x0DE1,depth)
    image(0x0DE1,0,0x81A6,size,size,0,0x1902,0x1406,(C.c_float*len(angled))(*angled))
    use(program);uniform4(location(program,b'controls'),radius,strength,6,1)
    draw(4,0,3);pixels=(C.c_ubyte*(reduced*reduced*4))()
    read(0,0,reduced,reduced,0x1908,0x1401,pixels)
    values=[pixels[4*(y*reduced+x)] for y in range(4,39) for x in range(6,58)]
    assert min(values)>=254,('planar road bands',min(values),max(values))
    flat=ao_pixels(wall=False);corner=ao_pixels();pole=ao_pixels(True,False)
    # Floor between 4.5m and the 6m wall, away from derivative discontinuities.
    corner_region=[y*reduced+x for y in (25,26,27,28) for x in range(24,40)]
    assert min(corner[i] for i in corner_region)<235,'wall/floor contact AO invisible'
    assert max(flat[i] for i in corner_region)==255,'flat floor globally darkened'
    # Pole contact at z~3.8, sample ground immediately beside its silhouette.
    pole_region=[y*reduced+x for y in (22,23) for x in range(28,36)]
    assert min(pole[i] for i in pole_region)<242,'narrow pole contact AO invisible'
    assert min(flat[y*reduced+x] for y in range(4,16) for x in range(8,56))>=254,'isolated planar ground self occludes'
    assert ao_pixels(True,True,0)==[255]*(reduced*reduced),'AO off still darkens'
    # Feed actual AO result to real full-resolution bilateral composite.
    corner=ao_pixels();fixture()
    active_tex(0x84C2);bind_tex(0x0DE1,ao)
    ao_rgba=[v for a in corner for v in (a,a,a,255)]
    image(0x0DE1,0,0x8058,reduced,reduced,0,0x1908,0x1401,(C.c_ubyte*len(ao_rgba))(*ao_rgba))
    active_tex(0x84C0);bind_tex(0x0DE1,scene)
    image(0x0DE1,0,0x8058,size,size,0,0x1908,0x1401,(C.c_ubyte*(size*size*4))(*([180,180,180,91]*(size*size))))
    bind_tex(0x0DE1,output);image(0x0DE1,0,0x8058,size,size,0,0x1908,0x1401,None)
    bind_tex(0x0DE1,output_depth);image(0x0DE1,0,0x81A6,size,size,0,0x1902,0x1406,None)
    function(gl,'glViewport',None,integer,integer,integer,integer)(0,0,size,size)
    program=programs['composite'];sources(program)
    uniform4(location(program,b'projection'),1,1,A,B)
    uniform4(location(program,b'weather'),0,0,0,0)
    uniform4(location(program,b'postControls'),0,0,1/size,1/size)
    uniform2(location(program,b'aoTexel'),1/reduced,1/reduced)
    uniform1(location(program,b'aoEnabled'),1)
    function(gl,'glEnable',None,uint)(0x0B71);function(gl,'glDepthFunc',None,uint)(0x0207)
    draw(4,0,3);resolved=(C.c_ubyte*(size*size*4))();read(0,0,size,size,0x1908,0x1401,resolved)
    resolved_depth=(C.c_float*(size*size))();read(0,0,size,size,0x1902,0x1406,resolved_depth)
    assert min(resolved[::4])<160,'AO lost in bilateral composite'
    assert resolved[4*(10*size+10)]==180,'AO composite globally darkens isolated floor'
    assert set(resolved[3::4])=={91},'AO composite changed source alpha'
    assert max(abs(a-b) for a,b in zip(resolved_depth,fixture()))<1e-6,'AO composite changed source depth'
    print('AO071 reduced GPU contacts: floor/wall min',min(corner[i] for i in corner_region),'pole min',min(pole[i] for i in pole_region),'isolated flat preserved')
'''
marker="    function(gl, 'glDeleteVertexArrays'"
base=base.replace(marker,checks+'\n'+marker,1)
exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
