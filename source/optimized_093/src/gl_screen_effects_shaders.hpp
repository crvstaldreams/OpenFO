#pragma once
namespace ofo {
// Screen textures use OpenGL bottom-left UV. Mesh UVs retain their native convention.
inline const char* gl_effects_fullscreen_vs(){return R"GLSL(#version 330 core
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0.,1.);}
)GLSL";}
inline const char* gl_effects_ao_fs(){return R"GLSL(#version 330 core
in vec2 uv;out vec4 color;uniform sampler2D sceneDepth;
uniform vec4 projection;uniform vec4 controls;
float viewZ(float d){return projection.w/min(d-projection.z,-.0000001);}
vec3 reconstruct(vec2 u,float d){float z=viewZ(d);return vec3((u*2.-1.)*z/projection.xy,z);}
bool inside(vec2 u){return all(greaterThan(u,vec2(.001)))&&all(lessThan(u,vec2(.999)));}
// Point depth belongs to the center of its texel.
vec2 depthUV(vec2 q){vec2 size=vec2(textureSize(sceneDepth,0));return (clamp(floor(q*size),vec2(0.),size-1.)+.5)/size;}
void main(){vec2 center=depthUV(uv);float raw=textureLod(sceneDepth,center,0.).r;vec3 p=reconstruct(center,raw);
// Reconstruct normals at source resolution. Coarse derivatives straddle contacts
// in the reduced target and erase the very corners SSAO needs to detect.
vec2 t=1./vec2(textureSize(sceneDepth,0));
vec3 l=p-reconstruct(center-vec2(t.x,0.),textureLod(sceneDepth,center-vec2(t.x,0.),0.).r);
vec3 r=reconstruct(center+vec2(t.x,0.),textureLod(sceneDepth,center+vec2(t.x,0.),0.).r)-p;
vec3 b=p-reconstruct(center-vec2(0.,t.y),textureLod(sceneDepth,center-vec2(0.,t.y),0.).r);
vec3 u=reconstruct(center+vec2(0.,t.y),textureLod(sceneDepth,center+vec2(0.,t.y),0.).r)-p;
vec3 n=cross(abs(l.z)<abs(r.z)?l:r,abs(b.z)<abs(u.z)?b:u);
n*=inversesqrt(max(dot(n,n),1e-12));if(dot(n,p)>0.)n=-n;
if(raw>=.99999||controls.w<.5){color=vec4(1.);return;}
float sum=0.;float count=max(controls.z,1.);
for(int i=0;i<8;++i){if(float(i)>=controls.z)break;float a=(float(i)+.5)*6.2831853/count;
vec2 q=center+vec2(cos(a),sin(a))*controls.x*projection.xy*.5/max(p.z,.01)*(.25+.4*float(i%2));
vec2 tap=depthUV(q);float d=textureLod(sceneDepth,tap,0.).r;vec3 v=reconstruct(tap,d)-p;float l=length(v);
if(inside(q)&&d<.99999&&l>.0001)sum+=max(0.,dot(n,v/l)-.08)*clamp(1.-l/controls.x,0.,1.);}
color=vec4(vec3(max(.78,1.-controls.y*sum/count)),1.);}
)GLSL";}
inline const char* gl_effects_composite_fs(){return R"GLSL(#version 330 core
in vec2 uv;out vec4 color;uniform sampler2D sceneColor,sceneDepth,aoTexture;
uniform vec4 projection;uniform vec2 aoTexel;uniform int aoEnabled;
// weather: enabled, night, rain strength, time. sunScreen: projected xy, intensity, front-facing.
uniform vec4 postControls; // bloom intensity, FXAA enabled, inverse source width/height
uniform vec4 weather,sunScreen;uniform vec4 atmosphere; // reserved, reserved, cockpit drops, aspect
float viewZ(float d){return projection.w/min(d-projection.z,-.0000001);}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float sky(vec2 p){return step(.99999,textureLod(sceneDepth,clamp(p,vec2(0.),vec2(1.)),0.).r);}
float sunVisibility(){
 if(sunScreen.w<.5||sunScreen.z<=0.||any(lessThan(sunScreen.xy,vec2(.006)))||any(greaterThan(sunScreen.xy,vec2(.994))))return 0.;
 // Entire footprint must see sky. A thin foreground edge must not leak a flare.
 float v=sky(sunScreen.xy);vec2 t=1./vec2(textureSize(sceneDepth,0));
 for(int i=0;i<4;++i){vec2 o=vec2((i%2==0)?-2.:2.,(i<2)?-2.:2.)*t;v*=sky(sunScreen.xy+o);}return v;
}

// Source color is linearly filtered; depth/alpha are copied from the center sample.
vec3 sceneSample(vec2 p){return textureLod(sceneColor,clamp(p,postControls.zw*.5,vec2(1.)-postControls.zw*.5),0.).rgb;}
float luma(vec3 p){return dot(p,vec3(.299,.587,.114));}
vec3 sceneAA(vec2 p,vec3 center){
 if(postControls.y<.5)return center;
 vec2 t=postControls.zw;
 vec3 nw=sceneSample(p+vec2(-1.,-1.)*t),ne=sceneSample(p+vec2(1.,-1.)*t);
 vec3 sw=sceneSample(p+vec2(-1.,1.)*t),se=sceneSample(p+t);
 float m=luma(center),a=luma(nw),b=luma(ne),c=luma(sw),d=luma(se);
 float lo=min(m,min(min(a,b),min(c,d))),hi=max(m,max(max(a,b),max(c,d)));
 if(hi-lo<max(.04,hi*.125))return center;
 vec2 dir=vec2(-((a+b)-(c+d)),(a+c)-(b+d));
 float reduce=max((a+b+c+d)*(.25*.125),1./128.);
 dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),vec2(-8.),vec2(8.))*t;
 vec3 first=.5*(sceneSample(p+dir*(1./3.-.5))+sceneSample(p+dir*(2./3.-.5)));
 vec3 second=first*.5+.25*(sceneSample(p-dir*.5)+sceneSample(p+dir*.5));
 float lm=luma(second);return lm<lo||lm>hi?first:second;
}
vec3 brightPass(vec3 sampleColor){
 float peak=max(sampleColor.r,max(sampleColor.g,sampleColor.b));
 float knee=clamp(peak-.62+.16,0.,.32);knee=knee*knee/.64;
 return sampleColor*(max(peak-.62,knee)/max(peak,.0001));
}
vec3 sceneBloom(vec2 p){
 if(postControls.x<=0.)return vec3(0.);
 // Three four-tap rings keep cost fixed, with no extra full-size buffers.
 vec3 bloom=vec3(0.);
 for(int i=0;i<12;++i){int ring=i/4,side=i%4;
 float radius=ring==0?2.:(ring==1?4.:8.);
 vec2 offset=side==0?vec2(1.,0.):(side==1?vec2(-1.,0.):(side==2?vec2(0.,1.):vec2(0.,-1.)));
 bloom+=brightPass(sceneSample(p+offset*postControls.zw*radius));}
 return min(bloom*(1.6/12.),vec3(.28))*postControls.x;
}

