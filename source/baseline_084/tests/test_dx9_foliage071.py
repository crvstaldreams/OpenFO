#!/usr/bin/env python3
"""Execute scene_vs foliage equations with Mesa transform feedback.
Only HLSL type/semantic spelling is translated; shipped expansion stays intact.
Optional argument validates the original HLSL with glslang as well.
"""
from pathlib import Path
import re,sys,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
text=(root/'src/dx9_scene_shaders.hpp').read_text()
hlsl=re.search(r'R"HLSL\((.*?)\)HLSL"',text,re.S).group(1)
if len(sys.argv)>1:
    with tempfile.TemporaryDirectory() as tmp:
        source=Path(tmp)/'scene.hlsl';source.write_text(hlsl)
        subprocess.run([sys.argv[1],'-D','--hlsl-dx9-compatible','-V','-S','vert','-e','scene_vs',str(source),'-o',str(Path(tmp)/'scene.spv')],check=True)
structs=hlsl[hlsl.index('struct Input'):hlsl.index('// Vertex-only')]
vs=hlsl[hlsl.index('Output scene_vs'):hlsl.index('// ps_3_0')]
code=re.sub(r':(?:POSITION|NORMAL|COLOR|TEXCOORD)\d','',structs)+vs
for h,g in [('float2','vec2'),('float3','vec3'),('float4','vec4')]:code=code.replace(h,g)
code=code.replace('(int)i.normal.z','int(i.normal.z)').replace('tint.a','tint.w').replace('frac(','fract(')
code='''#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;
layout(location=2)in vec4 tint;layout(location=3)in vec2 uv;layout(location=4)in vec2 uv1;
uniform vec4 matrixRows[12],materialControl,camera,billboardRight,billboardUp,foliageWind[16],foliageFade;
out vec4 outClip,outTint;out vec3 outWorld,outNormal;out vec2 outUV,outUV1;
float saturate(float v){return clamp(v,0.,1.);}
'''+code+'''
void main(){Input i;i.position=position;i.normal=normal;i.color=tint;i.uv=uv;i.uv1=uv1;
Output o=scene_vs(i);gl_Position=o.position;outClip=o.position;outWorld=o.world;outNormal=o.normal;outUV=o.uv;outUV1=o.uv1;outTint=o.color;}
'''
base=Path(__file__).with_name('test_gl_effects_shaders.py').read_text()
checks=r'''
    import math
    shader=create_shader(0x8B31);src=C.c_char_p(FOLIAGE_CODE.encode());source(shader,1,C.byref(src),None);compile_shader(shader)
    ok=integer();get_shader(shader,0x8B81,C.byref(ok));log=C.create_string_buffer(16384);shader_log(shader,len(log),None,log)
    assert ok.value,log.value.decode()
    p=create_program();attach(p,shader)
    names=(C.c_char_p*6)(b'outClip',b'outWorld',b'outNormal',b'outUV',b'outUV1',b'outTint')
    function(gl,'glTransformFeedbackVaryings',None,uint,integer,C.POINTER(C.c_char_p),uint)(p,6,names,0x8C8C)
    link(p);get_program(p,0x8B82,C.byref(ok));assert ok.value,'foliage feedback link'
    use(p)
    matrices=[0.]*48
    for offset in (0,16):
        for j in range(4):matrices[offset+j*4+j]=1.
    for j in range(3):matrices[32+j*4+j]=1.
    function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(p,b'matrixRows[0]'),12,(C.c_float*48)(*matrices))
    uniform4(location(p,b'camera'),0,0,0,1)
    uniform4(location(p,b'billboardRight'),0,0,-1,0);uniform4(location(p,b'billboardUp'),0,1,0,0)
    uniform4(location(p,b'foliageFade'),150,1/50,0,0)
    feedback=uint();inputs=uint();function(gl,'glGenBuffers',None,integer,C.POINTER(uint))(1,C.byref(feedback));function(gl,'glGenBuffers',None,integer,C.POINTER(uint))(1,C.byref(inputs))
    bind_buffer(0x8892,inputs.value)
    for index,size_,offset in ((0,3,0),(1,3,3),(2,4,6),(3,2,10),(4,2,12)):
        function(gl,'glEnableVertexAttribArray',None,uint)(index)
        function(gl,'glVertexAttribPointer',None,uint,integer,uint,C.c_ubyte,integer,ptr)(index,size_,0x1406,0,56,ptr(offset*4))
    function(gl,'glEnable',None,uint)(0x8C89)
    def execute(vertices,mode):
        use(p);uniform4(location(p,b'materialControl'),mode,0,0,0)
        data=(C.c_float*len(vertices))(*vertices);bind_buffer(0x8892,inputs.value)
        function(gl,'glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8892,C.sizeof(data),data,0x88E0)
        count=len(vertices)//14;bind_buffer(0x8C8E,feedback.value)
        function(gl,'glBufferData',None,uint,C.c_ssize_t,ptr,uint)(0x8C8E,count*18*4,None,0x88E1)
        function(gl,'glBindBufferBase',None,uint,uint,uint)(0x8C8E,0,feedback.value)
        function(gl,'glBeginTransformFeedback',None,uint)(0);draw(0,0,count);function(gl,'glEndTransformFeedback',None)()
        result=(C.c_float*(count*18))();function(gl,'glGetBufferSubData',None,uint,C.c_ssize_t,C.c_ssize_t,ptr)(0x8C8E,0,C.sizeof(result),result)
        return [list(result[i*18:(i+1)*18]) for i in range(count)]
    def close(a,b):assert max(abs(x-y) for x,y in zip(a,b))<4e-5,(a,b)
    tested=0
    for time in (0.,.001,1.23,4.095,4.096,9.875):
        t=math.floor((time*1000)%4096)*.0015339808305725455
        wind=[]
        for phase in range(16):
            angle=t+phase*.39269909262657166;tilt=math.sin(angle)*.27925267815589905
            wind.append((math.sin(tilt),math.cos(tilt),math.cos(angle)*.1))
        flat=[v for w in wind for v in (*w,0)]
        function(gl,'glUniform4fv',None,integer,integer,C.POINTER(C.c_float))(location(p,b'foliageWind[0]'),16,(C.c_float*64)(*flat))
        vertices=[];expected=[]
        for phase in range(16):
            for angle in (0.,.7,2.9):
                dx,dz=math.cos(angle),math.sin(angle);w=wind[phase]
                for width,top in ((1.7,0),(-1.7,0),(-1.7,2.3),(1.7,2.3)):
                    base=(11,3,-7);vertices.extend((*base,dx,dz,phase,.25,.5,.75,.5,.1,.2,width,top))
                    edge=abs(width)*w[2] if top>=.00001 else 0
                    position=(11+dx*width+dz*top*w[0]+dx*edge,3+top*w[1],-7+dz*width-dx*top*w[0]+dz*edge)
                    normal=(-dz*w[1],w[0],dx*w[1])
                    expected.append((position,normal,(width,top)))
        for out,(pos,n,uv1_) in zip(execute(vertices,21),expected):
            close(out[:4],(*pos,1));close(out[4:7],pos);close(out[7:10],n);close(out[10:14],(.1,.2,*uv1_))
            close(out[14:],(.25,.5,.75,1));tested+=1
    for distance in (90.,100.,125.,150.,170.):
        uniform4(location(p,b'camera'),11+distance,3,-7,1)
        out=execute([11,3,-7,1,0,0,1,1,1,.5,0,0,1,2],21)[0]
        assert abs(out[-1]-max(0,min(1,(150-distance)/50)))<1e-5,(distance,out)
    # Billboard expands encoded corner and supplies a physical plane normal.
    tree=execute([3,4,5,1,2,0,.25,.5,.75,.6,.1,.2,0,0],20)[0]
    close(tree[:4],(2,4,4,1));close(tree[7:10],(1,0,0));close(tree[14:],(.25,.5,.75,.6))
    function(gl,'glDisable',None,uint)(0x8C89)
    function(gl,'glDeleteBuffers',None,integer,C.POINTER(uint))(1,C.byref(feedback));function(gl,'glDeleteBuffers',None,integer,C.POINTER(uint))(1,C.byref(inputs));function(gl,'glDeleteShader',None,uint)(shader);function(gl,'glDeleteProgram',None,uint)(p)
    print('DX9 scene_vs GPU foliage:',tested,'wind corners, deterministic phases, distance fade, expanded world position and physical normals, camera billboard passed')
'''
checks=checks.replace('FOLIAGE_CODE',repr(code))
marker="    function(gl, 'glDeleteVertexArrays'"
base=base.replace(marker,checks+'\n'+marker,1)
exec(compile(base,str(Path(__file__).with_name('test_gl_effects_shaders.py')),'exec'))
