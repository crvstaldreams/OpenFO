"""Import a generated equirectangular panorama into shared cubemap DDS faces.
Projection/format conversion only: no painted content or lighting changes.
Usage: python build_night_sky_assets.py panorama.png profiles.json package_dir
"""
from pathlib import Path
import sys,json,hashlib
import numpy as np
from PIL import Image

def directions(face,size):
 a=np.linspace(-1,1,size,dtype=np.float32);u,v=np.meshgrid(a,a)
 one=np.ones_like(u)
 return ((u,-v,one),(-u,-v,-one),(-one,-v,u),(one,-v,-u),(u,one,v),(u,-one,-v))[face]

def project(panorama,face,yaw,size=512):
 x,y,z=directions(face,size);norm=np.sqrt(x*x+y*y+z*z)
 longitude=np.arctan2(x,z)-yaw
 uu=(longitude/(2*np.pi)+.5)%1
 vv=.5-np.arcsin(y/norm)/np.pi
 h,w=panorama.shape[:2];px=uu*w-.5;py=np.clip(vv*h-.5,0,h-1)
 ix=np.floor(px).astype(np.int32);iy=np.floor(py).astype(np.int32)
 fx=(px-ix)[...,None];fy=(py-iy)[...,None]
 a=panorama[iy,ix%w].astype(np.float32);b=panorama[iy,(ix+1)%w].astype(np.float32)
 c=panorama[np.minimum(iy+1,h-1),ix%w].astype(np.float32);d=panorama[np.minimum(iy+1,h-1),(ix+1)%w].astype(np.float32)
 return np.clip(((a*(1-fx)+b*fx)*(1-fy)+(c*(1-fx)+d*fx)*fy)+.5,0,255).astype(np.uint8)

def check_edges(faces):
 # Source UV conventions match sky_face_vertices/nativeReflection.
 pairs=[(faces[0][:,-1],faces[3][:,0]),(faces[3][:,-1],faces[1][:,0]),(faces[1][:,-1],faces[2][:,0]),(faces[2][:,-1],faces[0][:,0]),(faces[0][0],faces[4][-1]),(faces[1][0],faces[4][0,::-1]),(faces[2][0],faces[4][:,0]),(faces[3][0],faces[4][::-1,-1]),(faces[0][-1],faces[5][0]),(faces[1][-1],faces[5][-1,::-1]),(faces[2][-1],faces[5][::-1,0]),(faces[3][-1],faces[5][:,-1])]
 return max(int(np.max(np.abs(a.astype(int)-b.astype(int)))) for a,b in pairs)

if __name__=='__main__':
 source,profiles,package=map(Path,sys.argv[1:4]);image=np.asarray(Image.open(source).convert('RGBA'));rows=json.loads(profiles.read_text());report=[]
 for row in rows:
  # Generated source moon is near longitude +90deg. Match each native stage's
  # light direction plus its stable per-race phase; night lighting uses this yaw.
  yaw=row['yaw']-np.pi/2
  faces=[project(image,f,yaw) for f in range(6)];error=check_edges(faces)
  if error>1:raise RuntimeError((row['stage'],'cube seam',error))
  saved=[]
  for array,path in zip(faces,row['night']):
   target=package/'data'/path;target.parent.mkdir(parents=True,exist_ok=True)
   Image.fromarray(array).save(target,format='DDS')
   # Confirm raw DDS retains the projected 8-bit night gradient and face edges.
   assert np.array_equal(np.asarray(Image.open(target).convert('RGBA')),array)
   saved.append({'path':path,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
  report.append({**row,'faces':saved,'edge_error_8bit':error})
 dst=package/'reports/weather_070/night_sky_manifest.json';dst.parent.mkdir(parents=True,exist_ok=True);dst.write_text(json.dumps({'source':str(source.name),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'size':512,'format':'DDS A8R8G8B8, original 8-bit gradients retained','location_variants':'Four shared orientation variants of one coherent generated panorama, assigned across all races and menu','stages':report},indent=2))
 print(len(report),'race/menu night sets;',len(report)*6,'DDS; max cube edge error',max(r['edge_error_8bit'] for r in report))