void main(){float raw=texture(sceneDepth,uv).r;float z=viewZ(raw),total=0.,weight=0.;
if(aoEnabled!=0)for(int i=0;i<4;++i){vec2 q=uv+vec2(float(i%2)-.5,float(i/2)-.5)*aoTexel;
float qz=viewZ(texture(sceneDepth,q).r);float w=1./(1.+abs(qz-z)*20./max(.1,z*.01));
total+=texture(aoTexture,q).r*w;weight+=w;}
vec4 source=texture(sceneColor,uv);vec3 c=sceneAA(uv,source.rgb);if(aoEnabled!=0)c*=total/max(weight,.0001);c+=sceneBloom(uv);
if(weather.x>.5){
 float night=clamp(weather.y,0.,1.),rain=clamp(weather.z,0.,1.);float aspect=max(atmosphere.w,.1);
 float l=dot(c,vec3(.2126,.7152,.0722));
 c=mix(vec3(l),c,mix(1.035,.83,rain));
 c*=mix(vec3(1.015,1.,.975),vec3(.84,.91,1.05),night);
 c*=mix(1.,.93,rain); // Scene lighting owns night darkness; preserve headlight contrast.
 float visibility=sunVisibility()*(1.-night)*(1.-rain*.92)*sunScreen.z;
 vec2 delta=(uv-sunScreen.xy)*vec2(aspect,1.);float radius=length(delta);
 // Halo is visible on sky only; lens ghosts are suppressed by the same sun occlusion.
 float halo=(exp(-radius*radius/ .0025)*.16+exp(-radius*radius/ .025)*.025)*sky(uv);
 vec2 ghost=(uv-(vec2(.5)+(vec2(.5)-sunScreen.xy)*.65))*vec2(aspect,1.);
 float flare=exp(-dot(ghost,ghost)/.0018)*.018;
 c+=vec3(1.,.79,.48)*min(visibility*(halo+flare),.20);
 // Falling rain is depth-tested world geometry; only cockpit beads live here.
 // Sparse windshield beads at side edges, above the lower instrument band.
 float edge=smoothstep(.29,.46,abs(uv.x-.5))*smoothstep(.22,.35,uv.y);
 vec2 dp=uv*vec2(17.*aspect,13.);dp.y+=weather.w*.12;vec2 df=fract(dp)-.5;
 float bead=(1.-smoothstep(.08,.16,length(df*vec2(1.,.7))))*step(.82,hash(floor(dp)));
 c+=vec3(.09,.11,.12)*bead*edge*rain*atmosphere.z;
}
color=vec4(clamp(c,0.,1.),source.a);gl_FragDepth=raw;}
)GLSL";}
inline const char* gl_effects_water_vs(){return R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;
uniform mat4 world,view,projectionMatrix;
out vec3 viewPosition,viewNormal;out vec2 wave;
void main(){vec4 w=world*vec4(position,1.);vec4 v=view*w;vec4 p=projectionMatrix*v;
gl_Position=vec4(p.xy,2.*p.z-p.w,p.w);viewPosition=v.xyz;
viewNormal=transpose(inverse(mat3(view*world)))*normal;wave=w.xz;}
)GLSL";}
inline const char* gl_effects_water_fs(){return R"GLSL(#version 330 core
in vec3 viewPosition,viewNormal;in vec2 wave;out vec4 color;
uniform sampler2D sceneColor,sceneDepth;uniform vec4 projection,controls;
uniform vec3 viewUp,viewX,viewZAxis;
float viewZ(float d){return projection.w/min(d-projection.z,-.0000001);}
vec2 projectUV(vec3 p){return .5+.5*p.xy*projection.xy/p.z;}
bool inside(vec2 u){return all(greaterThan(u,vec2(.001)))&&all(lessThan(u,vec2(.999)));}
void main(){color=vec4(0.);vec3 up=normalize(viewUp);float n2=dot(viewNormal,viewNormal);
vec3 n=n2>1e-10?viewNormal*inversesqrt(max(n2,1e-10)):up;if(dot(n,up)<0.)n=-n;
vec2 waves=vec2(sin(wave.x*.19+controls.x*.7),cos(wave.y*.23-controls.x*.55));
n=normalize(n+viewX*(waves.x*.035)+viewZAxis*(waves.y*.035));
vec3 incident=normalize(viewPosition);if(dot(n,incident)>0.)n=-n;
vec3 ray=reflect(incident,n),start=viewPosition+n*.08+ray*.12;
if(start.z<=.01)return;vec2 startUV=projectUV(start);if(!inside(startUV))return;
float startDepth=textureLod(sceneDepth,startUV,0.).r,previous=0.;
bool previousFront=startDepth<.99999&&start.z<viewZ(startDepth),found=false;
vec2 hitUV=vec2(0.);float hitT=0.;
for(int j=1;j<=16;++j){if(float(j)>controls.y)break;float t=controls.z*float(j)/max(controls.y,1.);
vec3 q=start+ray*t;if(q.z<=.01)break;vec2 u=projectUV(q);if(!inside(u))break;
float d=textureLod(sceneDepth,u,0.).r;if(d>=.99999){previous=t;previousFront=false;continue;}
float delta=q.z-viewZ(d);
if(delta>=0.&&previousFront&&t>.2){float lo=previous,hi=t;
for(int k=0;k<3;++k){float mid=(lo+hi)*.5;vec3 v=start+ray*mid;vec2 m=projectUV(v);
float md=textureLod(sceneDepth,m,0.).r;if(md<.99999&&v.z>=viewZ(md))hi=mid;else lo=mid;}
vec3 v=start+ray*hi;hitUV=projectUV(v);float hd=textureLod(sceneDepth,hitUV,0.).r;float gap=v.z-viewZ(hd);
if(inside(hitUV)&&hd<.99999&&gap>=0.&&gap<.25+.015*v.z){found=true;hitT=hi;}break;}
previous=t;previousFront=delta<0.;}
if(!found)return;float edge=min(min(hitUV.x,hitUV.y),min(1.-hitUV.x,1.-hitUV.y));
float fresnel=.02+.98*pow(1.-clamp(dot(-incident,n),0.,1.),5.);
float alpha=clamp(controls.w*fresnel*clamp(edge*12.,0.,1.)*clamp(1.-hitT/controls.z,0.,1.),0.,1.);
color=vec4(textureLod(sceneColor,hitUV,0.).rgb,alpha);}
)GLSL";}
inline const char* gl_effects_photo_fs(){return R"GLSL(#version 330 core
in vec2 uv;out vec4 color;uniform sampler2D sceneColor;uniform vec4 controls;uniform int preset;
float luminance(vec3 c){return dot(c,vec3(.2126,.7152,.0722));}
void main(){vec3 c=texture(sceneColor,uv).rgb;float l=luminance(c);
if(preset==1){c=mix(vec3(l),c,1.25);c=(c-.5)*1.1+.5;}
else if(preset==2)c=(c*.9+.05)*vec3(1.04,1.,.94);
else if(preset==3)c=vec3(l);else if(preset==4)c=l*vec3(1.12,1.,.8);
else if(preset==5)c*=vec3(.9,1.02,1.12);
c*=controls.x;c=(c-.5)*controls.y+.5;c=mix(vec3(luminance(c)),c,controls.z);
vec2 p=uv-.5;c*=1.-controls.w*clamp(2.*dot(p,p),0.,1.);color=vec4(clamp(c,0.,1.),1.);}
)GLSL";}
}
