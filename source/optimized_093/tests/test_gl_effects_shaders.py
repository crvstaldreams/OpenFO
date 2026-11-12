#!/usr/bin/env python3
"""Compile/link the actual GLSL with a surfaceless Mesa OpenGL 3.3 context.
Optional developer smoke test: requires libEGL and libGL; no window or GPU.
This validates shaders, not Windows WGL or target-hardware performance.
"""
import ctypes as C
import ctypes.util
from pathlib import Path
import re


def run():
    egl = C.CDLL(ctypes.util.find_library('EGL'))
    gl = C.CDLL(ctypes.util.find_library('GL'))
    def function(lib, name, result, *args):
        f = getattr(lib, name); f.restype = result; f.argtypes = args; return f
    ptr, integer, uint = C.c_void_p, C.c_int, C.c_uint
    get_proc = function(egl, 'eglGetProcAddress', ptr, C.c_char_p)
    get_platform = C.CFUNCTYPE(ptr, uint, ptr, C.POINTER(integer))(get_proc(b'eglGetPlatformDisplayEXT'))
    display = get_platform(0x31DD, None, None)  # EGL_PLATFORM_SURFACELESS_MESA
    major, minor = integer(), integer()
    assert function(egl, 'eglInitialize', uint, ptr, C.POINTER(integer), C.POINTER(integer))(display, C.byref(major), C.byref(minor)), 'EGL initialization'
    assert function(egl, 'eglBindAPI', uint, uint)(0x30A2), 'OpenGL API'
    attributes = (integer * 11)(0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3033, 1, 0x3040, 8, 0x3038)
    config, count = ptr(), integer()
    assert function(egl, 'eglChooseConfig', uint, ptr, C.POINTER(integer), C.POINTER(ptr), integer, C.POINTER(integer))(display, attributes, C.byref(config), 1, C.byref(count)) and count.value
    attrs = (integer * 7)(0x3098, 3, 0x30FB, 3, 0x30FD, 1, 0x3038)
    context = function(egl, 'eglCreateContext', ptr, ptr, ptr, ptr, C.POINTER(integer))(display, config, None, attrs)
    assert context, 'OpenGL 3.3 context'
    assert function(egl, 'eglMakeCurrent', uint, ptr, ptr, ptr, ptr)(display, None, None, context)
    create_shader = function(gl, 'glCreateShader', uint, uint)
    source = function(gl, 'glShaderSource', None, uint, integer, C.POINTER(C.c_char_p), C.POINTER(integer))
    compile_shader = function(gl, 'glCompileShader', None, uint)
    get_shader = function(gl, 'glGetShaderiv', None, uint, uint, C.POINTER(integer))
    shader_log = function(gl, 'glGetShaderInfoLog', None, uint, integer, C.POINTER(integer), C.c_char_p)
    create_program = function(gl, 'glCreateProgram', uint)
    attach = function(gl, 'glAttachShader', None, uint, uint)
    link = function(gl, 'glLinkProgram', None, uint)
    get_program = function(gl, 'glGetProgramiv', None, uint, uint, C.POINTER(integer))
    program_log = function(gl, 'glGetProgramInfoLog', None, uint, integer, C.POINTER(integer), C.c_char_p)
    text = (Path(__file__).resolve().parents[1] / 'src/gl_screen_effects_shaders.hpp').read_text()
    shaders = dict(re.findall(r'inline const char\* (\w+)\(\)\{return R"GLSL\((.*?)\)GLSL";', text, re.S))
    def compile_one(name):
        shader = create_shader(0x8B31 if name.endswith('_vs') else 0x8B30)
        value = C.c_char_p(shaders[name].encode()); source(shader, 1, C.byref(value), None); compile_shader(shader)
        ok = integer(); get_shader(shader, 0x8B81, C.byref(ok))
        log = C.create_string_buffer(16384); shader_log(shader, len(log), None, log)
        assert ok.value, f'{name}: {log.value.decode()}'
        return shader
    compiled = {name: compile_one(name) for name in shaders}
    programs = {}
    for fragment in ('ao', 'composite', 'water', 'photo'):
        vertex = 'gl_effects_water_vs' if fragment == 'water' else 'gl_effects_fullscreen_vs'
        program = create_program(); attach(program, compiled[vertex]); attach(program, compiled[f'gl_effects_{fragment}_fs']); link(program)
        ok = integer(); get_program(program, 0x8B82, C.byref(ok))
        log = C.create_string_buffer(16384); program_log(program, len(log), None, log)
        assert ok.value, f'{fragment} link: {log.value.decode()}'
        programs[fragment] = program
        print(f'{fragment}: GLSL compile/link passed')
    material_text = (Path(__file__).resolve().parents[1] / 'src/gl_compat_win.hpp').read_text()
    material_sources = dict(re.findall(r'inline const char\*\s*(gl_material_\w+)\(\)\{return R"GLSL\((.*?)\)GLSL";', material_text, re.S))
    material_shaders = []
    for name, contents in material_sources.items():
        shader = create_shader(0x8B31 if name.endswith('vertex') else 0x8B30)
        value = C.c_char_p(contents.encode()); source(shader, 1, C.byref(value), None); compile_shader(shader)
        ok = integer(); get_shader(shader, 0x8B81, C.byref(ok))
        log = C.create_string_buffer(16384); shader_log(shader, len(log), None, log)
        assert ok.value, f'{name}: {log.value.decode()}'
        material_shaders.append(shader)
    material_program = create_program()
    for shader in material_shaders: attach(material_program, shader)
    link(material_program); ok = integer(); get_program(material_program, 0x8B82, C.byref(ok))
    log = C.create_string_buffer(16384); program_log(material_program, len(log), None, log)
    assert ok.value, f'material link: {log.value.decode()}'
    uniform_count = integer(); get_program(material_program, 0x8B86, C.byref(uniform_count))
    active_uniform = function(gl, 'glGetActiveUniform', None, uint, uint, integer, C.POINTER(integer), C.POINTER(integer), C.POINTER(uint), C.c_char_p)
    for index in range(uniform_count.value):
        size, kind = integer(), uint(); name = C.create_string_buffer(256)
        active_uniform(material_program, index, len(name), None, C.byref(size), C.byref(kind), name)
        if name.value == b'c[0]': print(f'material: GLSL compile/link passed; active c array length = {size.value}')
    function(gl, 'glUseProgram', None, uint)(material_program)
    c_location = function(gl, 'glGetUniformLocation', integer, uint, C.c_char_p)(material_program, b'c[0]')
    function(gl, 'glUniform4fv', None, integer, integer, C.POINTER(C.c_float))(c_location, 96, (C.c_float * 384)())
    assert function(gl, 'glGetError', uint)() == 0, 'material 96-register upload error'
    for shader in material_shaders: function(gl, 'glDeleteShader', None, uint)(shader)
    # Actual fullscreen draws: source and output depth are different textures.
    bind_fbo = function(gl, 'glBindFramebuffer', None, uint, uint)
    gen_fbos = function(gl, 'glGenFramebuffers', None, integer, C.POINTER(uint))
    attach_tex = function(gl, 'glFramebufferTexture2D', None, uint, uint, uint, uint, integer)
    bind_tex = function(gl, 'glBindTexture', None, uint, uint)
    active_tex = function(gl, 'glActiveTexture', None, uint)
    image = function(gl, 'glTexImage2D', None, uint, integer, integer, integer, integer, integer, uint, uint, ptr)
    parameter = function(gl, 'glTexParameteri', None, uint, uint, integer)
    gen_tex = function(gl, 'glGenTextures', None, integer, C.POINTER(uint))
    textures = []
    def texture(depth=False, values=None):
        tex = uint(); gen_tex(1, C.byref(tex)); textures.append(tex)
        bind_tex(0x0DE1, tex.value)
        for key in (0x2800, 0x2801): parameter(0x0DE1, key, 0x2600)
        for key in (0x2802, 0x2803): parameter(0x0DE1, key, 0x812F)
        image(0x0DE1, 0, 0x81A6 if depth else 0x8058, 4, 4, 0, 0x1902 if depth else 0x1908, 0x1406 if depth else 0x1401, values)
        return tex.value
    scene = texture(values=(C.c_ubyte * 64)(*([128, 64, 192, 255] * 16)))
    depth = texture(True, (C.c_float * 16)(*([.6] * 16)))
    ao = texture(values=(C.c_ubyte * 64)(*([128, 128, 128, 255] * 16)))
    output = texture(); output_depth = texture(True)
    fbo = uint(); gen_fbos(1, C.byref(fbo)); bind_fbo(0x8D40, fbo.value)
    attach_tex(0x8D40, 0x8CE0, 0x0DE1, output, 0); attach_tex(0x8D40, 0x8D00, 0x0DE1, output_depth, 0)
    assert function(gl, 'glCheckFramebufferStatus', uint, uint)(0x8D40) == 0x8CD5
    vao = uint(); function(gl, 'glGenVertexArrays', None, integer, C.POINTER(uint))(1, C.byref(vao))
    function(gl, 'glBindVertexArray', None, uint)(vao.value)
    function(gl, 'glViewport', None, integer, integer, integer, integer)(0, 0, 4, 4)
    use = function(gl, 'glUseProgram', None, uint)
    location = function(gl, 'glGetUniformLocation', integer, uint, C.c_char_p)
    uniform1 = function(gl, 'glUniform1i', None, integer, integer)
    uniform2 = function(gl, 'glUniform2f', None, integer, C.c_float, C.c_float)
    uniform4 = function(gl, 'glUniform4f', None, integer, C.c_float, C.c_float, C.c_float, C.c_float)
    draw = function(gl, 'glDrawArrays', None, uint, integer, integer)
    read = function(gl, 'glReadPixels', None, integer, integer, integer, integer, uint, uint, ptr)
    def sources(program):
        use(program)
        for unit, tex, name in ((0, scene, b'sceneColor'), (1, depth, b'sceneDepth'), (2, ao, b'aoTexture')):
            active_tex(0x84C0 + unit); bind_tex(0x0DE1, tex); uniform1(location(program, name), unit)
    # Draw the actual material program, including its vertex ABI, into the same FBO.
    bind_buffer = function(gl, 'glBindBuffer', None, uint, uint)
    vbo = uint(); function(gl, 'glGenBuffers', None, integer, C.POINTER(uint))(1, C.byref(vbo))
    bind_buffer(0x8892, vbo.value)
    for index, size, offset in ((0, 3, 0), (1, 3, 3), (2, 4, 6), (3, 2, 10), (4, 2, 12)):
        function(gl, 'glEnableVertexAttribArray', None, uint)(index)
        function(gl, 'glVertexAttribPointer', None, uint, integer, uint, C.c_ubyte, integer, ptr)(index, size, 0x1406, 0, 56, ptr(offset*4))
    base_material = texture(values=(C.c_ubyte * 64)(*([128, 64, 192, 64] * 16)))
    light_material = texture(values=(C.c_ubyte * 64)(*([128, 128, 128, 128] * 16)))
    constants = (C.c_float * 384)()
    for channel in range(3):
        constants[19*4+channel] = .55
        constants[21*4+channel] = .55
    constants[95*4+3] = .5
    use(material_program)
    function(gl, 'glUniform4fv', None, integer, integer, C.POINTER(C.c_float))(location(material_program, b'c[0]'), 96, constants)
    identity = (C.c_float * 16)(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
    for name in (b'world', b'view', b'projection', b'uvMatrix'):
        function(gl, 'glUniformMatrix4fv', None, integer, integer, C.c_ubyte, C.POINTER(C.c_float))(location(material_program, name), 1, 0, identity)
    function(gl, 'glUniform3f', None, integer, C.c_float, C.c_float, C.c_float)(location(material_program,b'sun'),0,0,1)
    uniform1(location(material_program, b'texA'), 0); uniform1(location(material_program, b'texB'), 1)
    # Different sampler types must use different units even when the cube is absent.
    uniform1(location(material_program, b'texEnvironment'), 2)
    uniform1(location(material_program, b'hasA'), 1); uniform1(location(material_program, b'hasB'), 1)
    active_tex(0x84C0); bind_tex(0x0DE1,base_material)
    active_tex(0x84C1); bind_tex(0x0DE1,light_material)
    function(gl, 'glDisable', None, uint)(0x0B71)
    function(gl, 'glDisable', None, uint)(0x0BE2)
    def material(mode, normal=(0,0,-1), cracked=0, mirrored=False, vertex_alpha=.5, pixel_x=2, vertex_rgb=(1,1,1), detail_uv=False):
        vertices=[]
        for x,y in ((-1,-1),(3,-1),(-1,3)):
            u=.5-x*.5 if mirrored else x*.5+.5
            v=y*.5+.5
            vertices.extend((x,y,.5,*normal,*vertex_rgb,vertex_alpha,u,v,u if detail_uv else .5,v if detail_uv else .5))
        data=(C.c_float * len(vertices))(*vertices)
        bind_buffer(0x8892,vbo.value)
        function(gl,'glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8892,C.sizeof(data),data,0x88E0)
        use(material_program); uniform1(location(material_program,b'materialMode'),mode)
        uniform1(location(material_program,b'materialCracked'),cracked)
        draw(4,0,3); value=(C.c_ubyte*4)(); read(pixel_x,2,1,1,0x1908,0x1401,value)
        return list(value)
    assert all(abs(a-b)<=1 for a,b in zip(material(1),(255,128,255,32))), material(1)
    assert all(abs(a-b)<=1 for a,b in zip(material(2),(64,32,96,32))), material(2)
    assert all(abs(a-b)<=1 for a,b in zip(material(6),(255,128,255,64))), material(6)
    assert material(3)[3] == 255, 'car base alpha must be gloss, not opacity'
    assert material(3,vertex_rgb=(.1,.3,.6))==material(3), 'native body RGB input is not diffuse tint; alpha only blends damage'
    clean_head=material(4); clean_grazing=material(4,normal=(1,0,0))
    assert clean_grazing[3]>clean_head[3], (clean_head,clean_grazing)
    fractured=material(4,cracked=1)
    assert abs(fractured[3]-32)<=1, fractured
    assert fractured[3]<clean_head[3], (fractured,clean_head)
    # An actual samplerCube must affect reflection RGB without changing fracture opacity.
    cube=uint(); gen_tex(1,C.byref(cube)); textures.append(cube)
    active_tex(0x84C2); bind_tex(0x8513,cube.value)
    for face in range(6):
        image(0x8515+face,0,0x8058,1,1,0,0x1908,0x1401,(C.c_ubyte*4)(255,0,0,255))
    for key in (0x2800,0x2801): parameter(0x8513,key,0x2600)
    uniform1(location(material_program,b'hasEnvironment'),1)
    reflected=material(4)
    assert reflected[0]>clean_head[0], (reflected,clean_head)
    assert abs(material(4,cracked=1)[3]-32)<=1
    uniform1(location(material_program,b'hasEnvironment'),0)
    # Removing dirt lowers opacity; the texture alpha is a dirt mask for clean glass.
    active_tex(0x84C0)
    clear_material=texture(values=(C.c_ubyte*64)(*([128,64,192,0]*16)))
    bind_tex(0x0DE1,clear_material)
    clear_head=material(4)
    assert clear_head[3]<clean_head[3], (clear_head,clean_head)
    bind_tex(0x0DE1,base_material)
    # Actual derivative TBN pixels: opposite tangent slopes and mirrored UVs.
    uniform1(location(material_program,b'texNormal'),3)
    active_tex(0x84C3)
    normal_right=texture(values=(C.c_ubyte*64)(*([224,128,224,255]*16)))
    normal_left=texture(values=(C.c_ubyte*64)(*([32,128,224,255]*16)))
    uniform1(location(material_program,b'hasNormal'),1)
    function(gl,'glUniform3f',None,integer,C.c_float,C.c_float,C.c_float)(location(material_program,b'sun'),-1,0,1)
    # Native body never samples a generated normal: paint logos and rust RGB
    # are surface color, not height. Both normals must preserve paint, gloss
    # reflection and the damage blend, including an active reflection cube.
    uniform1(location(material_program,b'hasEnvironment'),1)
    uniform1(location(material_program,b'texDirt'),4);active_tex(0x84C4)
    body_damage=texture(values=(C.c_ubyte*64)(*([72,36,18,192]*16)))
    uniform1(location(material_program,b'hasDirt'),1)
    for blend in (0,.5,1):
        active_tex(0x84C3);bind_tex(0x0DE1,normal_right);body_right=material(3,vertex_alpha=blend)
        bind_tex(0x0DE1,normal_left);body_left=material(3,vertex_alpha=blend)
        uniform1(location(material_program,b'hasNormal'),0);body_flat=material(3,vertex_alpha=blend)
        assert body_right==body_left==body_flat,(blend,body_right,body_left,body_flat)
        uniform1(location(material_program,b'hasNormal'),1)
    uniform1(location(material_program,b'hasEnvironment'),0)
    uniform1(location(material_program,b'hasDirt'),0)
    active_tex(0x84C3)
    bind_tex(0x0DE1,normal_right); right=material(8); mirrored=material(8,mirrored=True)
    bind_tex(0x0DE1,normal_left); left=material(8)
    assert abs(right[0]-left[0])>20,(right,left)
    assert abs(mirrored[0]-left[0])<=2,(mirrored,left)
    # Disabling normal input preserves diffuse alpha.
    uniform1(location(material_program,b'hasNormal'),0)
    flat=material(8); assert flat[3] in range(63,66),flat
    # Authored dirt uses source UV0 and vertex-alpha interpolation, retaining gloss alpha.
    uniform1(location(material_program,b'texDirt'),4); active_tex(0x84C4)
    dirt=texture(values=(C.c_ubyte*64)(*sum(([32+48*x,24,8,0] for y in range(4) for x in range(4)),[])))
    bind_tex(0x0DE1,dirt);uniform1(location(material_program,b'hasDirt'),1)
    clean_body=material(3,vertex_alpha=0);rust_body=material(3,vertex_alpha=1)
    assert rust_body[0]<clean_body[0] and rust_body[2]<clean_body[2],(clean_body,rust_body)
    assert clean_body[3]==rust_body[3]==255
    dirt_left=material(3,vertex_alpha=1,pixel_x=0);dirt_right=material(3,vertex_alpha=1,pixel_x=3)
    assert dirt_right[0]>dirt_left[0]+40,(dirt_left,dirt_right)
    # Optional real native DDS fixture: sample Pepper's authored worn paint,
    # logos and gloss, and its matching damage atlas at the same UV positions.
    # This deliberately uses RGB regardless of DDS alpha: body alpha is gloss,
    # not a cutout or a premultiplication factor. Original assets stay external.
    import os
    data_root = os.environ.get('OPENFO_TEST_DATA')
    if data_root:
        from PIL import Image
        native_base = Image.open(Path(data_root)/'cars/car_3/skin1.dds').convert('RGBA')
        native_damage = Image.open(Path(data_root)/'cars/car_3/skin1_damaged.dds').convert('RGBA')
        native_uvs = ((100,100),(400,400),(550,80),(230,725))
        for channel in range(3):
            constants[19*4+channel] = .6
            constants[21*4+channel] = 0
        function(gl, 'glUniform4fv', None, integer, integer, C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
        # Facing away from the light removes specular; no environment means the
        # result is the native RGB lerp times .6, with independent gloss alpha.
        function(gl,'glUniform3f',None,integer,C.c_float,C.c_float,C.c_float)(location(material_program,b'sun'),0,0,-1)
        uniform1(location(material_program,b'hasNormal'),0)
        uniform1(location(material_program,b'hasEnvironment'),0)
        for x,y in native_uvs:
            clean = native_base.getpixel((x,y)); damaged = native_damage.getpixel((x,y))
            active_tex(0x84C0); clean_tex=texture(values=(C.c_ubyte*64)(*(clean*16)))
            active_tex(0x84C4); damage_tex=texture(values=(C.c_ubyte*64)(*(damaged*16)))
            for blend in (0,.5,1):
                actual=material(3,vertex_alpha=blend)
                expected=[round(.6*((1-blend)*clean[k]+blend*damaged[k])) for k in range(3)]+[255]
                assert all(abs(a-b)<=1 for a,b in zip(actual,expected)), (x,y,blend,clean,damaged,actual,expected)
        # Restore shared fixtures for the remaining shader checks.
        for channel in range(3):
            constants[19*4+channel] = .55
            constants[21*4+channel] = .55
        function(gl, 'glUniform4fv', None, integer, integer, C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
        active_tex(0x84C0);bind_tex(0x0DE1,base_material)
        active_tex(0x84C4);bind_tex(0x0DE1,dirt)
        function(gl,'glUniform3f',None,integer,C.c_float,C.c_float,C.c_float)(location(material_program,b'sun'),-1,0,1)
        print('Native Pepper DDS paint/damage RGB at zero/mid/full blend: 12 opaque GPU pixels passed')
    # Static/lightmapped opaque surface responds to the same derivative TBN.
    uniform1(location(material_program,b'hasNormal'),1);active_tex(0x84C3);bind_tex(0x0DE1,normal_left)
    stone_left=material(2);stone_mirror=material(2,mirrored=True)
    assert stone_left[0]!=stone_mirror[0],(stone_left,stone_mirror)
    uniform1(location(material_program,b'hasNormal'),0)
    uniform1(location(material_program,b'hasDirt'),0)
    # One packed texture provides tangent XY, specular intensity and roughness.
    # Exercise a lightmapped material: the normal needs UV1, and changes to
    # microdetail must not affect the existing diffuse/lightmap alpha product.
    def material_flag(name,value):
        uniform1(location(material_program,name.encode()),value)
    def material_sun(x,y,z):
        function(gl,'glUniform3f',None,integer,C.c_float,C.c_float,C.c_float)(location(material_program,b'sun'),x,y,z)
    def surface_pixel(r,g,b,a):
        active_tex(0x84C3)
        texture(values=(C.c_ubyte*64)(*([r,g,b,a]*16)))
        return material(2,detail_uv=True)
    material_flag('sceneSurface',1)
    material_flag('normalUVIndex',1)
    material_flag('hasNormal',2)
    material_sun(-1,0,1)
    packed_left=surface_pixel(48,128,0,230);packed_right=surface_pixel(208,128,0,230)
    packed_flat=surface_pixel(128,128,0,230)
    assert packed_left[3]==packed_right[3]==packed_flat[3]==32,(packed_left,packed_right,packed_flat)
    assert abs(packed_left[0]-packed_right[0])>=5,(packed_left,packed_right,'bump must be visible on prelit UV1')
    material_sun(-.5,-.5,1)
    no_spec=surface_pixel(128,128,0,28)
    matte=surface_pixel(128,128,255,245);glossy=surface_pixel(128,128,255,28)
    assert glossy[0]>matte[0]+6 and matte[0]>no_spec[0]+2,(no_spec,matte,glossy,'specular mask and roughness')
    material_sun(0,0,1)
    no_spec_off=surface_pixel(128,128,0,28)
    matte_off=surface_pixel(128,128,255,245);glossy_off=surface_pixel(128,128,255,28)
    assert matte_off[0]-no_spec_off[0]>glossy_off[0]-no_spec_off[0],(matte_off,glossy_off,no_spec_off,'rough material has wider lobe away from mirror angle')
    material_flag('hasNormal',0)
    material_flag('normalUVIndex',0)
    material_flag('sceneSurface',0)
    material_sun(-1,0,1)
    print('Packed track surface GPU pixels: UV1 relief, baked lightmap alpha, specular mask and roughness width passed',packed_left,packed_right,no_spec,matte,glossy)
    # Original dynamic declaration has no COLOR input: RGB and alpha are texture/light only.
    assert material(8,vertex_rgb=(0,0,0),vertex_alpha=0)==material(8,vertex_alpha=1), 'dynamic must ignore unused vertex color'
    # PRO lamp base is lit, Tex2 is added without diffuse or vertex-alpha modulation.
    uniform1(location(material_program,b'texDirt'),4);active_tex(0x84C4);bind_tex(0x0DE1,dirt)
    uniform1(location(material_program,b'hasDirt'),0);lamp_base=material(16)
    uniform1(location(material_program,b'hasDirt'),1);lamp_on=material(16)
    assert lamp_on[0]>lamp_base[0]+25 and lamp_on[1]>lamp_base[1],(lamp_base,lamp_on)
    assert material(16,vertex_alpha=0)==material(16,vertex_alpha=1)
    uniform1(location(material_program,b'hasDirt'),0)
    # Water opacity is independent of caustic RGB alpha and source vertex alpha.
    water=material(7,normal=(0,0,-1));assert water[3]>=209,water
    # Trees stay upright when the camera rolls; encoded centers stay fixed.
    constants[10*4]=1;constants[11*4+1]=1
    function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
    data=(C.c_float*42)(*sum(([x,y,.5,x,y,0,1,1,1,1,.5,.5,.5,.5] for x,y in ((-1,-1),(3,-1),(-1,3))),[]))
    bind_buffer(0x8892,vbo.value);function(gl,'glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8892,C.sizeof(data),data,0x88E0)
    use(material_program);uniform1(location(material_program,b'materialMode'),20)
    function(gl,'glClearColor',None,C.c_float,C.c_float,C.c_float,C.c_float)(0,1,0,1)
    function(gl,'glClear',None,uint)(0x4000);draw(4,0,3)
    billboard=(C.c_ubyte*4)();read(2,2,1,1,0x1908,0x1401,billboard)
    assert list(billboard)!=[0,255,0,255],list(billboard)
    upright=list(billboard)
    constants[10*4+1]=.7;constants[11*4+1]=.6;constants[11*4+2]=.8
    function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(material_program,b'c[0]'),96,constants)
    function(gl,'glClear',None,uint)(0x4000);draw(4,0,3);read(2,2,1,1,0x1908,0x1401,billboard)
    assert list(billboard)==upright,list(billboard)
    active_tex(0x84C0);bind_tex(0x0DE1,base_material)
    # UI uses captured texture*diffuse alpha, including partially transparent meter art.
    uniform1(location(material_program,b'isUI'),1)
    uniform2(location(material_program,b'viewport'),4,4)
    # Test this using world path coordinates but the UI alpha operations explicitly.
    uniform1(location(material_program,b'isUI'),0)
    for name in (b'colorStage0', b'alphaStage0'):
        function(gl,'glUniform4i',None,integer,integer,integer,integer,integer)(location(material_program,name),4,2,0,0)
    assert abs(material(0)[3]-32)<=1, 'texture alpha times diffuse alpha'
    function(gl,'glDeleteBuffers',None,integer,C.POINTER(uint))(1,C.byref(vbo))
    function(gl,'glDeleteProgram',None,uint)(material_program)
    print('Material GPU pixels: static2x, lightmap, clouds, opaque body gloss, glass grazing Fresnel/dirt, cube reflection, fracture alpha, texture/diffuse alpha, mirrored derivative TBN, authored dirt/gloss, opaque water, billboard basis expansion passed')
    program = programs['composite']; sources(program)
    uniform4(location(program, b'projection'), 1., 1., 1.001, -.1001)
    uniform2(location(program, b'aoTexel'), .25, .25); uniform1(location(program, b'aoEnabled'), 1)
    function(gl, 'glEnable', None, uint)(0x0B71); function(gl, 'glDepthFunc', None, uint)(0x0207)
    draw(4, 0, 3)
    pixel = (C.c_ubyte * 4)(); raw_depth = C.c_float()
    read(1, 1, 1, 1, 0x1908, 0x1401, pixel); read(1, 1, 1, 1, 0x1902, 0x1406, C.byref(raw_depth))
    assert all(abs(a-b) <= 1 for a,b in zip(pixel, (64, 32, 96, 255))), list(pixel)
    assert abs(raw_depth.value-.6) < 1e-6, raw_depth.value
    # Native environment adapter is neutral until explicitly enabled. Sun must see sky
    # in every sampled part of its footprint; occluders, rain and night suppress it.
    uniform1(location(program, b'aoEnabled'), 0)
    uniform4(location(program, b'atmosphere'), 4., .10, 0., 1.)
    uniform4(location(program, b'weather'), 1., 0., 0., 0.)
    uniform4(location(program, b'sunScreen'), .375, .375, 1., 1.)
    def composite_pixel():
        use(program); draw(4, 0, 3); read(1, 1, 1, 1, 0x1908, 0x1401, pixel)
        return list(pixel)
    def scene_depth(value):
        active_tex(0x84C1); bind_tex(0x0DE1, depth)
        image(0x0DE1, 0, 0x81A6, 4, 4, 0, 0x1902, 0x1406, (C.c_float*16)(*([value]*16)))
    blocked=composite_pixel()
    uniform4(location(program, b'sunScreen'), .375, .375, 0., 1.)
    without_sun=composite_pixel()
    assert blocked==without_sun, (blocked,without_sun,'sun through foreground')
    scene_depth(1.)
    uniform4(location(program, b'sunScreen'), .375, .375, 1., 1.)
    sunny=composite_pixel()
    assert sunny[0]>blocked[0]+25,(sunny,blocked,'sky halo absent')
    active_tex(0x84C1); bind_tex(0x0DE1, depth)
    footprint=[1.]*16;footprint[0]=.6
    image(0x0DE1,0,0x81A6,4,4,0,0x1902,0x1406,(C.c_float*16)(*footprint))
    assert composite_pixel()==blocked,'partial sun-footprint occluder leaked'
    scene_depth(1.)
    uniform4(location(program, b'sunScreen'), .375, .375, 1., 0.)
    behind=composite_pixel(); assert behind==blocked,(behind,blocked)
    uniform4(location(program, b'sunScreen'), .375, .375, 1., 1.)
    uniform4(location(program, b'weather'), 1., 1., 0., 0.)
    night=composite_pixel()
    uniform4(location(program, b'sunScreen'), .375, .375, 0., 1.)
    assert composite_pixel()==night,'night sun leak'
    assert night[0]<blocked[0] and night[2]>=blocked[2],(night,blocked)
    uniform4(location(program, b'weather'), 1., 0., 1., 0.)
    wet=composite_pixel(); assert wet!=blocked and wet!=night,(wet,blocked,night)
    # AO depth was restored for the following passes; neutral photo cannot inherit weather.
    scene_depth(.6)
    uniform4(location(program, b'weather'), 0., 0., 0., 0.)
    print('Environment GPU pixels: sky halo, foreground/behind-camera/night sun suppression, night and rain grading passed')
    program = programs['ao']; sources(program); uniform4(location(program, b'projection'), 1., 1., 1.001, -.1001)
    function(gl, 'glDisable', None, uint)(0x0B71)
    for samples in (4, 6, 8):
        uniform4(location(program, b'controls'), 1.4, .32, samples, 1.)
        draw(4, 0, 3); read(1, 1, 1, 1, 0x1908, 0x1401, pixel)
        assert list(pixel) == [255, 255, 255, 255], (samples, list(pixel))
    program = programs['photo']; sources(program)
    uniform4(location(program, b'controls'), 1., 1., 1., 0.); uniform1(location(program, b'preset'), 0)
    draw(4, 0, 3); read(1, 1, 1, 1, 0x1908, 0x1401, pixel)
    assert list(pixel) == [128, 64, 192, 255], list(pixel)
    assert function(gl, 'glGetError', uint)() == 0, 'OpenGL draw error'
    print('GPU draws: bilateral composite + copied depth, AO 4/6/8 flat plane, neutral photo passed')
    function(gl, 'glDeleteVertexArrays', None, integer, C.POINTER(uint))(1, C.byref(vao))
    function(gl, 'glDeleteFramebuffers', None, integer, C.POINTER(uint))(1, C.byref(fbo))
    for tex in textures: function(gl, 'glDeleteTextures', None, integer, C.POINTER(uint))(1, C.byref(tex))
    for program in programs.values(): function(gl, 'glDeleteProgram', None, uint)(program)
    for shader in compiled.values(): function(gl, 'glDeleteShader', None, uint)(shader)
    function(egl, 'eglMakeCurrent', uint, ptr, ptr, ptr, ptr)(display, None, None, None)
    function(egl, 'eglDestroyContext', uint, ptr, ptr)(display, context)
    function(egl, 'eglTerminate', uint, ptr)(display)


if __name__ == '__main__':
    run()
