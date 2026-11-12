#pragma once
namespace ofo {
inline const char* screen_effects_hlsl(){return R"HLSL(
// Shared registers deliberately have entrypoint-specific meanings. All matrices
// arrive as explicit rows, independent of HLSL matrix packing flags.
float4 c0:register(c0);float4 c1:register(c1);float4 c2:register(c2);
float4 c3:register(c3);float4 c4:register(c4);float4 c5:register(c5);
float4 c6:register(c6);float4 c7:register(c7);float4 c8:register(c8);
float4 c9:register(c9);float4 c10:register(c10);float4 c11:register(c11);
float4 c12:register(c12);float4 c13:register(c13);float4 c14:register(c14);
float4 c15:register(c15);float4 c16:register(c16);
float4 c17:register(c17);float4 c18:register(c18);
float4 c19:register(c19);float4 c20:register(c20);
float4 postControls:register(c24); // bloom intensity, FXAA enabled, source texel
float4 weather:register(c21);float4 sunScreen:register(c22);float4 atmosphere:register(c23);
sampler2D sceneColor:register(s0);sampler2D sceneDepth:register(s1);sampler2D aoTexture:register(s2);
float viewZ(float d,float4 p){return p.w/min(d-p.z,-0.0000001);}
float3 reconstruct(float2 uv,float d,float4 p){float z=viewZ(d,p);return float3((uv.x*2-1)*z/p.x,(1-uv.y*2)*z/p.y,z);}
float2 projectUV(float3 p,float4 proj){return float2(.5+.5*p.x*proj.x/p.z,.5-.5*p.y*proj.y/p.z);}
bool insideUV(float2 uv){return all(uv>.001)&&all(uv<.999);}
// Match the reconstruction ray to the actual point-sampled INTZ texel.
float2 depthUV(float2 uv){float2 size=1/c1.xy;return (clamp(floor(uv*size),float2(0,0),size-1)+.5)*c1.xy;}
float4 ao_ps(float2 uv:TEXCOORD0):COLOR0 {
 float2 center=depthUV(uv);
 float raw=tex2Dlod(sceneDepth,float4(center,0,0)).r;
 float3 p=reconstruct(center,raw,c0);
 // Source-resolution, depth-continuous normal: reduced derivatives erase contacts.
 float2 t=c1.xy;
 float3 l=p-reconstruct(center-float2(t.x,0),tex2Dlod(sceneDepth,float4(center-float2(t.x,0),0,0)).r,c0);
 float3 r=reconstruct(center+float2(t.x,0),tex2Dlod(sceneDepth,float4(center+float2(t.x,0),0,0)).r,c0)-p;
 float3 b=p-reconstruct(center-float2(0,t.y),tex2Dlod(sceneDepth,float4(center-float2(0,t.y),0,0)).r,c0);
 float3 u=reconstruct(center+float2(0,t.y),tex2Dlod(sceneDepth,float4(center+float2(0,t.y),0,0)).r,c0)-p;
 float3 n=cross(abs(l.z)<abs(r.z)?l:r,abs(b.z)<abs(u.z)?b:u);n*=rsqrt(max(dot(n,n),1e-12));
 if(dot(n,p)>0)n=-n;
 if(raw>=.99999||c2.w<.5)return 1;
 float sum=0;float count=max(c2.z,1);
 for(int i=0;i<8;i++)if(i<c2.z){
  float angle=(i+.5)*6.2831853/count;
  float2 ring=float2(cos(angle),sin(angle));
  float2 suv=center+ring*c2.x*float2(c0.x,c0.y)*.5/max(p.z,.01)*(.25+.4*(i%2));
  float2 tap=depthUV(suv);float d=tex2Dlod(sceneDepth,float4(tap,0,0)).r;float3 v=reconstruct(tap,d,c0)-p;
  float len=length(v);
  if(insideUV(suv)&&d<.99999&&len>.0001)
   sum+=max(0,dot(n,v/len)-.08)*saturate(1-len/c2.x);
 }
 return max(.78,1-c2.y*sum/count);
}
float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float sky(float2 p){return step(.99999,tex2Dlod(sceneDepth,float4(saturate(p),0,0)).r);}
float sunVisibility(){
 if(sunScreen.w<.5||sunScreen.z<=0.||any(sunScreen.xy<float2(.006,.006))||any(sunScreen.xy>float2(.994,.994)))return 0.;
 // Entire footprint must see sky. A thin foreground edge must not leak a flare.
 float v=sky(sunScreen.xy);float2 t=c1.xy;
 for(int i=0;i<4;++i){float2 o=float2((i%2==0)?-2.:2.,(i<2)?-2.:2.)*t;v*=sky(sunScreen.xy+o);}return v;
}


// Source color is linearly filtered; depth/alpha are copied from the center sample.
float3 sceneSample(float2 p){return tex2Dlod(sceneColor,float4(clamp(p,postControls.zw*.5,float2(1.,1.)-postControls.zw*.5),0,0)).rgb;}
float luma(float3 p){return dot(p,float3(.299,.587,.114));}
float3 sceneAA(float2 p,float3 center){
 if(postControls.y<.5)return center;
 float2 t=postControls.zw;
 float3 nw=sceneSample(p+float2(-1.,-1.)*t),ne=sceneSample(p+float2(1.,-1.)*t);
 float3 sw=sceneSample(p+float2(-1.,1.)*t),se=sceneSample(p+t);
 float m=luma(center),a=luma(nw),b=luma(ne),c=luma(sw),d=luma(se);
 float lo=min(m,min(min(a,b),min(c,d))),hi=max(m,max(max(a,b),max(c,d)));
 if(hi-lo<max(.04,hi*.125))return center;
 float2 dir=float2(-((a+b)-(c+d)),(a+c)-(b+d));
 float reduce=max((a+b+c+d)*(.25*.125),1./128.);
 dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),float2(-8.,-8.),float2(8.,8.))*t;
 float3 first=.5*(sceneSample(p+dir*(1./3.-.5))+sceneSample(p+dir*(2./3.-.5)));
 float3 second=first*.5+.25*(sceneSample(p-dir*.5)+sceneSample(p+dir*.5));
 float lm=luma(second);return lm<lo||lm>hi?first:second;
}
float3 brightPass(float3 sampleColor){
 float peak=max(sampleColor.r,max(sampleColor.g,sampleColor.b));
 float knee=clamp(peak-.62+.16,0.,.32);knee=knee*knee/.64;
 return sampleColor*(max(peak-.62,knee)/max(peak,.0001));
}
float3 sceneBloom(float2 p){
 if(postControls.x<=0.)return float3(0,0,0);
 // Three four-tap rings keep cost fixed, with no extra full-size buffers.
 float3 bloom=float3(0,0,0);
 [loop]for(int i=0;i<12;++i){int ring=i/4,side=i%4;
 float radius=ring==0?2.:(ring==1?4.:8.);
 float2 offset=side==0?float2(1.,0.):(side==1?float2(-1.,0.):(side==2?float2(0.,1.):float2(0.,-1.)));
 bloom+=brightPass(sceneSample(p+offset*postControls.zw*radius));}
 return min(bloom*(1.6/12.),float3(.28,.28,.28))*postControls.x;
}

