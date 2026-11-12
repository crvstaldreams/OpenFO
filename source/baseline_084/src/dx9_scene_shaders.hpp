#pragma once
namespace ofo {
// Native LH world coordinates. Authored receiver normals never depend on camera facing.
inline const char* dx9_scene_hlsl(){return R"HLSL(
float4 matrixRows[12]:register(c0);
float4 environment:register(c12); // night, wet, sun, lamp cars
float4 camera:register(c13);float4 sunlight:register(c14);
float4 material:register(c15); // body, glass, normal (0=none, 1=RGB, 2=packed), lightmap
float4 materialControl:register(c18); // semantic mode, time, has Tex1, has Tex2
float4 materialOptions:register(c19); // has sky, fog enabled, cracked, full-sphere night sky
float4 fog:register(c16); // rgb, density
float4 poses[32]:register(c32);float4 mounts[8]:register(c64);
float4 boundsLo[8]:register(c72);float4 boundsHi[8]:register(c80);
sampler2D lampShadow0:register(s3);sampler2D lampShadow1:register(s4);float4 shadowControl:register(c17);
sampler2D dirtTexture:register(s5);
sampler2D skyDown:register(s11);sampler2D skyFront:register(s6);sampler2D skyBack:register(s7);sampler2D skyLeft:register(s8);sampler2D skyRight:register(s9);sampler2D skyUp:register(s10);
sampler2D diffuseTexture:register(s0);sampler2D lightTexture:register(s1);sampler2D normalTexture:register(s2);
struct Input{float3 position:POSITION0;float3 normal:NORMAL0;float4 color:COLOR0;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;};
struct Output{float4 position:POSITION0;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float2 uv1:TEXCOORD3;float4 color:COLOR0;};
// Vertex-only expansion registers do not alias the pixel lamp registers.
float4 billboardRight:register(c20);float4 billboardUp:register(c21);
float4 foliageWind[16]:register(c96);float4 foliageFade:register(c112);
Output scene_vs(Input i){
 Output o;float3 position=i.position;float3 normal=i.normal;float4 tint=i.color;
 if(materialControl.x==20){
  position=i.position-i.normal+i.normal.x*billboardRight.xyz+i.normal.y*billboardUp.xyz;
  normal=cross(billboardRight.xyz,billboardUp.xyz);
 }
 if(materialControl.x==21){
  // Exact native plant_wind_assembly signed width, top bend, phase and fade.
  float halfWidth=i.uv1.x;float top=i.uv1.y;
  float3 wind=foliageWind[clamp((int)i.normal.z,0,15)].xyz;
  float edge=abs(halfWidth)*wind.z*step(.00001,top);
  position.x+=i.normal.x*halfWidth+i.normal.y*top*wind.x+i.normal.x*edge;
  position.z+=i.normal.y*halfWidth-i.normal.x*top*wind.x+i.normal.y*edge;
  position.y+=top*wind.y;
  normal=cross(float3(i.normal.x,0,i.normal.y),float3(i.normal.y*wind.x,wind.y,-i.normal.x*wind.x));
  tint.a=saturate((foliageFade.x-length(i.position-camera.xyz))*foliageFade.y);
 }
 float4 p=float4(position,1);
 o.position=float4(dot(matrixRows[0],p),dot(matrixRows[1],p),dot(matrixRows[2],p),dot(matrixRows[3],p));
 o.world=float3(dot(matrixRows[4],p),dot(matrixRows[5],p),dot(matrixRows[6],p));
 o.normal=float3(dot(matrixRows[8].xyz,normal),dot(matrixRows[9].xyz,normal),dot(matrixRows[10].xyz,normal));
 o.uv=i.uv;if(materialControl.x==6)o.uv+=frac(materialControl.y*float2(.002,.0005));
 o.uv1=i.uv1;o.color=tint;return o;
}
// ps_3_0 constant registers cannot be relatively addressed. These bounded
// selectors keep every uniform register reference literal (Microsoft PS3 ABI).
float4 poseRow0(int car){if(car==0)return poses[0];if(car==1)return poses[4];if(car==2)return poses[8];if(car==3)return poses[12];if(car==4)return poses[16];if(car==5)return poses[20];if(car==6)return poses[24];return poses[28];}
float4 poseRow1(int car){if(car==0)return poses[1];if(car==1)return poses[5];if(car==2)return poses[9];if(car==3)return poses[13];if(car==4)return poses[17];if(car==5)return poses[21];if(car==6)return poses[25];return poses[29];}
float4 poseRow2(int car){if(car==0)return poses[2];if(car==1)return poses[6];if(car==2)return poses[10];if(car==3)return poses[14];if(car==4)return poses[18];if(car==5)return poses[22];if(car==6)return poses[26];return poses[30];}
float4 lampMount(int car){if(car==0)return mounts[0];if(car==1)return mounts[1];if(car==2)return mounts[2];if(car==3)return mounts[3];if(car==4)return mounts[4];if(car==5)return mounts[5];if(car==6)return mounts[6];return mounts[7];}
float4 carBoundsLo(int car){if(car==0)return boundsLo[0];if(car==1)return boundsLo[1];if(car==2)return boundsLo[2];if(car==3)return boundsLo[3];if(car==4)return boundsLo[4];if(car==5)return boundsLo[5];if(car==6)return boundsLo[6];return boundsLo[7];}
float4 carBoundsHi(int car){if(car==0)return boundsHi[0];if(car==1)return boundsHi[1];if(car==2)return boundsHi[2];if(car==3)return boundsHi[3];if(car==4)return boundsHi[4];if(car==5)return boundsHi[5];if(car==6)return boundsHi[6];return boundsHi[7];}
float3 localPoint(float3 p,int car){float4 q=float4(p,1);return float3(dot(poseRow0(car),q),dot(poseRow1(car),q),dot(poseRow2(car),q));}
float3 localDirection(float3 p,int car){return float3(dot(poseRow0(car).xyz,p),dot(poseRow1(car).xyz,p),dot(poseRow2(car).xyz,p));}
float blocked(float3 source,float3 target,int own){float result=0;[loop]for(int k=0;k<8;k++)if(k<environment.w&&k!=own){float3 a=localPoint(source,k),b=localPoint(target,k)-a;float3 inv=(step(0,b)*2-1)/max(abs(b),.00001);float3 x=(carBoundsLo(k).xyz-a)*inv,y=(carBoundsHi(k).xyz-a)*inv;float3 lo=min(x,y),hi=max(x,y);float enter=max(max(lo.x,lo.y),max(lo.z,.001)),leave=min(min(hi.x,hi.y),min(hi.z,.995));result=max(result,step(enter,leave));}return result;}
// Same pitched projection and nonlinear depth as the GL depth atlas.
float3 headlightBeam(float3 q){return float3(q.x,(q.y+.11*q.z)*.99398743,(q.z-.11*q.y)*.99398743);}
float headlightDepth(float z){return 150./(150.-.12)-(.12*150./(150.-.12))/max(z,.001);}
float headlightDistance(float depth){return .12*150./max(150.-depth*(150.-.12),.12);}
float4 shadow_ps(Output i):COLOR0{float3 q=localPoint(i.world,0)-float3(0,mounts[0].y,mounts[0].z);return headlightDepth(headlightBeam(q).z);}
// Project geometric derivatives analytically, outside SM3 gradient operations
// in dynamic light loops. This retains receiver-plane PCF on steep terrain.
float2 headlightDepthGradient(float3 beam,float3 beamDx,float3 beamDy){
 float invZ=rcp(max(beam.z,.001));
 float2 projected=beam.xy*invZ;
 float2 scale=float2(.5/.70,-.5/.38)*invZ;
 float2 dx=(beamDx.xy-projected*beamDx.z)*scale,dy=(beamDy.xy-projected*beamDy.z)*scale;
 float depthScale=(.12*150./(150.-.12))*invZ*invZ;
 float zx=beamDx.z*depthScale,zy=beamDy.z*depthScale;
 float det=dx.x*dy.y-dx.y*dy.x;
 return abs(det)>1.e-12?float2(zx*dy.y-zy*dx.y,zy*dx.x-zx*dy.x)/det:float2(0,0);
}
float headlightReceiverBias(float up,float incidence){
 return .065+smoothstep(.48,.85,abs(up))*min(.8,.012/max(abs(incidence),.01));
}
float staticShadow(float3 q,int side,float bias,float3 beamDx,float3 beamDy){
 float3 beam=headlightBeam(q);float z=max(.001,beam.z);
 // D3D9 raster centers are integers; the shadow texture samples at i+.5.
 float size=shadowControl.w>0?shadowControl.w:512;
 float2 cone=beam.xy/(z*float2(.70,.38));
 float2 uv=float2(.5+.5*cone.x,.5-.5*cone.y)+.5/size;
 float receiver=headlightDepth(beam.z);float2 gradient=headlightDepthGradient(beam,beamDx,beamDy);
 [branch]if(beam.z<=.12||beam.z>=150||dot(cone,cone)>=1)return 1;
 float2 base=floor(uv*size-.5);float visible=0;
 [unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  float2 tap=clamp((base+float2(x,y)+.5)/size,.5/size,1-.5/size);
  float depth=side==0?tex2Dlod(lampShadow0,float4(tap,0,0)).r:tex2Dlod(lampShadow1,float4(tap,0,0)).r;
  float corrected=saturate(receiver+dot(gradient,tap-uv));
  visible+=headlightDistance(corrected)-bias<=headlightDistance(depth)?.25:0;
 }return visible;
}
// Compact SM3 receiver for static track geometry when the general material
// shader is unavailable, or no original effect covers the material ID.
float4 night_static_ps(Output i):COLOR0{
 int mode=(int)materialControl.x;
 float4 tex=shadowControl.y>.5?tex2D(diffuseTexture,i.uv):float4(1,1,1,1);
 bool lightmapped=(mode==2||mode==11||mode==14)&&materialControl.z>.5;
 float4 detail=lightmapped?tex2D(lightTexture,i.uv1):float4(1,1,1,1);
 float4 surface=lightmapped?tex*detail:tex*i.color;
 if(mode==1||mode==9||mode==20)surface.rgb*=2;
 if(mode==12)surface=i.color;
 if(mode==13)return tex;
 float3 n=normalize(i.normal),l=normalize(-sunlight.xyz);
 float3 color=surface.rgb*float3(.205,.265,.36)*(.78+.14*max(n.y,0)+.18*saturate(dot(n,l)));
 float3 beamDx=headlightBeam(localDirection(ddx(i.world),0)),beamDy=headlightBeam(localDirection(ddy(i.world),0));
 if(environment.w>.5){
  float3 p=localPoint(i.world,0),localNormal=localDirection(n,0);
  [unroll]for(int side=0;side<2;side++){
   float3 q=p-float3((side*2-1)*mounts[0].x,mounts[0].yz);
   float3 beam=headlightBeam(q);
   float2 uv=beam.xy/max(beam.z,.001)/float2(.70,.38);
   float radius=dot(uv,uv);
   float cone=saturate(1-radius)*saturate((.65-uv.y)*3)*(1-smoothstep(.55,1,radius))*
    smoothstep(.12,1,beam.z)*(1-smoothstep(105,150,beam.z))/(1+.0008*dot(q,q));
   float incidence=dot(localNormal,-normalize(q));
   float receive=mode==20?abs(incidence):max(saturate(incidence),smoothstep(.48,.85,n.y)*smoothstep(15,55,beam.z)*.30);
   if(cone>.001){
    float visibility=shadowControl.z>.5?staticShadow(q,side,headlightReceiverBias(n.y,incidence),beamDx,beamDy):1;
    color+=(lightmapped?detail.rgb:tex.rgb)*float3(3.3,3.003,2.376)*cone*receive*visibility;
   }
  }
 }
 float nearFog=100/(1+fog.w*3),farFog=650/(1+fog.w*2);
 float amount=materialOptions.y*saturate((length(camera.xyz-i.world)-nearFog)/(farFog-nearFog));
 return float4(lerp(color,fog.rgb,amount),surface.a);
}
// Borrow the five original skybox faces: static reflection, no extra scene render.
float3 nativeReflection(float3 r){float3 a=abs(r);float2 uv;
 if(a.y>=max(a.x,a.z)&&r.y>0){uv=float2(r.x,r.z)/max(a.y,.0001)*.5+.5;return tex2Dlod(skyUp,float4(uv,0,0)).rgb;}
 if(a.y>=max(a.x,a.z)&&r.y<=0){uv=float2(r.x,-r.z)/max(a.y,.0001)*.5+.5;return materialOptions.w>.5?tex2Dlod(skyDown,float4(uv,0,0)).rgb:float3(.10,.12,.13);}
 if(a.z>=a.x){uv=float2((r.z>0?r.x:-r.x)/max(a.z,.0001)*.5+.5,(1-r.y/max(a.z,.0001))*(materialOptions.w>.5?.5:1));return r.z>0?tex2Dlod(skyFront,float4(uv,0,0)).rgb:tex2Dlod(skyBack,float4(uv,0,0)).rgb;}
 uv=float2((r.x>0?-r.z:r.z)/max(a.x,.0001)*.5+.5,(1-r.y/max(a.x,.0001))*(materialOptions.w>.5?.5:1));return r.x>0?tex2Dlod(skyRight,float4(uv,0,0)).rgb:tex2Dlod(skyLeft,float4(uv,0,0)).rgb;}
// Native PRO body uses authored mesh normals; paint/rust RGB is not height.
float3 surfaceNormal(float3 n,float3 px,float3 py,float2 tx,float2 ty,float3 sampleNormal){
 float3 a=cross(py,n),b=cross(n,px),t=a*tx.x+b*ty.x,bitangent=a*tx.y+b*ty.y;
 float scale=max(dot(t,t),dot(bitangent,bitangent));
 return scale>1.e-12?normalize((t*sampleNormal.x+bitangent*sampleNormal.y)*rsqrt(max(scale,1.e-12))+n*sampleNormal.z):n;
}
float surfaceHighlight(float3 n,float3 l,float3 v,float reflectance,float gloss,float power){
 return reflectance*lerp(.24,1,gloss)*pow(saturate(dot(n,normalize(l+v))),power);
}
float4 scene_ps(Output i):COLOR0{
 int mode=(int)materialControl.x;
 float2 baseUV=i.uv,normalUV=shadowControl.x>.5?i.uv1:i.uv;
 float4 tex=shadowControl.y>.5?tex2D(diffuseTexture,baseUV):float4(1,1,1,1);
 float3 n=normalize(i.normal),detailN=n;
 float3 worldDx=ddx(i.world),worldDy=ddy(i.world);
 float2 uvDx=ddx(normalUV),uvDy=ddy(normalUV);
 bool packed=material.z>1.5&&(mode==1||mode==2||mode==8||mode==10||mode==11);
 float surfaceSpec=0,gloss=0,power=8;
 if(material.z>.5&&mode!=3){
  float4 surfaceSample=tex2D(normalTexture,normalUV);
  if(packed){
   float2 xy=surfaceSample.rg*2-1;
   detailN=surfaceNormal(n,worldDx,worldDy,uvDx,uvDy,float3(xy,sqrt(max(.0001,1-dot(xy,xy)))));
   surfaceSpec=surfaceSample.b;
   float roughness=clamp(surfaceSample.a,.08,1.);
   gloss=1-lerp(roughness,max(.12,roughness*.55),environment.y);power=lerp(6,112,gloss*gloss);
  }else if(material.z<1.5)n=detailN=surfaceNormal(n,worldDx,worldDy,uvDx,uvDy,surfaceSample.xyz*2-1);
 }
float3 v=normalize(camera.xyz-i.world),l=normalize(-sunlight.xyz);float ndl=saturate(dot(n,l));
// Directional LUT and hemisphere constants are reconstructed from the original
// shader equations. Native LUT cubemap/live register captures are unavailable.
float3 diffuse=saturate(float3(.55,.55,.55)+n.y*.12+float3(.55,.55,.55)*ndl);
float spec=pow(saturate(dot(n,normalize(l+v))),8)*step(.0001,ndl);
float grazing=pow(1-saturate(abs(dot(n,v))),5);float4 result=tex*i.color;
bool lightmapMode=mode==2||mode==11||mode==14;
bool lightmapped=lightmapMode&&materialControl.z>.5;
float4 second=materialControl.z>.5?tex2D(lightTexture,lightmapMode?i.uv1:i.uv):float4(1,1,1,1);
if(mode==1||mode==9||mode==20)result=float4(2*tex.rgb*i.color.rgb,tex.a*i.color.a);
else if(mode==2||mode==11||mode==14){result=tex*second;if(mode==11&&!packed)result.rgb+=.3*spec*result.a;}
else if(mode==12)result=i.color;
else if(mode==13)result=tex;
else if(mode==5)result=float4(tex.rgb*second.rgb,tex.a*i.color.a);
else if(mode==6)result=2*tex*i.color;
else if(mode==8||mode==16)result=float4(tex.rgb*diffuse,tex.a);
else if(mode==7){
 float nv=saturate(abs(dot(n,v)));float3 reflected=materialOptions.x>.5?nativeReflection(reflect(-v,n)):tex.rgb;
 float3 tint=lerp(float3(.065,.14,.16),tex.rgb*i.color.rgb,.22);
 result=float4(lerp(reflected,tint,.35+.5*nv),clamp(.82+.12*grazing,.82,.94));
}
else if(mode==3){
 if(materialControl.w>.5)tex=lerp(tex,tex2D(dirtTexture,i.uv),saturate(i.color.a));
 float mask=shadowControl.y>.5?tex.a:0;float fresnel=.25+.75*grazing;
 float3 lit=saturate(tex.rgb*diffuse+.3*spec*mask);
 result=float4(materialOptions.x>.5?lerp(lit,nativeReflection(reflect(-v,n)),mask*fresnel):lit,1);
}
if(material.y>.5){bool cracked=materialOptions.z>.5||mode==15;float fresnel=.375+.625*grazing;
 float dirt=shadowControl.y>.5?tex.a:0;float amount=saturate(fresnel-dirt);
 float3 reflected=materialOptions.x>.5?nativeReflection(reflect(-v,n)):tex.rgb;
 result=float4(lerp(tex.rgb*diffuse,reflected,amount)+.3*spec*amount,saturate(fresnel+dirt));
 if(cracked)result=float4(lerp(tex.rgb*diffuse,reflected,.12*fresnel)+.3*spec*.12,tex.a*i.color.a);
}
float3 albedo=lightmapped?second.rgb:tex.rgb;float3 color=result.rgb;float alpha=result.a;
bool emissive=mode==13||mode==5;
if(!emissive){// Cool moon/hemisphere fill preserves authored prelight and lightmap contrast.
 float moon=saturate(dot(n,l));float hemisphere=.78+.14*max(n.y,0)+.18*moon;
 color*=lerp(float3(1,1,1),float3(.205,.265,.36)*hemisphere,environment.x);color*=1-environment.y*lerp(.28,.12,environment.x);
 if(packed){
  color*=clamp((.72+.28*saturate(dot(detailN,l)))/(.72+.28*ndl),.87,1.13);
  float prelit=saturate(max(color.r,max(color.g,color.b))*2);
  color+=float3(1,.93,.83)*(.22*surfaceHighlight(detailN,l,v,surfaceSpec,gloss,power)*ndl*prelit*environment.z*(1-environment.x));
 }
 if(environment.y>.001&&material.y<.5){float f=lerp(.025+.25*grazing,.02+.14*grazing,environment.x);float3 reflected=materialOptions.x>.5?nativeReflection(reflect(-v,detailN)):color;
 reflected=lerp(reflected,min(reflected,float3(.28,.28,.28)),environment.x);color=lerp(color,reflected,environment.y*f*(packed?lerp(.4,1,gloss):1));}
 color*=lerp((.55+.45*ndl*environment.z)/(.55+.45*ndl),1,environment.x);
}
float3 beamDx=headlightBeam(localDirection(worldDx,0)),beamDy=headlightBeam(localDirection(worldDy,0));
[loop]for(int car=0;car<8;car++)if(car<environment.w&&!emissive){float3 p=localPoint(i.world,car);[loop]for(int side=0;side<2;side++){float3 lamp=float3((side*2-1)*lampMount(car).x,lampMount(car).y,lampMount(car).z);float3 q=p-lamp;float3 beam=headlightBeam(q);float2 uv=beam.xy/max(beam.z,.001)/float2(.70,.38);float radius=dot(uv,uv);float cookie=saturate(1-radius)*saturate((.65-uv.y)*3);float cone=cookie*(1-smoothstep(.55,1,radius))*smoothstep(.12,1.0,beam.z)*(1-smoothstep(105,150,beam.z))/(1+.0008*dot(q,q));float3 dir=normalize(localDirection(n,car));float receive=(mode==20||mode==21||mode==22)?abs(dot(dir,-normalize(q))):max(saturate(dot(dir,-normalize(q))),smoothstep(.48,.85,n.y)*smoothstep(15,55,beam.z)*.30); /* distant road grazing compensation */float3 source=i.world; // inverse rigid pose: R transpose maps local lamp offset back to world
float3 delta=lamp-p;source+=float3(dot(float3(poseRow0(car).x,poseRow1(car).x,poseRow2(car).x),delta),dot(float3(poseRow0(car).y,poseRow1(car).y,poseRow2(car).y),delta),dot(float3(poseRow0(car).z,poseRow1(car).z,poseRow2(car).z),delta));
float staticVisibility=(car==0&&shadowControl.z>.5)?staticShadow(q,side,headlightReceiverBias(n.y,dot(dir,-normalize(q))),beamDx,beamDy):1;
float reflectance=mode==3?tex.a:(material.y>.5?.16:(mode==7?.8:.12));
if(cone>.001&&beam.z>.12&&beam.z<150){
 float3 ray=-normalize(q),localView=normalize(localDirection(v,car));
 // Gloss follows microgeometry, but LIGHTS072 coverage uses the authored normal.
 float highlight=packed?.65*surfaceHighlight(normalize(localDirection(detailN,car)),ray,localView,surfaceSpec,gloss,power):
     pow(saturate(dot(dir,normalize(ray+localView))),lerp(16,60,environment.y))*environment.y*.45*reflectance;
 color+=(albedo*3.3+highlight)*float3(1,.91,.72)*cone*receive*(1-blocked(source,i.world,car))*staticVisibility;
}}}
float distance=length(camera.xyz-i.world);if(mode==16&&materialControl.w>.5)color+=tex2D(dirtTexture,i.uv).rgb;float fogAmount=materialOptions.y*saturate((distance-100/(1+fog.w*3))/(650/(1+fog.w*2)-100/(1+fog.w*3)));color=lerp(color,fog.rgb,fogAmount);return float4(color,alpha);}
)HLSL";}
}
