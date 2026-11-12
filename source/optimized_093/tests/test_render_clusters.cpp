#include "render_clusters.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
using namespace ofo;
static void need(bool value,const char* msg){if(!value)throw std::runtime_error(msg);}
static Mesh grid(){
 Mesh m;m.name="long_track_wall";m.material=0;m.source_surface=72;m.captured=true;
 m.solid=true;m.transform=multiply(translate({0,0,150}),rotate_y(.035f));
 const int nx=192,ny=32;
 for(int y=0;y<=ny;++y)for(int x=0;x<=nx;++x){
  Vertex v;v.position={float(x*5-480),float(y*2-32),0};
  v.normal={0,0,1};v.u=float(x)/nx;v.v=float(y)/ny;v.lu=.5f;v.lv=.75f;
  v.color=uint32_t(m.vertices.size()+100);m.vertices.push_back(v);
 }
 for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
  const auto a=uint32_t(y*(nx+1)+x),b=a+1,c=a+nx+1,d=c+1;
  m.indices.insert(m.indices.end(),{a,b,d,a,d,c});
 }
 m.center={0,0,150};m.radius=600;
 return m;
}
static bool inside(Vec eye,Vec p){
 // D3D and OpenGL backend share this exact world-space culling convention;
 // this is a hand-checked narrow view for the preservation test.
 const Vec d=p-eye;
 return d.z>0&&std::abs(d.x)<d.z*.8f&&std::abs(d.y)<d.z*.8f;
}
int main(){try{
 const auto original=grid();Material mat;mat.shader=0;
 const auto result=build_render_clusters(original,mat);
 need(result.meshes.size()>2&&result.meshes.size()<=64,"large static world mesh split into bounded clusters");
 need(result.output_triangles==original.indices.size()/3,"no triangle dropped or duplicated");
 std::map<std::array<uint32_t,3>,int> triangles;
 for(size_t i=0;i<original.indices.size();i+=3)
  ++triangles[{original.vertices[original.indices[i]].color,original.vertices[original.indices[i+1]].color,original.vertices[original.indices[i+2]].color}];
 size_t total=0,rejected=0;
 for(const auto& packet:result.meshes){
  need(packet.material==original.material&&packet.source_surface==72&&packet.captured&&
       packet.transform==original.transform&&!packet.solid&&packet.prop_index==-1,
       "render-only cluster retains exact material/transform, not physics flags");
  need(packet.vertices.size()<=3*packet.indices.size()/3,"no more than three unique vertices per triangle");
  for(const auto& v:packet.vertices){
   Vec p=point(packet.transform,v.position);
   need(length(p-packet.center)<=packet.radius+.002f,"world-space sphere bounds all vertices");
   const auto index=v.color-100;
   need(index<original.vertices.size(),"source-vertex ID retained");
   const auto& s=original.vertices[index];
   need(v.position.x==s.position.x&&v.position.y==s.position.y&&v.position.z==s.position.z&&
        v.normal.z==s.normal.z&&v.u==s.u&&v.v==s.v&&v.lu==s.lu&&v.lv==s.lv,
        "attributes, lightmap and UVs exact");
  }
  for(size_t i=0;i<packet.indices.size();i+=3){
   std::array<uint32_t,3> key{};
   for(int k=0;k<3;++k){
    const auto at=packet.indices[i+k];need(at<packet.vertices.size(),"index in cluster range");key[k]=packet.vertices[at].color;
   }
   need(triangles[key]-->0,"each triangle copied only once with original winding");
  }
  total+=packet.indices.size();
  // Conservative sphere must not reject any potentially visible vertex.
  if(!inside({0,0,0},packet.center)&&length(packet.center)>packet.radius+400)++rejected;
 }
 need(total==original.indices.size(),"original index count conserved");
 for(auto [key,count]:triangles)need(count==0,"all source triangles preserved");
 need(rejected>0,"cluster bounds permit rejection of distant side geometry");
 need(original.solid&&original.indices.size()==192*32*6,"source collision mesh unchanged");
 auto moving=original;moving.prop_index=3;need(build_render_clusters(moving,mat).meshes.empty(),"moving props never clustered");
 auto billboard=original;billboard.billboard=true;need(build_render_clusters(billboard,mat).meshes.empty(),"native billboards never clustered");
 auto unsafe=mat;unsafe.alpha=1;need(build_render_clusters(original,unsafe).meshes.empty(),"transparent geometry is left untouched");
 unsafe=mat;unsafe.name="glass window";need(build_render_clusters(original,unsafe).meshes.empty(),"glass render ordering not altered");
 unsafe=mat;unsafe.shader=25;need(build_render_clusters(original,unsafe).meshes.empty(),"water shader not clustered");
 auto broken=original;broken.indices.back()=uint32_t(broken.vertices.size()+1);need(build_render_clusters(broken,mat).meshes.empty(),"bad index leaves source intact");
 auto tiny=grid();for(auto& v:tiny.vertices)v.position.x*=.01f;tiny.radius=20;
 need(build_render_clusters(tiny,mat).meshes.empty(),"compact static meshes avoid extra draw calls");
 auto low_vram=RenderClusterOptions{};low_vram.max_vertex_expansion=1.f;
 need(build_render_clusters(original,mat,low_vram).meshes.empty(),"VRAM expansion guard can reject clusters");
 std::cout<<"render clusters PASS: "<<result.meshes.size()<<" clusters for "<<total/3
          <<" exact source triangles, plus preservation and fallback tests\n";
 }catch(const std::exception& e){std::cerr<<"render cluster FAIL: "<<e.what()<<'\n';return 1;}
}
