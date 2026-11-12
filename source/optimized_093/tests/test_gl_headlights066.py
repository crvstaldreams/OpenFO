#!/usr/bin/env python3
"""Real Mesa fragment assertions for receiver headlights and environment materials.
Reuses the existing context/mesh fixture; it also executes its regression suite.
"""
from pathlib import Path
base=Path(__file__).with_name('test_gl_effects_shaders.py').read_text()
checks=r'''
    uniform2=function(gl,'glUniform2f',None,integer,C.c_float,C.c_float)
    def health(i,a,b):uniform2(location(material_program,('headlightHealth[%d]'%i).encode()),a,b)
    for i in range(8):health(i,1,1)

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
    # Native LH camera projection can rasterize an authored outward normal as
    # GL back-facing. Lighting must depend on lamp/surface, never that flag.
    front_face=function(gl,'glFrontFace',None,uint)
    front_face(0x0900) # GL_CW: same physical receiver now has opposite raster side
    clockwise=material(2)
    clockwise_back=material(2,normal=(0,0,1))
    front_face(0x0901)
    assert clockwise==bright,(clockwise,bright,'camera winding inverted physical receiver normal')
    assert sum(clockwise_back[:3])<sum(bright[:3])*.5,(clockwise_back,bright,'opposite side must stay dark')
    health(0,0,0);broken=material(2)
    assert broken==bright,(broken,bright,'damage must not switch lamps off')
    health(0,0,1);single=material(2)
    assert single==bright,(single,broken,bright)
    health(0,1,1)
    # Level road at a .7m lamp height: .11 downward aim centres around 6.4m.
    # Measure light contribution separately from night ambience at each range.
    def ground(distance):
        receiver(0,distance+1.5)
        w=list(identity);w[13]=-.25;w[14]=distance+1.5;matrix('world',w)
        v=list(identity);v[13]=.25;v[14]=-distance-1.5;matrix('view',v)
        flag('headlightCount',0);unlit=material(2,normal=(0,1,0))
        flag('headlightCount',1);lit=material(2,normal=(0,1,0))
        return sum(lit[:3])-sum(unlit[:3])
    road2,road5,road15,road60,road110=ground(2),ground(5),ground(15),ground(60),ground(110)
    assert road5>road15*2 and road5>35,(road2,road5,road15,'beam must light near road')
    assert road60>40 and road110>20,(road60,road110,'long range must remain visible on grazing asphalt')
    print('headlights072 road RGB contribution at 2/5/15/60/110m:',road2,road5,road15,road60,road110)
    # A steep normal-map slope cannot remove geometric road reception. With
    # zero specular strength the packed track shader must preserve the gain.
    flag('texNormal',3);flag('hasNormal',2);active_tex(0x84C3)
    texture(values=(C.c_ubyte*64)(*([128,250,0,222]*16)))
    packed_road=ground(60)
    assert abs(packed_road-road60)<=3,(packed_road,road60,'micro-normal changed the lamp footprint')
    flag('hasNormal',0)

    receiver()
    # Every ordinary receiving material path, including CPU posed drivers (8)
    # and water (7), receives the same physical projected lamp contribution.
    for mode in (1,2,3,4,7,8,9,10,11,12,14,15,16):
        flag('headlightCount',0);without=material(mode)
        flag('headlightCount',1);with_lamp=material(mode)
        assert sum(with_lamp[:3])>sum(without[:3])+20,(mode,without,with_lamp)
    # Tree billboard corner offsets are not normals. Reconstructing the leaf
    # plane gives equal lighting from either leaf side, with the same geometry.
    for k,val in enumerate((1,0,0,0)):constants[10*4+k]=val
    for k,val in enumerate((0,1,0,0)):constants[11*4+k]=val
    function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
    flag('headlightCount',0);leaf_unlit=material(20,normal=(0,0,0))
    flag('headlightCount',1);leaf_front=material(20,normal=(0,0,0))
    constants[10*4]=-1
    function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
    leaf_back=material(20,normal=(0,0,0))
    flag('headlightCount',0);leaf_back_unlit=material(20,normal=(0,0,0));flag('headlightCount',1)
    front_gain=leaf_front[1]-leaf_unlit[1];back_gain=leaf_back[1]-leaf_back_unlit[1]
    assert abs(front_gain-back_gain)<=12 and front_gain>20,(leaf_unlit,leaf_front,leaf_back_unlit,leaf_back)
    for row in (10,11):
        for k in range(4):constants[row*4+k]=0
    function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
    # Expanded CPU foliage uses ordinary texture*vertex color, but both sides
    # of its true leaf plane must still receive the physical projected lamps.
    for normal in ((0,0,1),(0,0,-1)):
        flag('headlightCount',0);cpu_leaf_unlit=material(22,normal=normal)
        flag('headlightCount',1);cpu_leaf_lit=material(22,normal=normal)
        assert sum(cpu_leaf_lit[:3])>sum(cpu_leaf_unlit[:3])+20,(normal,cpu_leaf_unlit,cpu_leaf_lit)
    # Depth receiver comparison uses the actual projected z, with independent lamps.
    projected=list(identity);projected[10]=0;projected[14]=.5
    matrix('headlightShadowMatrix[0]',projected);matrix('headlightShadowMatrix[1]',projected)
    # Render an intervening opaque triangle through the shipped depth shaders,
    # then use its GPU depth texture in the actual receiving material shader.
    shadow_source=(Path(__file__).resolve().parents[1]/'src/gl_headlight_shadow_win.hpp').read_text()
    shadow_sources=dict(re.findall(r'inline const char\*\s*(gl_headlight_shadow_\w+)\(\)\{return R"GLSL\((.*?)\)GLSL";',shadow_source,re.S))
    assert len(shadow_sources)==2
    shadow_program=create_program()
    for name,contents in shadow_sources.items():
        shader=create_shader(0x8B31 if name.endswith('_vs') else 0x8B30)
        value=C.c_char_p(contents.encode());source(shader,1,C.byref(value),None);compile_shader(shader)
        ok=integer();get_shader(shader,0x8B81,C.byref(ok));assert ok.value,name
        attach(shadow_program,shader)
    link(shadow_program);ok=integer();get_program(shadow_program,0x8B82,C.byref(ok));assert ok.value
    active_tex(0x84C6);shadow_depth=texture(True,(C.c_float*16)(*([1.]*16)))
    shadow_fbo=uint();gen_fbos(1,C.byref(shadow_fbo));bind_fbo(0x8D40,shadow_fbo.value)
    attach_tex(0x8D40,0x8CE0,0x0DE1,output,0);attach_tex(0x8D40,0x8D00,0x0DE1,shadow_depth,0)
    assert function(gl,'glCheckFramebufferStatus',uint,uint)(0x8D40)==0x8CD5
    use(shadow_program);blocker=list(identity);blocker[14]=-.3
    matrix4(location(shadow_program,b'lightWorld'),1,0,(C.c_float*16)(*blocker))
    function(gl,'glEnable',None,uint)(0x0B71);function(gl,'glDepthFunc',None,uint)(0x0201)
    function(gl,'glDepthMask',None,C.c_ubyte)(1);function(gl,'glClearDepth',None,C.c_double)(1.)
    function(gl,'glClear',None,uint)(0x00000100);draw(0x0004,0,3)
    sample=(C.c_float*1)();read(2,2,1,1,0x1902,0x1406,sample)
    assert abs(sample[0]-.2)<.001,sample[0]
    bind_fbo(0x8D40,fbo.value);function(gl,'glDisable',None,uint)(0x0B71);use(material_program)
    active_tex(0x84C6);bind_tex(0x0DE1,shadow_depth);flag('headlightShadow',6);flag('hasHeadlightShadow',1)
    blocked_wall=material(2)
    assert sum(blocked_wall[:3])<sum(bright[:3])*.5,(blocked_wall,bright,'depth blocker must cast shadow')
    bind_fbo(0x8D40,shadow_fbo.value);function(gl,'glClear',None,uint)(0x00000100)
    bind_fbo(0x8D40,fbo.value)
    unblocked_wall=material(2)
    assert sum(unblocked_wall[:3])>sum(blocked_wall[:3])+25,(unblocked_wall,blocked_wall)
    flag('hasHeadlightShadow',0)
    # Real projected grazing road: the receiver itself is the depth
    # caster, then a camera with different pixel phases samples the map. A
    # fixed centre-depth comparison produces stripes over this flat plane.
    material_source=(Path(__file__).resolve().parents[1]/'src/gl_compat_win.hpp').read_text()
    visibility_source=material_source[material_source.index('vec2 headlightDepthGradient('):material_source.index('uniform ivec4 colorStage0')]
    visibility_program=create_program()
    visibility_vs='#version 330 core\nlayout(location=0)in vec3 position;uniform mat4 lightWorld;uniform vec2 phase;\nout vec3 worldPosition;\nvoid main(){worldPosition=position;vec4 p=lightWorld*vec4(position,1);p.z=2*p.z-p.w;p.xy+=phase*p.w;gl_Position=p;}\n'
    visibility_fs='#version 330 core\nin vec3 worldPosition;out vec4 frag;uniform int hasHeadlightShadow;\nuniform mat4 headlightShadowMatrix[2];uniform sampler2D headlightShadow;\n'+visibility_source+'void main(){float v=headlightStaticVisibility(0,headlightReceiverBias(1.,normalize(-worldPosition).y));frag=vec4(v,v,v,1);}'
    for kind,contents in ((0x8B31,visibility_vs),(0x8B30,visibility_fs)):
        shader=create_shader(kind);value=C.c_char_p(contents.encode());source(shader,1,C.byref(value),None);compile_shader(shader)
        ok=integer();get_shader(shader,0x8B81,C.byref(ok));log=C.create_string_buffer(16384);shader_log(shader,len(log),None,log)
        assert ok.value,log.value.decode();attach(visibility_program,shader)
    link(visibility_program);ok=integer();get_program(visibility_program,0x8B82,C.byref(ok));assert ok.value
    def sized_texture(width,height,depth=False):
        tex=texture(depth);image(0x0DE1,0,0x81A6 if depth else 0x8058,width,height,0,0x1902 if depth else 0x1908,0x1406 if depth else 0x1401,None);return tex
    active_tex(0x84C6);road_depth=sized_texture(1024,512,True);road_color=sized_texture(1024,512)
    road_fbo=uint();gen_fbos(1,C.byref(road_fbo));bind_fbo(0x8D40,road_fbo.value)
    attach_tex(0x8D40,0x8CE0,0x0DE1,road_color,0);attach_tex(0x8D40,0x8D00,0x0DE1,road_depth,0)
    assert function(gl,'glCheckFramebufferStatus',uint,uint)(0x8D40)==0x8CD5
    road_output=sized_texture(64,64);road_output_fbo=uint();gen_fbos(1,C.byref(road_output_fbo));bind_fbo(0x8D40,road_output_fbo.value)
    attach_tex(0x8D40,0x8CE0,0x0DE1,road_output,0)
    # Project with the same lamp pitch and cone as both hosts.
    pitch=.11;co=1/(1+pitch*pitch)**.5;si=pitch*co
    projection=[1/.70,0,0,0, 0,1/.38,0,0, 0,0,150/(150-.12),1, 0,0,-.12*150/(150-.12),0]
    rotation=[1,0,0,0, 0,co,-si,0, 0,si,co,0, 0,0,0,1]
    light=[sum(projection[k*4+r]*rotation[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]
    def geometry(points):
        # Retain the 56-byte fixture stride and enabled position attribute.
        data=(C.c_float*(14*len(points)))(*sum(([x,y,z]+[0.]*11 for x,y,z in points),[]))
        bind_buffer(0x8892,vbo.value);function(gl,'glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8892,C.sizeof(data),data,0x88E0)
    road=[(-35,-.7,1),(35,-.7,1),(35,-.7,70),(-35,-.7,1),(35,-.7,70),(-35,-.7,70)]
    def cast_road(block=False, height_delta=0):
        bind_fbo(0x8D40,road_fbo.value);function(gl,'glViewport',None,integer,integer,integer,integer)(0,0,shadow_size,shadow_size)
        use(shadow_program);matrix4(location(shadow_program,b'lightWorld'),1,0,(C.c_float*16)(*light))
        function(gl,'glEnable',None,uint)(0x0B71);function(gl,'glClear',None,uint)(0x00000100)
        geometry([(x,y+height_delta,z) for x,y,z in road]);draw(4,0,6)
        if block:
            geometry([(-2,-.7,7),(2,-.7,7),(2,1,7),(-2,-.7,7),(2,1,7),(-2,1,7)]);draw(4,0,6)
        function(gl,'glDisable',None,uint)(0x0B71)
    def visible_road(phase=0):
        bind_fbo(0x8D40,road_output_fbo.value);function(gl,'glViewport',None,integer,integer,integer,integer)(0,0,64,64)
        use(visibility_program);matrix4(location(visibility_program,b'lightWorld'),1,0,(C.c_float*16)(*light))
        matrix4(location(visibility_program,b'headlightShadowMatrix[0]'),1,0,(C.c_float*16)(*light))
        uniform1(location(visibility_program,b'hasHeadlightShadow'),1);uniform1(location(visibility_program,b'headlightShadow'),6)
        uniform2(location(visibility_program,b'phase'),phase,phase)
        active_tex(0x84C6);bind_tex(0x0DE1,road_depth)
        function(gl,'glClearColor',None,C.c_float,C.c_float,C.c_float,C.c_float)(0,0,0,0);function(gl,'glClear',None,uint)(0x00004000)
        geometry(road);draw(4,0,6);pixels=(C.c_ubyte*(64*64*4))();read(0,0,64,64,0x1908,0x1401,pixels)
        # Ignore raster boundaries; inspect the central visible road to 50m.
        return [pixels[(y*64+x)*4] for y in range(4,45) for x in range(8,56) if pixels[(y*64+x)*4+3]==255]
    for shadow_size in (512,256):
        if shadow_size==256:
            active_tex(0x84C6)
            road_depth=sized_texture(512,256,True);road_color=sized_texture(512,256)
            bind_fbo(0x8D40,road_fbo.value)
            attach_tex(0x8D40,0x8CE0,0x0DE1,road_color,0);attach_tex(0x8D40,0x8D00,0x0DE1,road_depth,0)
            assert function(gl,'glCheckFramebufferStatus',uint,uint)(0x8D40)==0x8CD5
        cast_road()
        for phase in (0,.007,-.011):
            vis=visible_road(phase);assert len(vis)>1000,(len(vis),phase,shadow_size)
            assert min(vis)>=250,(min(vis),sum(v<250 for v in vis),phase,shadow_size,'grazing road self-shadow stripes')
        cast_road(height_delta=.008)
        layered=visible_road(.007)
        assert min(layered)>=250,(min(layered),sum(v<250 for v in layered),shadow_size,'8mm ground layer falsely occludes the road')
        cast_road(True);occluded=visible_road()
        assert sum(v<64 for v in occluded)>100,(min(occluded),shadow_size,'real blocker must retain shadow')
    print('headlights076: grazing road, 8mm road layers and a real blocker passed with 512px and 256px shadows')
    bind_fbo(0x8D40,fbo.value);function(gl,'glViewport',None,integer,integer,integer,integer)(0,0,4,4);use(material_program)
    active_tex(0x84C6);bind_tex(0x0DE1,shadow_depth)
    # An intervening vehicle box shadows the receiver; off-axis boxes do not.
    flag('headlightOccluderCount',2)
    matrix('headlightOccluderPose[1]',identity)
    uniform4(location(material_program,b'headlightOccluderLo[1]'),-2,-2,4,0)
    uniform4(location(material_program,b'headlightOccluderHi[1]'),2,2,6,0)
    matrix('headlightOccluderPose[0]',identity)
    uniform4(location(material_program,b'headlightOccluderLo[0]'),-2,-2,0,0)
    uniform4(location(material_program,b'headlightOccluderHi[0]'),2,2,5,0)
    blocked_car=material(2)
    assert sum(blocked_car[:3])<sum(bright[:3])*.5,(blocked_car,bright,'car box must cast shadow')
    shifted=list(identity);shifted[12]=-12;matrix('headlightOccluderPose[1]',shifted)
    off_axis=material(2)
    assert off_axis==bright,(off_axis,bright,'lamp must not be blocked by its own car')
    flag('headlightOccluderCount',0)
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
    health(1,0,0);assert material(2)==ai_lit,'damaged AI lamps must stay on'
    # Eighth car, rotated 90 degrees, illuminates a world -X-facing receiver.
    receiver(10,0);flag('headlightCount',8)
    for i in range(8):
        matrix('headlightWorldToCar[%d]'%i,identity)
        uniform4(location(material_program,('headlightLamps[%d]'%i).encode()),.65,.7,2,0)
    turned=list(identity);turned[0]=turned[10]=0;turned[2]=1;turned[8]=-1
    matrix('headlightWorldToCar[7]',turned)
    rotated=material(2,normal=(-1,0,0))
    assert sum(rotated[:3])>sum(night[:3])+25,(rotated,night)
    health(7,0,0);assert material(2,normal=(-1,0,0))==rotated,'damaged eighth AI lamps must stay on'
    receiver(0,170);far=material(2)
    assert sum(far[:3])<sum(bright[:3])*.5,(far,bright)
    receiver();flag('headlightCount',1);matrix('headlightWorldToCar[0]',identity)
    active_tex(0x84C5)
    black_cookie=texture(values=(C.c_ubyte*64)(*([0,0,0,255]*16)))
    active_tex(0x84C5);bind_tex(0x0DE1,black_cookie);flag('headlightCookie',5);flag('hasHeadlightCookie',1)
    blocked=material(2)
    assert sum(blocked[:3])<sum(bright[:3])*.5,(blocked,bright)
    flag('hasHeadlightCookie',0);flag('headlightCount',0)
    dry_unlit=material(2);scalar('environmentWet',1);wet=material(2)
    assert wet!=dry_unlit,(wet,dry_unlit,'wet response must be checked below display clipping')
    print('headlights068: real pixels pass damage-independent player/AI lamps, raster-side-invariant normals, vehicle occlusion, rendered depth shadow, day/night, normals, cone, eighth AI, range, cookie and wet response',day,night,bright,ai_lit,wet)
    scalar('environmentNight',0);scalar('environmentWet',0);flag('sceneSurface',0);flag('headlightCount',0)
    matrix('world',identity);matrix('view',identity)
    active_tex(0x84C0);bind_tex(0x0DE1,base_material)
    active_tex(0x84C1);bind_tex(0x0DE1,light_material)
'''
marker='    assert all(abs(a-b)<=1 for a,b in zip(material(1)'
assert marker in base
base=base.replace(marker,checks+'\n'+marker,1)
exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
