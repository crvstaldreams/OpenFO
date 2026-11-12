#include "mesh_lod.hpp"
#include <iostream>
#include <stdexcept>
#include <set>
#include <limits>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 std::vector<Vertex>v;std::vector<uint32_t>ix;for(int y=0;y<=40;y++)for(int x=0;x<=40;x++){Vertex a;a.position={x*.25f,0,y*.25f};a.normal={0,1,0};a.u=x/40.f;a.v=y/40.f;v.push_back(a);}for(int y=0;y<40;y++)for(int x=0;x<40;x++){uint32_t a=y*41+x;for(auto n:{a,a+41,a+1,a+1,a+41,a+42})ix.push_back(n);}
 auto lod=build_mesh_lods(v,ix);need(lod.indices[0]==ix,"base geometry exact");need(lod.indices[1].size()<ix.size()*3/4,"medium LOD must actually reduce triangles");need(lod.indices[2].size()<lod.indices[1].size(),"far LOD must reduce further");
 for(int l=1;l<3;l++){need(lod.error[l]>0&&lod.error[l]>=lod.error[l-1],"measured conservative monotone error");for(size_t i=0;i<lod.indices[l].size();i+=3){auto a=lod.indices[l][i],b=lod.indices[l][i+1],c=lod.indices[l][i+2];need(a<v.size()&&b<v.size()&&c<v.size(),"indices in range");need(cross(v[b].position-v[a].position,v[c].position-v[a].position).y>0,"no flipped or degenerate faces");}std::set<uint32_t>used(lod.indices[l].begin(),lod.indices[l].end());for(uint32_t i=0;i<v.size();i++)if(v[i].position.x==0||v[i].position.x==10||v[i].position.z==0||v[i].position.z==10)need(used.count(i),"open boundaries remain exact, avoid cracks across batches");}
 auto seam=v;auto seamIx=ix;for(int i=0;i<=40;i++){auto a=v[i*41+20];a.u+=1;seam.push_back(a);seamIx.push_back(uint32_t(seam.size()-1));seamIx.push_back(i*41+19);seamIx.push_back(i*41+21);}auto seamLod=build_mesh_lods(seam,seamIx);for(int l=1;l<3;l++){std::set<uint32_t>used(seamLod.indices[l].begin(),seamLod.indices[l].end());for(int i=0;i<=40;i++)need(used.count(i*41+20),"texture seam positions stay pinned");}
 auto small=build_mesh_lods({v[0],v[41],v[1]},{0,1,2});need(small.indices[2].size()==3,"thin/small mesh is never removed");bool rejected=false;try{build_mesh_lods(v,{0,1,99999});}catch(const std::exception&){rejected=true;}need(rejected,"malformed index rejected");
 need(choose_mesh_lod(lod.error,1,10,720,1,0)==0,"near geometry retains full detail");need(choose_mesh_lod(lod.error,1,1000,720,1,0)>0,"far geometry uses generated LOD");need(choose_mesh_lod(lod.error,1,0,720,1,2)==0,"inside bounds full detail");need(choose_mesh_lod({0,.1f,.2f},1,44,1440,1,0)==0,"hysteresis keeps current full level");need(choose_mesh_lod({0,.1f,.2f},1,44,1440,1,1)==1,"hysteresis keeps current reduced level");
 Mesh source;source.vertices=v;source.indices=ix;std::vector<const Mesh*>jobs(8,&source);auto serial=build_mesh_lods_many(jobs,1),parallel=build_mesh_lods_many(jobs,4);for(size_t i=0;i<jobs.size();++i)need(serial[i].indices==parallel[i].indices&&serial[i].error==parallel[i].error,"parallel LOD exactly matches serial output order and errors");
 bool null_rejected=false;try{build_mesh_lods_many({nullptr});}catch(const std::invalid_argument&){null_rejected=true;}need(null_rejected,"null preprocessing input rejected");
 auto invalid=v;invalid[0].u=std::numeric_limits<float>::quiet_NaN();rejected=false;try{build_mesh_lods(invalid,ix);}catch(const std::exception&){rejected=true;}need(rejected,"nonfinite UV rejected before simplifier");
 std::cout<<"LOD: "<<ix.size()/3<<" -> "<<lod.indices[1].size()/3<<" -> "<<lod.indices[2].size()/3<<" triangles; bounds/seams/winding/error/policy passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