struct CompositeOut {float4 color:COLOR0;float depth:DEPTH;};
CompositeOut composite_ps(float2 uv:TEXCOORD0){
 CompositeOut o;float raw=tex2D(sceneDepth,uv).r;float z=viewZ(raw,c0);
 float total=0,weight=0;
 // Bilateral four tap upsample: avoid AO bleeding across foreground edges.
 for(int i=0;i<4;i++){
  float2 offset=float2((i%2)-.5,(i/2)-.5)*c1.zw;
  float2 q=uv+offset;float qz=viewZ(tex2D(sceneDepth,q).r,c0);
  float w=1/(1+abs(qz-z)*20/max(.1,z*.01));
  total+=tex2D(aoTexture,q).r*w;weight+=w;
 }
 float factor=c2.w>.5?total/max(weight,.0001):1;
 o.color=tex2D(sceneColor,uv);float3 c=sceneAA(uv,o.color.rgb)*factor+sceneBloom(uv);
if(weather.x>.5){
 float night=clamp(weather.y,0.,1.);float rain=clamp(weather.z,0.,1.);float aspect=max(atmosphere.w,.1);
 float l=dot(c,float3(.2126,.7152,.0722));
 c=lerp(float3(l,l,l),c,lerp(1.035,.83,rain));
 c*=lerp(float3(1.015,1.,.975),float3(.84,.91,1.05),night);
 c*=lerp(1.,.93,rain); // Scene lighting owns night darkness; preserve headlight contrast.
 float visibility=sunVisibility()*(1.-night)*(1.-rain*.92)*sunScreen.z;
 float2 delta=(uv-sunScreen.xy)*float2(aspect,1.);float radius=length(delta);
 // Halo is visible on sky only; lens ghosts are suppressed by the same sun occlusion.
 float halo=(exp(-radius*radius/ .0025)*.16+exp(-radius*radius/ .025)*.025)*sky(uv);
 float2 ghost=(uv-(float2(.5,.5)+(float2(.5,.5)-sunScreen.xy)*.65))*float2(aspect,1.);
 float flare=exp(-dot(ghost,ghost)/.0018)*.018;
 c+=float3(1.,.79,.48)*min(visibility*(halo+flare),.20);
 // Falling rain is depth-tested world geometry; only cockpit beads live here.
 // Sparse windshield beads at side edges, above the lower instrument band.
 float edge=smoothstep(.29,.46,abs(uv.x-.5))*smoothstep(.22,.35,uv.y);
 float2 dp=uv*float2(17.*aspect,13.);dp.y+=weather.w*.12;float2 df=frac(dp)-.5;
 float bead=(1.-smoothstep(.08,.16,length(df*float2(1.,.7))))*step(.82,hash(floor(dp)));
 c+=float3(.09,.11,.12)*bead*edge*rain*atmosphere.z;
}

o.color.rgb=saturate(c);o.depth=raw;return o;
}
struct WaterInput {float3 position:POSITION0;float3 normal:NORMAL0;float2 uv:TEXCOORD0;};
struct WaterOutput {float4 position:POSITION0;float3 viewPosition:TEXCOORD0;float3 viewNormal:TEXCOORD1;float2 wave:TEXCOORD2;};
WaterOutput water_vs(WaterInput i){WaterOutput o;float4 p=float4(i.position,1);
 o.position=float4(dot(c0,p),dot(c1,p),dot(c2,p),dot(c3,p));
 o.viewPosition=float3(dot(c4,p),dot(c5,p),dot(c6,p));
 o.viewNormal=float3(dot(c8.xyz,i.normal),dot(c9.xyz,i.normal),dot(c10.xyz,i.normal));
 // World XZ phase remains continuous across batches and camera movement.
 o.wave=float2(dot(c17,p),dot(c19,p));return o;
}
float4 water_ps(WaterOutput i):COLOR0 {
 float3 up=normalize(c11.xyz);float n2=dot(i.viewNormal,i.viewNormal);float3 n=n2>1e-10?i.viewNormal*rsqrt(max(n2,1e-10)):up;
 if(dot(n,up)<0)n=-n;
 float2 wave=float2(sin(i.wave.x*.19+c13.x*.7),cos(i.wave.y*.23-c13.x*.55));
 n=normalize(n+c15.xyz*(wave.x*.035)+c16.xyz*(wave.y*.035));
 float3 incident=normalize(i.viewPosition);if(dot(n,incident)>0)n=-n;
 float3 ray=reflect(incident,n);float3 start=i.viewPosition+n*.08+ray*.12;
 if(start.z<=.01)return 0;float2 startUV=projectUV(start,c12);if(!insideUV(startUV))return 0;
 float startDepth=tex2Dlod(sceneDepth,float4(startUV,0,0)).r;
 float previous=0;bool previousFront=startDepth<.99999&&start.z<viewZ(startDepth,c12);
 float2 hitUV=0;float hitT=0;bool found=false;
 [loop] for(int j=1;j<=16;j++){
  if(j>c13.y||found)break;
  float t=c13.z*j/max(c13.y,1);float3 q=start+ray*t;
  if(q.z<=.01)break;float2 uv=projectUV(q,c12);if(!insideUV(uv))break;
  float d=tex2Dlod(sceneDepth,float4(uv,0,0)).r;
  if(d>=.99999){previous=t;previousFront=false;continue;}
  float delta=q.z-viewZ(d,c12);
  if(delta>=0&&previousFront&&t>.2){
   float lo=previous,hi=t;
   [unroll] for(int k=0;k<3;k++){
    float mid=(lo+hi)*.5;float3 v=start+ray*mid;float2 m=projectUV(v,c12);
    float md=tex2Dlod(sceneDepth,float4(m,0,0)).r;
    if(md<.99999&&v.z>=viewZ(md,c12))hi=mid;else lo=mid;
   }
   float3 v=start+ray*hi;hitUV=projectUV(v,c12);
   float hd=tex2Dlod(sceneDepth,float4(hitUV,0,0)).r;float gap=v.z-viewZ(hd,c12);
   if(insideUV(hitUV)&&hd<.99999&&gap>=0&&gap<.25+.015*v.z){found=true;hitT=hi;}
   else break;
  }
  previous=t;previousFront=delta<0;
 }
 if(!found)return 0;
 float edge=min(min(hitUV.x,hitUV.y),min(1-hitUV.x,1-hitUV.y));
 float fresnel=.02+.98*pow(1-saturate(dot(-incident,n)),5);
 float alpha=saturate(c13.w*fresnel*saturate(edge*12)*saturate(1-hitT/c13.z));
 return float4(tex2Dlod(sceneColor,float4(hitUV,0,0)).rgb,alpha);
}
)HLSL";}
}
