#pragma once
// Private rendering-state adapter: D3D9 POD enums describe the inherited game
// materials. Every resource, shader, draw and presentation is OpenGL, with no
// Direct3D COM creation or runtime import.
#include "gl_api_win.hpp"
#include "assets.hpp"
#include <d3d9.h>
#include <array>
#include <vector>
#include <map>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <cstdint>
namespace ofo {
struct GLRef {unsigned refs=1;virtual ~GLRef()=default;ULONG AddRef(){return ++refs;}ULONG Release(){auto n=--refs;if(!n)delete this;return n;}};
struct GLShader:GLRef{};struct GLDeclaration:GLRef{};
struct GLQuery:GLRef {HRESULT Issue(DWORD){return S_OK;}HRESULT GetData(void*,DWORD,DWORD){return S_FALSE;}};
struct GLBuffer:GLRef {
 GLuint id=0,vao=0;UINT attrStride=0,attrOffset=0;int attrUI=-1;GLuint attrIndex=0;GLenum target;D3DFORMAT format=D3DFMT_UNKNOWN;std::vector<unsigned char>bytes;size_t capacity=0,lock_offset=0,lock_size=0;bool discard=false,dynamic=false;
 GLBuffer(size_t n,GLenum t,bool dyn=false):target(t),bytes(n),capacity(n),dynamic(dyn){gl.GenBuffers(1,&id);gl.BindBuffer(target,id);gl.BufferData(target,n,nullptr,dynamic?GL_STREAM_DRAW:GL_STATIC_DRAW);}
 ~GLBuffer(){if(vao)gl.DeleteVertexArrays(1,&vao);if(id)gl.DeleteBuffers(1,&id);}
 HRESULT Lock(UINT offset,UINT size,void**out,DWORD flags){if(!out||offset>capacity||(size&&size>capacity-offset))return E_INVALIDARG;if(bytes.size()!=capacity)bytes.resize(capacity);lock_offset=offset;lock_size=size?size:capacity-offset;discard=(flags&D3DLOCK_DISCARD)!=0;*out=bytes.data()+offset;return S_OK;}
 HRESULT Unlock(){gl.BindBuffer(target,id);if(discard)gl.BufferData(target,capacity,nullptr,GL_STREAM_DRAW);if(lock_size)gl.BufferSubData(target,lock_offset,lock_size,bytes.data()+lock_offset);if(!dynamic){bytes.clear();bytes.shrink_to_fit();}return S_OK;}
};
struct GLVertexBuffer:GLBuffer{GLVertexBuffer(size_t n,bool dyn):GLBuffer(n,GL_ARRAY_BUFFER,dyn){}};
struct GLIndexBuffer:GLBuffer{GLIndexBuffer(size_t n,D3DFORMAT f):GLBuffer(n,GL_ELEMENT_ARRAY_BUFFER){format=f;}GLenum type()const{return format==D3DFMT_INDEX16?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT;}};
inline std::vector<unsigned char> gl_decode_dxt(const unsigned char*data,int w,int h,D3DFORMAT format){
 std::vector<unsigned char>out(size_t(w)*h*4);size_t offset=0;const bool dxt1=format==D3DFMT_DXT1;const int block=dxt1?8:16;
 auto rgb=[](unsigned c){return std::array<unsigned,4>{((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31,255};};
 for(int by=0;by<h;by+=4)for(int bx=0;bx<w;bx+=4){auto*b=data+offset;auto*c=b+(dxt1?0:8);unsigned c0=c[0]|unsigned(c[1])<<8,c1=c[2]|unsigned(c[3])<<8;std::array<std::array<unsigned,4>,4>p{};p[0]=rgb(c0);p[1]=rgb(c1);if(c0>c1||!dxt1){for(int k=0;k<3;k++){p[2][k]=(2*p[0][k]+p[1][k])/3;p[3][k]=(p[0][k]+2*p[1][k])/3;}p[2][3]=p[3][3]=255;}else{for(int k=0;k<3;k++)p[2][k]=(p[0][k]+p[1][k])/2;p[2][3]=255;}
 uint32_t bits;std::memcpy(&bits,c+4,4);std::array<unsigned,8>a{};uint64_t ab=0;if(format==D3DFMT_DXT5){a[0]=b[0];a[1]=b[1];if(a[0]>a[1])for(int i=1;i<=6;i++)a[i+1]=((7-i)*a[0]+i*a[1])/7;else{for(int i=1;i<=4;i++)a[i+1]=((5-i)*a[0]+i*a[1])/5;a[6]=0;a[7]=255;}for(int i=0;i<6;i++)ab|=uint64_t(b[i+2])<<(i*8);}
 for(int k=0;k<16;k++){int x=bx+k%4,y=by+k/4;if(x>=w||y>=h)continue;auto pix=p[(bits>>(k*2))&3];if(format==D3DFMT_DXT3)pix[3]=((b[k/2]>>((k%2)*4))&15)*17;else if(format==D3DFMT_DXT5)pix[3]=a[(ab>>(k*3))&7];for(int j=0;j<4;j++)out[(size_t(y)*w+x)*4+j]=static_cast<unsigned char>(pix[j]);}offset+=block;}
 return out;
}
struct GLTexture:GLRef {
 GLuint id=0;int w,h,levels;D3DFORMAT format;bool staging;std::vector<std::vector<unsigned char>>mips;
 GLTexture(int width,int height,int count,D3DFORMAT f,bool cpu):w(width),h(height),levels(count),format(f),staging(cpu){if(!staging)glGenTextures(1,&id);mips.resize(levels);for(int l=0;l<levels;l++){int x=std::max(1,w>>l),y=std::max(1,h>>l);mips[l].resize(compressed()?size_t(std::max(1,(x+3)/4))*std::max(1,(y+3)/4)*(format==D3DFMT_DXT1?8:16):size_t(x)*y*4);}}
 ~GLTexture(){if(id)glDeleteTextures(1,&id);}
 bool compressed()const{return format==D3DFMT_DXT1||format==D3DFMT_DXT3||format==D3DFMT_DXT5;}
 HRESULT LockRect(UINT l,D3DLOCKED_RECT*out,const RECT*,DWORD){if(l>=mips.size()||!out)return E_INVALIDARG;out->pBits=mips[l].data();int x=std::max(1,w>>l);out->Pitch=compressed()?std::max(1,(x+3)/4)*(format==D3DFMT_DXT1?8:16):x*4;return S_OK;}
 HRESULT UnlockRect(UINT){return S_OK;}
 UINT GetLevelCount()const{return levels;}
 HRESULT GetLevelDesc(UINT l,D3DSURFACE_DESC*out){if(!out||l>=unsigned(levels))return E_INVALIDARG;*out={};out->Width=std::max(1,w>>l);out->Height=std::max(1,h>>l);out->Format=format;return S_OK;}
 void upload(bool s3tc){gl.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);for(int l=0;l<levels;l++){int x=std::max(1,w>>l),y=std::max(1,h>>l);if(compressed()&&s3tc){GLenum f=format==D3DFMT_DXT1?GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:format==D3DFMT_DXT3?GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;gl.CompressedTexImage2D(GL_TEXTURE_2D,l,f,x,y,0,GLsizei(mips[l].size()),mips[l].data());}else if(compressed()){auto rgba=gl_decode_dxt(mips[l].data(),x,y,format);glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA8,x,y,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());}else glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA8,x,y,0,GL_BGRA,GL_UNSIGNED_BYTE,mips[l].data());}
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,levels-1);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,levels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);mips.clear();mips.shrink_to_fit();}
};
// Material formulas ported from ARGN_Engine_CPP/src/material_gl_sources.hpp
// (pre-D3D OpenGL 026). See docs/GL_SHADER_PROVENANCE.md.
inline const char*gl_material_vertex(){return R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec4 argb;layout(location=3)in vec2 tex0;layout(location=4)in vec2 tex1;
uniform mat4 world,view,projection,uvMatrix;uniform vec2 viewport;uniform int isUI,materialMode,uvTransform;uniform vec4 c[96];uniform float time;
out vec4 color;out vec2 uv0,uv1;out vec3 worldPosition,worldNormal,eyeDirection;out float viewDepth;
vec3 safeNormal(vec3 v){return v*inversesqrt(max(dot(v,v),.000001));}
void main(){vec3 pos=position;vec4 tint=argb.bgra;
 vec3 treeRight=vec3(c[10].x,0.,c[10].z);
 if(materialMode==20){
  treeRight=dot(treeRight,treeRight)>1.e-8?normalize(treeRight):vec3(1.,0.,0.);
  pos=position-normal+normal.x*treeRight+vec3(0.,normal.y,0.);
 }
 if(materialMode==21){float halfWidth=tex1.x,top=tex1.y;vec3 wind=c[64+clamp(int(normal.z),0,15)].xyz;float edge=abs(halfWidth)*wind.z*step(.00001,top);pos.x+=normal.x*halfWidth+normal.y*top*wind.x+normal.x*edge;pos.z+=normal.y*halfWidth-normal.x*top*wind.x+normal.y*edge;pos.y+=top*wind.y;tint.a*=clamp((c[32].x-length(position-c[8].xyz))*c[32].y,0.,1.);}
 // Billboard corner offsets and plant direction/phase are encoded payloads,
 // not authored surface normals. Reconstruct the actual expanded leaf plane.
 vec3 receiverNormal=normal;
 if(materialMode==20)receiverNormal=cross(treeRight,vec3(0.,1.,0.));
 if(materialMode==21){vec3 wind=c[64+clamp(int(normal.z),0,15)].xyz;receiverNormal=cross(vec3(normal.x,0.,normal.y),vec3(normal.y*wind.x,wind.y,-normal.x*wind.x));}
 vec4 wp=world*vec4(pos,1.);worldPosition=wp.xyz;worldNormal=safeNormal(mat3(transpose(inverse(world)))*receiverNormal);vec4 vp=view*wp;viewDepth=vp.z;
 // View is rigid: transpose(rotation) transforms eye-space vectors to world.
 eyeDirection=transpose(mat3(view))*(-vp.xyz);
 if(isUI!=0)gl_Position=vec4(2.*(position.x+.5)/viewport.x-1.,1.-2.*(position.y+.5)/viewport.y,position.z*2.-1.,1.);else{vec4 clip=projection*vp;clip.z=2.*clip.z-clip.w;gl_Position=clip;}
 if(materialMode==9)tint*=c[32];
 if(materialMode==11){vec3 V=safeNormal(c[8].xyz-(position+c[9].xyz*c[32].x));vec3 H=safeNormal(V+c[20].xyz);float nh=max(0.,dot(normal,H));float spec=dot(normal,c[20].xyz)>=0.?pow(nh,8.):0.;tint=c[22]*spec;}
 color=tint;uv0=tex0;uv1=tex1;if(uvTransform!=0)uv0=(uvMatrix*vec4(tex0,1.,1.)).xy;
 if(materialMode==6)uv0+=c[32].xy;
 if(materialMode==7)uv0=position.xz*c[64].xy*c[94].xy+time*c[64].zw*c[94].zw;
})GLSL";}
inline const char*gl_material_fragment(){return R"GLSL(#version 330 core
in vec4 color;in vec2 uv0,uv1;in vec3 worldPosition,worldNormal,eyeDirection;in float viewDepth;out vec4 frag;
uniform sampler2D texA,texB,texNormal,texDirt;uniform int hasNormal,hasDirt,normalUVIndex;uniform samplerCube texEnvironment;
uniform int hasA,hasB,hasEnvironment,materialGlass,materialCracked,isUI,materialMode,lighting,fog,alphaTest,stage1,uvIndex0,uvIndex1;
uniform float environmentNight,environmentWet;uniform float environmentSun=1.;uniform int sceneSurface,skySurface,skyNativeNight,headlightCount,hasHeadlightCookie;
uniform mat4 headlightWorldToCar[8];uniform vec4 headlightLamps[8];uniform sampler2D headlightCookie;
uniform vec2 headlightHealth[8]=vec2[8](vec2(1),vec2(1),vec2(1),vec2(1),vec2(1),vec2(1),vec2(1),vec2(1));
uniform int headlightOccluderCount,hasHeadlightShadow;
uniform mat4 headlightOccluderPose[8],headlightShadowMatrix[2];
uniform vec4 headlightOccluderLo[8],headlightOccluderHi[8];uniform sampler2D headlightShadow;
// Slab intersection of the open lamp-to-receiver segment. Excluding the end
// avoids self-shadow on receiving body surfaces; mounted lamps start outside.
float headlightVehicleVisibility(vec3 origin,vec3 receiver,int own){
 for(int j=0;j<8;j++){if(j>=headlightOccluderCount)break;if(j==own)continue;
  vec3 a=(headlightOccluderPose[j]*vec4(origin,1)).xyz,b=(headlightOccluderPose[j]*vec4(receiver,1)).xyz;
  vec3 delta=b-a;vec3 safeDelta=mix(delta,vec3(.000001),lessThan(abs(delta),vec3(.000001)));
  vec3 t0=(headlightOccluderLo[j].xyz-a)/safeDelta,t1=(headlightOccluderHi[j].xyz-a)/safeDelta;
  vec3 lo=min(t0,t1),hi=max(t0,t1);float enter=max(max(lo.x,lo.y),max(lo.z,.002));float leave=min(min(hi.x,hi.y),min(hi.z,.985));
  if(enter<leave)return 0.;
 }return 1.;}
// Projected depth is affine over a planar receiver in shadow UV. Correct each
// tap to its own receiver depth instead of comparing all texels at one depth:
// at grazing angles one 256px texel spans far more than a fixed metre bias.
vec2 headlightDepthGradient(vec2 uv,float depth){
 vec2 dx=dFdx(uv),dy=dFdy(uv);float zx=dFdx(depth),zy=dFdy(depth);
 float det=dx.x*dy.y-dx.y*dy.x;
 return abs(det)>1.e-12?vec2(zx*dy.y-zy*dx.y,zy*dx.x-zx*dy.x)/det:vec2(0.);
}
float headlightDistance(float depth){return .12*150./max(150.-depth*(150.-.12),.12);}
// Allow a thin overlapping road layer (1.2 cm normal to the surface), capped
// in light space so genuine banks, barriers and other casters stay dark.
float headlightReceiverBias(float up,float incidence){
 return .065+smoothstep(.48,.85,abs(up))*min(.8,.012/max(abs(incidence),.01));
}
float headlightStaticVisibility(int side,float bias){
 if(hasHeadlightShadow==0)return 1.;vec4 clip=headlightShadowMatrix[side]*vec4(worldPosition,1.);
 vec3 q=clip.xyz/clip.w;vec2 uv=vec2((q.x*.5+.5)*.5+float(side)*.5,q.y*.5+.5);
 vec2 gradient=headlightDepthGradient(uv,q.z);
 if(clip.w<=0.||dot(q.xy,q.xy)>=1.||q.z<=0.||q.z>=1.)return 1.;
 vec2 size=vec2(textureSize(headlightShadow,0)),base=floor(uv*size-.5);float visible=0.;
 for(int y=0;y<2;y++)for(int x=0;x<2;x++){
  vec2 tap=(base+vec2(x,y)+.5)/size;
  tap=clamp(tap,vec2(float(side)*.5+.5/size.x,.5/size.y),vec2(float(side+1)*.5-.5/size.x,1.-.5/size.y));
  float stored=textureLod(headlightShadow,tap,0.).r;
  float receiver=clamp(q.z+dot(gradient,tap-uv),0.,1.);
  visible+=headlightDistance(receiver)-bias<=headlightDistance(stored)?.25:0.;
 }return visible;}


uniform ivec4 colorStage0,alphaStage0,colorStage1,alphaStage1;uniform vec4 factor,fogColor;uniform vec3 sun;uniform vec4 c[96];uniform float alphaRef,fogStart,fogEnd,time;
vec3 safeNormal(vec3 v){return v*inversesqrt(max(dot(v,v),.000001));}
vec4 argument(int a,vec4 t,vec4 current){if(a==2)return t;if(a==0)return color;if(a==3)return factor;return current;}
vec4 operation(ivec4 s,vec4 t,vec4 current){vec4 a=argument(s.y,t,current),b=argument(s.z,t,current);if(s.x==1)return current;if(s.x==2)return a;if(s.x==3)return b;if(s.x==4)return a*b;if(s.x==5)return a*b*2.;if(s.x==6)return a*b*4.;if(s.x==7)return a+b;if(s.x==10)return a+b-.5;return a*b;}
vec3 environment(vec3 direction){if(hasEnvironment!=0)return texture(texEnvironment,direction).rgb;return mix(fogColor.rgb*.48,fogColor.rgb,clamp(direction.y*.5+.5,0.,1.));}
// Derivative cotangent frame handles mirrored UVs and nonuniform world transforms.
vec3 mappedNormal(vec3 N,vec2 uv,vec3 sampleNormal){
 vec3 px=dFdx(worldPosition),py=dFdy(worldPosition);vec2 tx=dFdx(uv),ty=dFdy(uv);
 vec3 a=cross(py,N),b=cross(N,px);vec3 T=a*tx.x+b*ty.x,B=a*tx.y+b*ty.y;
 float scale=max(dot(T,T),dot(B,B));if(scale<1.e-12)return N;
 return safeNormal(mat3(T*inversesqrt(scale),B*inversesqrt(scale),N)*sampleNormal);
}
// Two physical lamps per car, projected in its rigid local frame. The cone is
// evaluated at the actual receiving fragment, with normal-facing attenuation.
float surfaceHighlight(vec3 N,vec3 L,vec3 V,float reflectance,float gloss,float power){
 return reflectance*mix(.24,1.,gloss)*pow(max(dot(N,safeNormal(L+V)),0.),power);
}
vec3 projectedHeadlights(vec3 N,vec3 detailN,vec3 V,vec3 albedo,float reflectance,bool trackPacked,float gloss,float power){vec3 sum=vec3(0.);
 for(int i=0;i<8;i++){if(i>=headlightCount)break;vec3 local=(headlightWorldToCar[i]*vec4(worldPosition,1.)).xyz;
  for(int side=0;side<2;side++){vec3 lamp=vec3((side==0?-1.:1.)*headlightLamps[i].x,headlightLamps[i].y,headlightLamps[i].z);vec3 q=local-lamp;
   vec3 beam=vec3(q.x,(q.y+.11*q.z)*.99398743,(q.z-.11*q.y)*.99398743);
   // Derivative-based shadow PCF must execute before per-fragment cone rejects.
   // Otherwise neighbouring lanes can skip dFdx/dFdy and create black stripes.
   vec3 L=safeNormal(transpose(mat3(headlightWorldToCar[i]))*(-q));
   float staticVisibility=i==0?headlightStaticVisibility(side,headlightReceiverBias(N.y,dot(N,L))):1.;
   if(beam.z<=.12||beam.z>=150.)continue;vec2 cone=beam.xy/(beam.z*vec2(.70,.38));
   float radius=dot(cone,cone);if(radius>=1.)continue;vec2 cookieUV=cone*.5+.5;
   float cookie=hasHeadlightCookie!=0?dot(texture(headlightCookie,cookieUV).rgb,vec3(.2126,.7152,.0722)):1.;
   float edge=1.-smoothstep(.55,1.,radius),range=smoothstep(.12,1.0,beam.z)*(1.-smoothstep(105.,150.,beam.z));
   float facing=(materialMode==20||materialMode==21||materialMode==22)?abs(dot(N,L)):max(max(dot(N,L),0.),smoothstep(.48,.85,N.y)*smoothstep(15.,55.,beam.z)*.30); // road grazing illumination: avoid fragment loss on horizontal track triangles
   vec3 origin=worldPosition-transpose(mat3(headlightWorldToCar[i]))*q;
   float visibility=headlightVehicleVisibility(origin,worldPosition,i)*staticVisibility;
   float attenuation=edge*range*cookie*visibility/(1.+.0008*dot(q,q));
   // Packed normals can change gloss, not geometric light coverage or occlusion.
   float spec=trackPacked?.65*surfaceHighlight(detailN,L,V,reflectance,gloss,power):
     pow(max(dot(N,safeNormal(L+V)),0.),mix(16.,60.,environmentWet))*environmentWet*.45*reflectance;
   sum+=vec3(1.,.91,.72)*attenuation*facing*(albedo*3.3+spec);
  }
 }return sum;}
void main(){vec4 a=hasA!=0?texture(texA,uvIndex0==1?uv1:uv0):vec4(1),b=hasB!=0?texture(texB,uvIndex1==1?uv1:uv0):vec4(1);vec4 result=operation(colorStage0,a,color);result.a=operation(alphaStage0,a,color).a;
 if(materialMode!=0){
  // Base native static formulas; shared receiver lighting follows below.
  result=a*color;
  if(materialMode==1||materialMode==9||materialMode==20)result.rgb=2.*a.rgb*color.rgb;
  else if(materialMode==2||materialMode==14)result=a*b;
  else if(materialMode==11){result=a*b;result.rgb+=color.rgb*result.a;}
  else if(materialMode==12)result=color;
  else if(materialMode==13)result=a;
  else if(materialMode==5)result.rgb=a.rgb*b.rgb;
  else if(materialMode==6)result=2.*a*color;
 }else{if(stage1!=0){vec4 r=operation(colorStage1,b,result);r.a=operation(alphaStage1,b,result).a;result=r;}if(lighting!=0)result.rgb*=.5+.65*max(0.,dot(safeNormal(worldNormal),-safeNormal(sun)));}
 if(isUI==0&&hasNormal==1&&(materialMode==1||materialMode==2)){vec3 N=safeNormal(worldNormal);if(!gl_FrontFacing)N=-N;vec3 M=mappedNormal(N,normalUVIndex==1?uv1:uv0,texture(texNormal,normalUVIndex==1?uv1:uv0).rgb*2.-1.);vec3 L=-safeNormal(sun);result.rgb*=clamp((.75+.25*max(dot(M,L),0.))/(.75+.25*max(dot(N,L),0.)),.85,1.15);}
 if(isUI==0&&(materialMode==3||materialMode==4||materialMode==7||materialMode==8||materialMode==16||materialMode==15||materialGlass!=0)){
  vec3 N=safeNormal(worldNormal),V=safeNormal(eyeDirection),L=-safeNormal(sun);if(!gl_FrontFacing)N=-N;
  // Native body has no normal-map input: paint/rust RGB is not height.
  if(hasNormal==1&&materialMode!=7&&materialMode!=3)N=mappedNormal(N,normalUVIndex==1?uv1:uv0,texture(texNormal,normalUVIndex==1?uv1:uv0).xyz*2.-1.);
  float nl=max(dot(N,L),0.);vec3 ambient=c[19].rgb+c[18].rgb*N.y;
  // Same hemisphere+sun and Blinn power-8 terms as the old GL source.
  vec3 diffuse=clamp(ambient+c[21].rgb*nl,0.,1.);
  float spec=pow(max(dot(N,safeNormal(L+V)),0.),8.)*step(.0001,nl);
  vec3 specular=vec3(.3)*spec;
  float nv=clamp(abs(dot(N,V)),0.,1.);float grazing=pow(1.-nv,5.);
  // Native pro_car_body passes vertex alpha as damage blend only; RGB is
  // replaced by hemisphere lighting, never multiplied into authored paint.
  if(materialMode==3){if(hasDirt!=0)a=mix(a,texture(texDirt,uv0),color.a);float fresnel=.25+.75*grazing;float mask=hasA!=0?a.a:0.;vec3 lit=clamp(a.rgb*diffuse+specular*mask,0.,1.);result=vec4(hasEnvironment!=0?mix(lit,environment(reflect(-V,N)),mask*fresnel):lit,1.);}
  else if(materialMode==8||materialMode==16)result=vec4(a.rgb*diffuse,a.a);
  if(materialMode==4||materialMode==15||materialGlass!=0){
   // Original clean window: reflectivity = Fresnel-dirt, opacity = Fresnel+dirt.
   // Tint remains authored RGB; damage uses authored fracture alpha, never a solid pane.
   bool cracked=materialCracked!=0||materialMode==15;
   float fresnel=.375+.625*grazing;float dirt=hasA!=0?a.a:0.;float reflectivity=max(fresnel-dirt,0.);
   vec3 lit=a.rgb*diffuse;vec3 reflected=environment(reflect(-V,N));
   result.rgb=mix(lit,reflected,reflectivity)+specular*reflectivity;
   result.a=clamp(fresnel+dirt,0.,1.);
   if(cracked){result.rgb=mix(lit,reflected,.12*fresnel)+specular*.12;result.a=a.a*color.a;}
  }
  if(materialMode==7){
   // Old GL two scrolling normal-map samples and N.V reflection blend.
   vec2 flow=worldPosition.xz*.035;
   vec3 na=hasNormal!=0?texture(texNormal,flow+vec2(.013,.009)*time).rgb:vec3(.5,.5,1.);
   vec3 nb=hasNormal!=0?texture(texNormal,flow*1.73+vec2(-.009,.013)*time).rgb:vec3(.5,.5,1.);
   vec3 blended=safeNormal((na+nb)-1.);blended.xy*=.28;vec3 waterN=mappedNormal(N,flow,safeNormal(blended));
   float waterNV=clamp(abs(dot(waterN,V)),0.,1.);vec3 reflected=hasEnvironment!=0?environment(reflect(-V,waterN)):a.rgb;
   vec3 tint=mix(vec3(.065,.14,.16),a.rgb*color.rgb,.22);result=vec4(mix(reflected,tint,.35+.5*waterNV),clamp(.82+.12*grazing,.82,.94));
  }
 }
 if(isUI==0&&skySurface!=0){if(skyNativeNight==0)result.rgb*=mix(vec3(1.),vec3(.055,.085,.15),environmentNight);result.rgb*=mix(1.,.55,environmentWet);}
 if(isUI==0&&sceneSurface!=0){
  // Native normals describe the physical receiver side. Camera raster winding
  // is not a light-facing test (LH geometry is drawn without face culling).
  // Flipping here made the far side bright and changed light as the camera moved.
  vec3 N=safeNormal(worldNormal);
  // Native body has no normal-map input: paint/rust RGB is not height.
  if(hasNormal==1&&materialMode!=7&&materialMode!=3)N=mappedNormal(N,normalUVIndex==1?uv1:uv0,texture(texNormal,normalUVIndex==1?uv1:uv0).rgb*2.-1.);
  vec3 V=safeNormal(eyeDirection),L=-safeNormal(sun),detailN=N;
  bool trackPacked=hasNormal==2&&(materialMode==0||materialMode==1||materialMode==2||materialMode==8||materialMode==10||materialMode==11);
  float reflectance=materialMode==3?a.a:(materialGlass!=0?.16:(materialMode==7?.8:.12)),gloss=0.,power=8.;
  if(trackPacked){
   vec2 uv=normalUVIndex==1?uv1:uv0;
   vec4 surface=texture(texNormal,uv);vec2 xy=surface.rg*2.-1.;
   detailN=mappedNormal(N,uv,vec3(xy,sqrt(max(.0001,1.-dot(xy,xy)))));
   reflectance=surface.b;
   float roughness=clamp(surface.a,.08,1.);
   gloss=1.-mix(roughness,max(.12,roughness*.55),environmentWet);power=mix(6.,112.,gloss*gloss);
  }
  bool emissive=materialMode==13||materialMode==5;
  if(!emissive){float daylight=max(dot(N,L),0.);result.rgb*=mix((.55+.45*daylight*environmentSun)/(.55+.45*daylight),1.,environmentNight);// Moon and sky fill modulate native prelight/lightmaps instead of replacing them.
   if(trackPacked){
    float bump=clamp((.72+.28*max(dot(detailN,L),0.))/(.72+.28*daylight),.87,1.13);
    result.rgb*=bump;
    float prelit=clamp(max(result.r,max(result.g,result.b))*2.,0.,1.);
    result.rgb+=vec3(1.,.93,.83)*(.22*surfaceHighlight(detailN,L,V,reflectance,gloss,power)*daylight*prelit*environmentSun*(1.-environmentNight));
   }
   float moon=max(dot(N,-safeNormal(sun)),0.);
   float hemisphere=.78+.14*max(N.y,0.)+.18*moon;
   result.rgb*=mix(vec3(1.),vec3(.205,.265,.36)*hemisphere,environmentNight);
   result.rgb*=mix(1.,mix(.72,.88,environmentNight),environmentWet);
   if(environmentWet>.001&&materialGlass==0&&materialMode!=7){float grazing=pow(1.-abs(dot(N,V)),5.);float fresnel=mix(.025+.25*grazing,.02+.14*grazing,environmentNight);
    vec3 reflected=environment(reflect(-V,detailN));reflected=mix(reflected,min(reflected,vec3(.28)),environmentNight);
    result.rgb=mix(result.rgb,reflected,environmentWet*fresnel*(trackPacked?mix(.4,1.,gloss):1.));}
   if(environmentNight>.001)result.rgb+=projectedHeadlights(N,detailN,V,a.rgb,reflectance,trackPacked,gloss,power)*environmentNight;
  }
  if(materialMode==5)result.a*=1.-environmentNight;
 }
 // PRO car lights P1: Tex2 is additive and is not attenuated by scene lighting.
 if(isUI==0&&materialMode==16&&hasDirt!=0)result.rgb+=texture(texDirt,uv0).rgb;
 if(alphaTest!=0&&result.a<=alphaRef)discard;if(fog!=0&&isUI==0)result.rgb=mix(result.rgb,fogColor.rgb,clamp((viewDepth-fogStart)/max(.01,fogEnd-fogStart),0.,1.));frag=clamp(result,0.,1.);
})GLSL";}
struct GLDevice:GLRef {
 // Context outlives every GL resource owned by the renderer.
 GLWindow window;GLuint program=0,defaultVao=0,samplerObjects[2]{};std::array<std::map<GLenum,GLint>,2>samplerValues;std::array<DWORD,2>samplerBias{{0xffffffff,0xffffffff}};std::string adapter;bool s3tc=false,anisotropy_supported=false;unsigned anisotropy_max=1;std::array<unsigned,2>samplerAnisotropy{{0,0}};int width=1280,height=720;DWORD fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX2;int material_mode=0;bool material_glass=false,material_cracked=false,packed_normal=false;GLTexture*normal_texture=nullptr,*dirt_texture=nullptr;int normal_uv=0;GLuint environment_cube=0;std::array<GLuint,6> environment_faces{};float time=0;Vec sun{-.6f,-1,-.2f};
 float environment_night=0,environment_wet=0,environment_sun=1;bool scene_surface=false,sky_surface=false,sky_native_night=false;int headlight_count=0;GLTexture*headlight_cookie=nullptr;std::array<Mat,8>headlight_world_to_car{};std::array<std::array<float,4>,8>headlight_lamps{};
 std::array<std::array<float,2>,8>headlight_health{{{{1,1}},{{1,1}},{{1,1}},{{1,1}},{{1,1}},{{1,1}},{{1,1}},{{1,1}}}};
 int headlight_occluder_count=0;std::array<Mat,8>headlight_occluder_pose{};std::array<std::array<float,4>,8>headlight_occluder_lo{},headlight_occluder_hi{};GLuint headlight_shadow=0;std::array<Mat,2>headlight_shadow_matrix{};
 std::map<std::string,GLint>uniform_locations;std::map<GLint,std::vector<unsigned char>>uniform_values;size_t uniform_writes=0,uniform_skips=0;
 std::map<DWORD,DWORD>rs;std::array<std::map<DWORD,DWORD>,2>stages,samplers;std::array<GLTexture*,2>textures{};std::array<float,384>constants{};Mat world=identity(),view=identity(),projection=identity(),uv=identity();GLVertexBuffer*vb=nullptr;GLIndexBuffer*ib=nullptr;UINT offset=0,stride=44;D3DVIEWPORT9 viewport{0,0,1280,720,0,1};
 explicit GLDevice(HWND h):window(h){const auto*vendor=glGetString(GL_VENDOR);const auto*renderer=glGetString(GL_RENDERER);adapter=std::string(vendor?reinterpret_cast<const char*>(vendor):"unknown")+" / "+(renderer?reinterpret_cast<const char*>(renderer):"unknown");program=gl_program(gl_material_vertex(),gl_material_fragment(),"OpenFO material/state adapter");gl.GenVertexArrays(1,&defaultVao);gl.BindVertexArray(defaultVao);gl.GenSamplers(2,samplerObjects);GLint n=0;glGetIntegerv(GL_NUM_EXTENSIONS,&n);for(int i=0;i<n;i++){auto s=reinterpret_cast<const char*>(gl.GetStringi(GL_EXTENSIONS,i));if(s&&(std::strstr(s,"texture_compression_s3tc")))s3tc=true;if(s&&(std::strcmp(s,"GL_EXT_texture_filter_anisotropic")==0||std::strcmp(s,"GL_ARB_texture_filter_anisotropic")==0))anisotropy_supported=true;}if(anisotropy_supported){GLfloat maximum=1;glGetFloatv(0x84FF,&maximum);if(std::isfinite(maximum)&&maximum>=1)anisotropy_max=unsigned(std::min(maximum,65535.f));}rs[D3DRS_ZENABLE]=TRUE;rs[D3DRS_ZWRITEENABLE]=TRUE;rs[D3DRS_TEXTUREFACTOR]=0xffffffff;rs[D3DRS_FOGCOLOR]=0xff90a4ae;float a=180,b=650;std::memcpy(&rs[D3DRS_FOGSTART],&a,4);std::memcpy(&rs[D3DRS_FOGEND],&b,4);for(auto&s:stages){s[D3DTSS_COLOROP]=D3DTOP_MODULATE;s[D3DTSS_COLORARG1]=D3DTA_TEXTURE;s[D3DTSS_COLORARG2]=D3DTA_DIFFUSE;s[D3DTSS_ALPHAOP]=D3DTOP_MODULATE;s[D3DTSS_ALPHAARG1]=D3DTA_TEXTURE;s[D3DTSS_ALPHAARG2]=D3DTA_DIFFUSE;}stages[1][D3DTSS_COLOROP]=D3DTOP_DISABLE;}
 ~GLDevice(){if(environment_cube)glDeleteTextures(1,&environment_cube);gl.DeleteSamplers(2,samplerObjects);if(program)gl.DeleteProgram(program);if(defaultVao)gl.DeleteVertexArrays(1,&defaultVao);}
 // Reuse the 026 static-sky reflection approach: bounded 64px cube, built only
 // when sky texture IDs change. Readback is loading-time only, never per draw.
 void set_environment_faces(const std::array<GLTexture*,6>&faces){
  std::array<GLuint,6> ids{};for(size_t i=0;i<6;i++)ids[i]=faces[i]?faces[i]->id:0;
  if(ids==environment_faces)return;environment_faces=ids;
  if(environment_cube){glDeleteTextures(1,&environment_cube);environment_cube=0;}
  if(std::all_of(ids.begin(),ids.end(),[](GLuint id){return id==0;}))return;
  struct Image{int w=0,h=0;std::vector<unsigned char>rgba;};std::array<Image,6> images;
  GLint active=0,old2d=0,oldCube=0,pack=4;glGetIntegerv(0x84E0,&active);gl.ActiveTexture(GL_TEXTURE0+2);glGetIntegerv(GL_TEXTURE_BINDING_2D,&old2d);glGetIntegerv(0x8514,&oldCube);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glPixelStorei(GL_PACK_ALIGNMENT,1);
  for(size_t i=0;i<6;i++)if(faces[i]){auto&im=images[i];im.w=faces[i]->w;im.h=faces[i]->h;im.rgba.resize(size_t(im.w)*im.h*4);glBindTexture(GL_TEXTURE_2D,ids[i]);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,im.rgba.data());}
  glGenTextures(1,&environment_cube);glBindTexture(0x8513,environment_cube);constexpr int size=64;std::array<unsigned char,size*size*4> pixels{};
  for(int face=0;face<6;face++){for(int y=0;y<size;y++)for(int x=0;x<size;x++){
   float u=2*(x+.5f)/size-1,v=2*(y+.5f)/size-1;std::array<float,3>d{};
   switch(face){case 0:d={1,-v,-u};break;case 1:d={-1,-v,u};break;case 2:d={u,1,v};break;case 3:d={u,-1,-v};break;case 4:d={u,-v,1};break;default:d={-u,-v,-1};}
   float a=std::abs(d[0]),b=std::abs(d[1]),c=std::abs(d[2]),s=0,t=0;int source=-1;
   if(b>=a&&b>=c){source=d[1]>0?4:5;s=(d[0]/b+1)*.5f;t=(d[1]>0?d[2]/b:-d[2]/b)*.5f+.5f;}
   else if(a>=c){source=d[0]>0?3:2;s=(d[0]>0?-d[2]/a:d[2]/a)*.5f+.5f;t=(1-d[1]/a)*(sky_native_night?.5f:1.f);}
   else{source=d[2]>0?0:1;s=(d[2]>0?d[0]/c:-d[0]/c)*.5f+.5f;t=(1-d[1]/c)*(sky_native_night?.5f:1.f);}
   size_t to=(size_t(y)*size+x)*4;pixels[to]=144;pixels[to+1]=164;pixels[to+2]=174;pixels[to+3]=255;
   if(source>=0){auto&im=images[source];if(im.w&&im.h){int ix=std::clamp(int(s*im.w),0,im.w-1),iy=std::clamp(int(t*im.h),0,im.h-1);std::copy_n(im.rgba.data()+(size_t(iy)*im.w+ix)*4,4,pixels.data()+to);}}
  }glTexImage2D(0x8515+face,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());}
  glTexParameteri(0x8513,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(0x8513,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(0x8513,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(0x8513,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(0x8513,0x8072,GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D,old2d);glBindTexture(0x8513,oldCube);glPixelStorei(GL_PACK_ALIGNMENT,pack);gl.ActiveTexture(active);
 }
 HRESULT CreateVertexBuffer(UINT n,DWORD usage,DWORD,D3DPOOL,GLVertexBuffer**out,HANDLE*){try{gl.BindVertexArray(defaultVao);*out=new GLVertexBuffer(n,(usage&D3DUSAGE_DYNAMIC)!=0);return S_OK;}catch(...){return E_OUTOFMEMORY;}}
 HRESULT CreateIndexBuffer(UINT n,DWORD,D3DFORMAT format,D3DPOOL,GLIndexBuffer**out,HANDLE*){try{gl.BindVertexArray(defaultVao);*out=new GLIndexBuffer(n,format);return S_OK;}catch(...){return E_OUTOFMEMORY;}}
 HRESULT CreateTexture(UINT w,UINT h,UINT levels,DWORD,D3DFORMAT format,D3DPOOL pool,GLTexture**out,HANDLE*){try{*out=new GLTexture(w,h,std::max(1u,levels),format,pool==D3DPOOL_SYSTEMMEM);return S_OK;}catch(...){return E_OUTOFMEMORY;}}
 HRESULT UpdateTexture(GLTexture*source,GLTexture*dest){if(!source||!dest)return E_INVALIDARG;dest->mips=source->mips;dest->upload(s3tc);return S_OK;}
 HRESULT SetMaximumFrameLatency(UINT){return S_OK;}HRESULT CreateQuery(D3DQUERYTYPE,GLQuery**out){*out=nullptr;return E_NOTIMPL;}
 HRESULT SetRenderState(D3DRENDERSTATETYPE t,DWORD v){rs[t]=v;return S_OK;}HRESULT GetRenderState(D3DRENDERSTATETYPE t,DWORD*v){if(!v)return E_INVALIDARG;*v=rs[t];return S_OK;}
 HRESULT SetTextureStageState(DWORD slot,D3DTEXTURESTAGESTATETYPE t,DWORD v){if(slot<2)stages[slot][t]=v;return S_OK;}HRESULT SetSamplerState(DWORD slot,D3DSAMPLERSTATETYPE t,DWORD v){if(slot<2)samplers[slot][t]=v;return S_OK;}
 HRESULT SetTexture(DWORD slot,GLTexture*t){if(slot<2)textures[slot]=t;return S_OK;}
 HRESULT SetTransform(D3DTRANSFORMSTATETYPE type,const D3DMATRIX*m){if(!m)return E_INVALIDARG;Mat*dst=type==D3DTS_WORLD?&world:type==D3DTS_VIEW?&view:type==D3DTS_PROJECTION?&projection:&uv;std::memcpy(dst->data(),m,64);return S_OK;}
 HRESULT SetFVF(DWORD f){fvf=f;return S_OK;}HRESULT SetVertexDeclaration(GLDeclaration*){fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX2;return S_OK;}
 HRESULT SetVertexShader(GLShader*p){if(!p)material_mode=0;return S_OK;}HRESULT SetPixelShader(GLShader*){return S_OK;}
 HRESULT SetVertexShaderConstantF(UINT start,const float*data,UINT count){if(start+count>96)return E_INVALIDARG;std::memcpy(constants.data()+start*4,data,count*16);return S_OK;}
 HRESULT SetStreamSource(UINT,GLVertexBuffer*b,UINT off,UINT s){vb=b;offset=off;stride=s;return S_OK;}HRESULT SetIndices(GLIndexBuffer*b){ib=b;return S_OK;}
 HRESULT SetLight(DWORD,const D3DLIGHT9*l){if(l)sun={l->Direction.x,l->Direction.y,l->Direction.z};return S_OK;}HRESULT LightEnable(DWORD,BOOL){return S_OK;}HRESULT SetMaterial(const D3DMATERIAL9*){return S_OK;}
 HRESULT GetDeviceCaps(D3DCAPS9*c){if(!c)return E_INVALIDARG;*c={};GLint max=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max);c->MaxTextureWidth=c->MaxTextureHeight=max;c->MaxAnisotropy=anisotropy_max;if(anisotropy_supported&&anisotropy_max>1)c->TextureFilterCaps=D3DPTFILTERCAPS_MINFANISOTROPIC|D3DPTFILTERCAPS_MAGFANISOTROPIC;c->VertexShaderVersion=D3DVS_VERSION(3,0);c->PixelShaderVersion=D3DPS_VERSION(3,0);return S_OK;}
 HRESULT GetViewport(D3DVIEWPORT9*v){if(!v)return E_INVALIDARG;*v=viewport;return S_OK;}HRESULT SetViewport(const D3DVIEWPORT9*v){if(!v)return E_INVALIDARG;viewport=*v;glViewport(v->X,height-v->Y-v->Height,v->Width,v->Height);glDepthRange(v->MinZ,v->MaxZ);return S_OK;}
 HRESULT BeginScene(){return S_OK;}HRESULT EndScene(){return S_OK;}
 HRESULT Clear(DWORD rectCount,const D3DRECT*rects,DWORD mask,D3DCOLOR color,float depth,DWORD stencil){glClearColor(((color>>16)&255)/255.f,((color>>8)&255)/255.f,(color&255)/255.f,((color>>24)&255)/255.f);glClearDepth(depth);glClearStencil(stencil);glDepthMask(GL_TRUE);GLbitfield bits=((mask&D3DCLEAR_TARGET)?GL_COLOR_BUFFER_BIT:0)|((mask&D3DCLEAR_ZBUFFER)?GL_DEPTH_BUFFER_BIT:0)|((mask&D3DCLEAR_STENCIL)?GL_STENCIL_BUFFER_BIT:0);GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);GLint box[4];glGetIntegerv(GL_SCISSOR_BOX,box);if(rectCount&&rects){glEnable(GL_SCISSOR_TEST);for(DWORD i=0;i<rectCount;i++){auto&r=rects[i];glScissor(r.x1,height-r.y2,std::max(0L,r.x2-r.x1),std::max(0L,r.y2-r.y1));glClear(bits);}}else{glDisable(GL_SCISSOR_TEST);glClear(bits);}glScissor(box[0],box[1],box[2],box[3]);if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);return S_OK;}
 HRESULT PresentEx(const RECT*,const RECT*,HWND,const RGNDATA*,DWORD){return window.present()?S_OK:E_FAIL;}
 HRESULT ResetEx(D3DPRESENT_PARAMETERS*p,D3DDISPLAYMODEEX*){width=p->BackBufferWidth;height=p->BackBufferHeight;window.resize(width,height);viewport={0,0,UINT(width),UINT(height),0,1};glViewport(0,0,width,height);return S_OK;}
 GLint uniform(const char*name){auto i=uniform_locations.find(name);if(i!=uniform_locations.end())return i->second;auto l=gl.GetUniformLocation(program,name);uniform_locations.emplace(name,l);return l;}
 template<class F>void uniform_value(const char*n,const void*data,size_t size,F submit){auto l=uniform(n);if(l<0)return;auto&previous=uniform_values[l];if(previous.size()==size&&!std::memcmp(previous.data(),data,size)){++uniform_skips;return;}submit(l);previous.assign(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+size);++uniform_writes;}
 void one(const char*n,int v){uniform_value(n,&v,sizeof(v),[&](GLint l){gl.Uniform1i(l,v);});}void real(const char*n,float v){uniform_value(n,&v,sizeof(v),[&](GLint l){gl.Uniform1f(l,v);});}void matrix(const char*n,const Mat&m){uniform_value(n,m.data(),sizeof(float)*16,[&](GLint l){gl.UniformMatrix4fv(l,1,GL_FALSE,m.data());});}
 void vector2(const char*n,float a,float b){float v[]={a,b};uniform_value(n,v,sizeof(v),[&](GLint l){gl.Uniform2f(l,a,b);});}void vector3(const char*n,float a,float b,float c){float v[]={a,b,c};uniform_value(n,v,sizeof(v),[&](GLint l){gl.Uniform3f(l,a,b,c);});}void vector4(const char*n,const float*v,int count=1){uniform_value(n,v,count*16,[&](GLint l){gl.Uniform4fv(l,count,v);});}void int4(const char*n,int a,int b,int c,int d){int v[]={a,b,c,d};uniform_value(n,v,sizeof(v),[&](GLint l){gl.Uniform4i(l,a,b,c,d);});}
 static void unpack(DWORD n,float*out){out[0]=((n>>16)&255)/255.f;out[1]=((n>>8)&255)/255.f;out[2]=(n&255)/255.f;out[3]=(n>>24)/255.f;}
 void attributes(){if(!vb)return;if(!vb->vao)gl.GenVertexArrays(1,&vb->vao);gl.BindVertexArray(vb->vao);bool ui=(fvf&D3DFVF_XYZRHW)==D3DFVF_XYZRHW;if(vb->attrStride!=stride||vb->attrOffset!=offset||vb->attrUI!=int(ui)){gl.BindBuffer(GL_ARRAY_BUFFER,vb->id);for(int i=0;i<5;i++)gl.EnableVertexAttribArray(i);auto ptr=[&](size_t o){return reinterpret_cast<const void*>(size_t(offset)+o);};gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,ptr(0));if(ui){gl.DisableVertexAttribArray(1);gl.VertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,stride,ptr(16));gl.VertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,stride,ptr(20));gl.DisableVertexAttribArray(4);}else{gl.VertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,ptr(12));gl.VertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,stride,ptr(24));gl.VertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,stride,ptr(28));gl.VertexAttribPointer(4,2,GL_FLOAT,GL_FALSE,stride,ptr(36));}vb->attrStride=stride;vb->attrOffset=offset;vb->attrUI=int(ui);}GLuint index=ib?ib->id:0;if(vb->attrIndex!=index){gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,index);vb->attrIndex=index;}}

 void apply(){
  gl.UseProgram(program);one("isUI",(fvf&D3DFVF_XYZRHW)==D3DFVF_XYZRHW);vector2("viewport",float(width),float(height));matrix("world",world);matrix("view",view);matrix("projection",projection);matrix("uvMatrix",uv);one("uvTransform",stages[0][D3DTSS_TEXTURETRANSFORMFLAGS]!=0);one("materialMode",material_mode);real("time",time);vector3("sun",sun.x,sun.y,sun.z);vector4("c[0]",constants.data(),96);
  real("environmentSun",environment_sun);real("environmentNight",environment_night);real("environmentWet",environment_wet);one("sceneSurface",scene_surface);one("skySurface",sky_surface);one("skyNativeNight",sky_native_night);one("headlightCount",headlight_count);one("hasHeadlightCookie",headlight_cookie!=nullptr);one("headlightCookie",5);gl.ActiveTexture(GL_TEXTURE0+5);glBindTexture(GL_TEXTURE_2D,headlight_cookie?headlight_cookie->id:0);gl.BindSampler(5,samplerObjects[0]);
  for(int i=0;i<headlight_count&&i<8;i++){auto suffix="["+std::to_string(i)+"]";matrix(("headlightWorldToCar"+suffix).c_str(),headlight_world_to_car[i]);vector4(("headlightLamps"+suffix).c_str(),headlight_lamps[i].data());vector2(("headlightHealth"+suffix).c_str(),headlight_health[i][0],headlight_health[i][1]);}
  one("headlightOccluderCount",headlight_occluder_count);for(int i=0;i<headlight_occluder_count&&i<8;i++){auto suffix="["+std::to_string(i)+"]";matrix(("headlightOccluderPose"+suffix).c_str(),headlight_occluder_pose[i]);vector4(("headlightOccluderLo"+suffix).c_str(),headlight_occluder_lo[i].data());vector4(("headlightOccluderHi"+suffix).c_str(),headlight_occluder_hi[i].data());}
  one("hasHeadlightShadow",headlight_shadow!=0);one("headlightShadow",6);gl.ActiveTexture(GL_TEXTURE0+6);glBindTexture(GL_TEXTURE_2D,headlight_shadow);gl.BindSampler(6,0);for(int i=0;i<2;i++)matrix(("headlightShadowMatrix["+std::to_string(i)+"]").c_str(),headlight_shadow_matrix[i]);
  one("normalUVIndex",normal_uv);one("texDirt",4);one("hasDirt",dirt_texture!=nullptr);gl.ActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D,dirt_texture?dirt_texture->id:0);gl.BindSampler(4,samplerObjects[0]);one("texNormal",3);one("hasNormal",normal_texture?(packed_normal?2:1):0);gl.ActiveTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D,normal_texture?normal_texture->id:0);gl.BindSampler(3,samplerObjects[0]);
  one("materialGlass",material_glass);one("materialCracked",material_cracked);one("hasEnvironment",environment_cube!=0);one("texEnvironment",2);gl.ActiveTexture(GL_TEXTURE0+2);glBindTexture(0x8513,environment_cube);gl.BindSampler(2,0);one("texA",0);one("texB",1);for(int i=0;i<2;i++){gl.ActiveTexture(GL_TEXTURE0+i);auto*t=textures[i];glBindTexture(GL_TEXTURE_2D,t?t->id:0);gl.BindSampler(i,samplerObjects[i]);if(t){const auto&s=samplers[i];auto get=[&](DWORD key,DWORD fallback){auto it=s.find(key);return it==s.end()?fallback:it->second;};auto parameter=[&](GLenum key,GLint value){auto&cache=samplerValues[i];auto old=cache.find(key);if(old==cache.end()||old->second!=value){gl.SamplerParameteri(samplerObjects[i],key,value);cache[key]=value;}};bool point=get(D3DSAMP_MINFILTER,D3DTEXF_LINEAR)==D3DTEXF_POINT;bool mip=get(D3DSAMP_MIPFILTER,D3DTEXF_NONE)!=D3DTEXF_NONE&&t->levels>1;parameter(GL_TEXTURE_MIN_FILTER,mip?(point?GL_NEAREST_MIPMAP_NEAREST:GL_LINEAR_MIPMAP_LINEAR):(point?GL_NEAREST:GL_LINEAR));parameter(GL_TEXTURE_MAG_FILTER,get(D3DSAMP_MAGFILTER,D3DTEXF_LINEAR)==D3DTEXF_POINT?GL_NEAREST:GL_LINEAR);parameter(GL_TEXTURE_WRAP_S,get(D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP)==D3DTADDRESS_CLAMP?GL_CLAMP_TO_EDGE:GL_REPEAT);parameter(GL_TEXTURE_WRAP_T,get(D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP)==D3DTADDRESS_CLAMP?GL_CLAMP_TO_EDGE:GL_REPEAT);if(anisotropy_supported){unsigned requested=get(D3DSAMP_MINFILTER,D3DTEXF_LINEAR)==D3DTEXF_ANISOTROPIC?std::clamp(unsigned(get(D3DSAMP_MAXANISOTROPY,1)),1u,anisotropy_max):1u;if(requested!=samplerAnisotropy[i]){gl.SamplerParameterf(samplerObjects[i],0x84FE,float(requested));samplerAnisotropy[i]=requested;}}DWORD bits=get(D3DSAMP_MIPMAPLODBIAS,0);if(bits!=samplerBias[i]){float bias;std::memcpy(&bias,&bits,4);gl.SamplerParameterf(samplerObjects[i],GL_TEXTURE_LOD_BIAS,bias);samplerBias[i]=bits;}}}

  one("hasA",textures[0]!=nullptr);one("hasB",textures[1]!=nullptr);one("stage1",stages[1][D3DTSS_COLOROP]!=D3DTOP_DISABLE);one("uvIndex0",stages[0][D3DTSS_TEXCOORDINDEX]);one("uvIndex1",stages[1][D3DTSS_TEXCOORDINDEX]);
  for(int i=0;i<2;i++){auto&s=stages[i];const char*cn=i?"colorStage1":"colorStage0",*an=i?"alphaStage1":"alphaStage0";int4(cn,s[D3DTSS_COLOROP],s[D3DTSS_COLORARG1]&15,s[D3DTSS_COLORARG2]&15,0);int4(an,s[D3DTSS_ALPHAOP],s[D3DTSS_ALPHAARG1]&15,s[D3DTSS_ALPHAARG2]&15,0);}
  float colors[4];unpack(rs[D3DRS_TEXTUREFACTOR],colors);vector4("factor",colors);unpack(rs[D3DRS_FOGCOLOR],colors);vector4("fogColor",colors);float fs,fe;auto a=rs[D3DRS_FOGSTART],b=rs[D3DRS_FOGEND];std::memcpy(&fs,&a,4);std::memcpy(&fe,&b,4);real("fogStart",fs);real("fogEnd",fe);one("lighting",rs[D3DRS_LIGHTING]);one("fog",rs[D3DRS_FOGENABLE]);one("alphaTest",rs[D3DRS_ALPHATESTENABLE]);real("alphaRef",rs[D3DRS_ALPHAREF]/255.f);
  if(rs[D3DRS_ZENABLE])glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(rs[D3DRS_ZWRITEENABLE]?GL_TRUE:GL_FALSE);if(rs[D3DRS_ALPHABLENDENABLE])glEnable(GL_BLEND);else glDisable(GL_BLEND);auto blend=[](DWORD b){return b==D3DBLEND_ONE?GL_ONE:b==D3DBLEND_ZERO?GL_ZERO:b==D3DBLEND_SRCALPHA?GL_SRC_ALPHA:b==D3DBLEND_INVSRCALPHA?GL_ONE_MINUS_SRC_ALPHA:GL_ONE;};glBlendFunc(blend(rs[D3DRS_SRCBLEND]),blend(rs[D3DRS_DESTBLEND]));const auto cull=rs[D3DRS_CULLMODE];
  // Both adapters use the same left-handed clip transform. Its front-facing
  // vehicle triangles are clockwise in GL window coordinates.
  glFrontFace(cull==D3DCULL_CCW?GL_CW:GL_CCW);
  if(cull==D3DCULL_CW||cull==D3DCULL_CCW){
   glEnable(GL_CULL_FACE);
   glCullFace(GL_BACK);
  }else glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK,rs[D3DRS_FILLMODE]==D3DFILL_WIREFRAME?GL_LINE:GL_FILL);attributes();
 }
 static GLenum primitive(D3DPRIMITIVETYPE t){return t==D3DPT_TRIANGLESTRIP?GL_TRIANGLE_STRIP:t==D3DPT_TRIANGLEFAN?GL_TRIANGLE_FAN:t==D3DPT_LINELIST?GL_LINES:GL_TRIANGLES;}static UINT count(D3DPRIMITIVETYPE t,UINT n){return t==D3DPT_TRIANGLELIST?n*3:t==D3DPT_LINELIST?n*2:n+2;}
 HRESULT DrawPrimitive(D3DPRIMITIVETYPE t,UINT first,UINT n){if(!vb)return E_FAIL;apply();glDrawArrays(primitive(t),first,count(t,n));return S_OK;}
 HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE t,INT base,UINT,UINT,UINT start,UINT n){if(!vb||!ib)return E_FAIL;apply();gl.DrawElementsBaseVertex(primitive(t),count(t,n),ib->type(),reinterpret_cast<const void*>(size_t(start)*(ib->format==D3DFMT_INDEX16?2:4)),base);return S_OK;}
};
struct GLBackendInfo:GLRef{};
}
