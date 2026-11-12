// Offline import; runtime needs only the generated OFAD mesh and the original DDS.
// Usage: node export_alpha_driver.mjs <granny-ro-js/src/Granny.js> <driver.gr2> <male_bones.ini> <johnny.ofad>
import fs from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
const [library,input,retailBones,output]=process.argv.slice(2);
if (!output) throw Error('Pass Granny.js, prototype driver.gr2, native male_bones.ini and output path');
const {parseAll}=await import(pathToFileURL(resolve(library)));
const source=fs.readFileSync(input), parsed=parseAll(Uint8Array.from(source).buffer);
const mesh=parsed.meshes.find(m=>m.name==='Johnny_body');
if (!mesh || mesh.vertexCount!==1663 || mesh.indexCount!==6774 || parsed.skeletons.length!==1) throw Error('Expected the Retro Demo Johnny driver');
const names=['lower_torso','upper_leg_l','lower_leg_l','upper_leg_r','lower_leg_r','upper_torso','head','upper_arm_l','lower_arm_l','upper_arm_r','lower_arm_r'];
const boneText=fs.readFileSync(retailBones,'utf8');
const groups=boneText.split(/\bSegments\s*=\s*\{/);
if(groups.length!==2)throw Error('Expected native body segments');
const coord=s=>s.split(',').map(Number);
const dst=names.map((name,id)=>{
 const joint=groups[0].match(new RegExp('\\['+(id+1)+'\\]\\s*=\\s*\\{\\s*Position\\s*=\\s*\\{([^}]+)'));
 const center=groups[1].match(new RegExp('\\b'+name+'\\s*=\\s*\\{[^}]+\\}\\s*,\\s*Position\\s*=\\s*\\{([^}]+)'));
 if (!joint || !center) throw Error('Missing native rig joint '+name);
 return {joint:coord(joint[1]),center:coord(center[1])};
});
const add=(a,b)=>a.map((x,i)=>x+b[i]), sub=(a,b)=>a.map((x,i)=>x-b[i]), mul=(a,s)=>a.map(x=>x*s);
const dot=(a,b)=>a.reduce((s,x,i)=>s+x*b[i],0);
const cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
const len=a=>Math.hypot(...a), unit=a=>mul(a,1/Math.max(1e-10,len(a)));
const axis=([x,y,z])=>[-x,z,-y]; // Granny centimeters / Z up / forward -Y; OpenFO meters / Y up / +Z
const src={};
for(const b of parsed.skeletons[0].bones) {
 // InverseWorldTransform maps point in world into the original joint frame.
 const m=b.inverseWorldTransform, t=m.slice(12,15);
 src[b.name]=mul(axis([dot(m.slice(0,3),t),dot(m.slice(4,7),t),dot(m.slice(8,11),t)]),-.01);
}
function body(v) {
 const a=src.Hips, root=dst[0].joint;
 const y=v[1] <= a[1]?root[1]+(v[1]-a[1])*.90 :
         v[1] <= src.Neck[1]?root[1]+(v[1]-a[1])*(dst[6].joint[1]-root[1])/(src.Neck[1]-a[1]) :
         dst[6].joint[1]+(v[1]-src.Neck[1])*.88;
 const width=.76+.12*Math.max(0,Math.min(1,(v[1]-1.5)*12));
 return [root[0]+(v[0]-a[0])*width,y,root[2]+(v[2]-a[2])*.88];
}
function rotate(a,b,v) {
 const c=cross(a,b), d=dot(a,b), t=cross(c,v);
 if(d<-.999)throw Error('Opposed retarget axes');
 return add(add(v,t),mul(cross(c,t),1/(1+d)));
}
const maps={};
function limb(i,a,b,end,radial){
 const old=src[a], target=dst[i].joint, u=unit(sub(src[b],old)), span=sub(end,target), w=unit(span), scale=len(span)/len(sub(src[b],old));
 maps[i]=v=>{
  const v0=sub(v,old), along=dot(u,v0), off=sub(v0,mul(u,along));
  return add(add(target,mul(w,along*scale)),mul(rotate(u,w,off),radial));
 };
}
for(const [side,sign,upperLeg,lowerLeg,upperArm,lowerArm] of [['Left',-1,1,2,7,8],['Right',1,3,4,9,10]]) {
 const finger='Bip01 '+side[0]+' Finger11';
 limb(upperLeg,side+'Hip',side+'Knee',dst[lowerLeg].joint,.84);
 limb(lowerLeg,side+'Knee',side+'Ankle',add(dst[lowerLeg].joint,[.016*sign,-.43,.022]),.85);
 limb(upperArm,side+'Shoulder',side+'Elbow',dst[lowerArm].joint,.81);
 limb(lowerArm,side+'Elbow',finger,sub(mul(dst[lowerArm].center,2),dst[lowerArm].joint),.86);
}
function bone(name) {
 if(name==='Hips')return 0;
 if(name==='Chest'||name==='Chest2'||name==='Neck'||name.endsWith('Collar'))return 5;
 if(name==='Head'||name.includes('HeadNub'))return 6;
 const l=name.startsWith('Left')||name.includes(' L ');
 const r=name.startsWith('Right')||name.includes(' R ');
 if(!l&&!r)throw Error('Unmapped weighted bone '+name);
 if(name.endsWith('Hip'))return l?1:3;
 if(/Knee|Ankle|Toe/.test(name))return l?2:4;
 if(name.endsWith('Shoulder'))return l?7:9;
 if(/Elbow|Wrist|Finger/.test(name))return l?8:10;
 throw Error('Unmapped weighted bone '+name);
}
const n=mesh.vertexCount, ni=mesh.indexCount;
const buf=Buffer.alloc(16+n*40+ni*2);
buf.write('OFAD'); buf.writeUInt32LE(1,4); buf.writeUInt32LE(n,8); buf.writeUInt32LE(ni,12);
let off=16;
for(let v=0;v<n;v++){
 const weights=Array(11).fill(0);
 for(const {boneIndex,weight} of mesh.vertexWeights[v])weights[bone(mesh.boneBindings[boneIndex].name)]+=Math.round(255*weight);
 const ranked=weights.map((w,i)=>[i,w]).sort((a,b)=>b[1]-a[1]).slice(0,4);
 if(ranked.reduce((sum,[,w])=>sum+w,0)!==255)throw Error('Invalid prototype driver vertex weights');
 const p=mul(axis(mesh.positions[v]),.01), normal=axis(mesh.normals[v]);
 const warp=q=>ranked.reduce((point,[i,w])=>w?add(point,mul((maps[i]??body)(q),w/255)):point,[0,0,0]);
 const rest=warp(p);
 // Apply the inverse-transpose normal transform of the same local bind warp.
 const J=[0,1,2].map(i=>sub(warp(p.map((x,j)=>x+(i===j?.001:0))),rest));
 const cofactor=[cross(J[1],J[2]),cross(J[2],J[0]),cross(J[0],J[1])];
 const norm=unit(cofactor.reduce((n,c,i)=>add(n,mul(c,normal[i]*1e6)),[0,0,0]));
 for(const x of [...rest,...norm,...mesh.uvs[v]]){if(!Number.isFinite(x))throw Error('Invalid prototype vertex');buf.writeFloatLE(x,off);off+=4;}
 for(const [i] of ranked)buf[off++]=i;
 for(const [,w] of ranked)buf[off++]=w;
}
for(let i=0;i<ni;i+=3){
 // The source coordinate conversion reverses handedness; preserve winding.
 for(const j of [i,i+2,i+1]){if(mesh.indices[j]>=n)throw Error('Invalid prototype index');buf.writeUInt16LE(mesh.indices[j],off);off+=2;}
}
fs.writeFileSync(output,buf);
console.log(JSON.stringify({mesh:mesh.name,source_sha256:createHash('sha256').update(source).digest('hex'),vertices:n,triangles:ni/3,source_bindings:mesh.boneBindings.length,native_bones:11}));
