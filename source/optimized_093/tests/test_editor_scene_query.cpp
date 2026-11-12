#include "editor_scene_query.hpp"
#include "assets.hpp"
#include "water.hpp"
#include "captured_scene.hpp"
#include <chrono>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>
using namespace ofo;
namespace {
Mesh triangle(Vec a,Vec b,Vec c){Mesh m;for(auto p:{a,b,c}){Vertex v;v.position=p;m.vertices.push_back(v);}m.indices={0,1,2};return m;}
Mesh ground_face(float y=0){return triangle({-2,y,-2},{2,y,-2},{0,y,2});}
bool near(Vec a,Vec b){return length(a-b)<.0002f;}
// Independent double precision plane intersection and barycentric oracle.
std::optional<Vec> brute(const Geometry&g,Vec o,Vec d,float limit=100000){
 d=unit(d);long double best=limit;std::optional<Vec>hit;
 auto subtract=[](Vec a,Vec b){return std::array<long double,3>{(long double)a.x-b.x,(long double)a.y-b.y,(long double)a.z-b.z};};
 auto dot3=[](const std::array<long double,3>&a,const std::array<long double,3>&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
 for(const auto&m:g.meshes){if(m.prop_index>=0||m.captured||m.billboard||!m.solid||m.material>=g.materials.size())continue;const auto&mat=g.materials[m.material];if(water_material(mat)||lower(mat.name).find("shadow")!=std::string::npos)continue;
 for(size_t i=0;i+2<m.indices.size();i+=3){if(m.indices[i]>=m.vertices.size()||m.indices[i+1]>=m.vertices.size()||m.indices[i+2]>=m.vertices.size())continue;Vec a=point(m.transform,m.vertices[m.indices[i]].position),b=point(m.transform,m.vertices[m.indices[i+1]].position),c=point(m.transform,m.vertices[m.indices[i+2]].position);auto e=subtract(b,a),f=subtract(c,a),ao=subtract(a,o);std::array<long double,3>n{e[1]*f[2]-e[2]*f[1],e[2]*f[0]-e[0]*f[2],e[0]*f[1]-e[1]*f[0]},dir{d.x,d.y,d.z};long double nd=dot3(n,dir);if(nd==0)continue;long double t=dot3(n,ao)/nd;if(t<=.1f||t>best)continue;auto q=subtract(o,a);for(int k=0;k<3;k++)q[k]+=dir[k]*t;long double ee=dot3(e,e),ef=dot3(e,f),ff=dot3(f,f),qe=dot3(q,e),qf=dot3(q,f),den=ee*ff-ef*ef;if(den==0)continue;long double u=(qe*ff-qf*ef)/den,v=(qf*ee-qe*ef)/den;if(u>=0&&v>=0&&u+v<=1){best=t;hit=o+d*float(t);}}}return hit;
}

void compare(const Geometry&g,const EditorSceneQuery&q,Vec o,Vec d,float limit=100000){auto a=brute(g,o,d,limit),b=q.ray(o,d,limit);assert(bool(a)==bool(b));if(a)assert(near(*a,*b));}
}
void real_resources(const char* root){
 Resources resources(root);auto g=load_geometry(resources,"tracks/forest/forest1/geometry/track_geom.w32","tracks/forest/forest1/a");
 auto start=std::chrono::steady_clock::now();EditorSceneQuery query(g);auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 size_t rays=0,candidates=0;for(const auto&m:g.meshes){if(m.prop_index>=0||m.captured||m.billboard||!m.solid||water_material(g.materials[m.material])||lower(g.materials[m.material].name).find("shadow")!=std::string::npos||m.indices.size()<3)continue;
 auto a=point(m.transform,m.vertices[m.indices[0]].position),b=point(m.transform,m.vertices[m.indices[1]].position),c=point(m.transform,m.vertices[m.indices[2]].position);Vec center=(a+b+c)/3.f,origin=center+Vec{0,2000,0};auto expected=brute(g,origin,{0,-1,0},4000),actual=query.ray(origin,{0,-1,0},4000);assert(bool(expected)==bool(actual));if(expected&&length(*expected-*actual)>=.01f){std::cerr<<"ray="<<rays<<" mesh="<<m.name<<" origin="<<origin.x<<","<<origin.y<<","<<origin.z<<" expectedY="<<expected->y<<" actualY="<<actual->y<<" delta="<<length(*expected-*actual)<<"\n";}if(expected)assert(length(*expected-*actual)<.01f);candidates+=query.last_triangle_tests();if(++rays==128)break;}
 assert(rays==128&&query.triangle_count()>100000);assert(candidates<rays*query.triangle_count()/100);
 std::cout<<"real Forest1 BVH: triangles="<<query.triangle_count()<<" rays="<<rays<<" candidates="<<candidates<<" brute_triangle_tests="<<rays*query.triangle_count()<<" construction_host_ms="<<ms<<"\n";
}
int main(int argc,char**argv){if(argc>1)real_resources(argv[1]);
 static_assert(!std::is_copy_constructible<EditorSceneQuery>::value);static_assert(std::is_nothrow_move_constructible<EditorSceneQuery>::value);
 Geometry empty;EditorSceneQuery none(empty);assert(none.triangle_count()==0&&!none.ray({0,1,0},{0,-1,0})&&none.last_triangle_tests()==0);
 Geometry g;g.materials.push_back({"road"});g.meshes.push_back(ground_face());g.meshes.push_back(ground_face(-3));EditorSceneQuery q(g);assert(q.triangle_count()==2);compare(g,q,{0,5,0},{0,-1,0});compare(g,q,{0,-5,0},{0,1,0});compare(g,q,{0,5,0},{0,-4,0});compare(g,q,{8,5,0},{0,-1,0});compare(g,q,{0,5,0},{0,-1,0},4);compare(g,q,{0,5,0},{0,-1,0},5);compare(g,q,{0,.05f,0},{0,-1,0});
 Geometry threshold;threshold.materials=g.materials;threshold.meshes.push_back(ground_face());EditorSceneQuery thresholdQuery(threshold);assert(!thresholdQuery.ray({0,.1f,0},{0,-1,0}));assert(thresholdQuery.ray({0,.1001f,0},{0,-1,0}));
 assert(!q.ray({0,5,0},{0,0,0}));assert(!q.ray({0,5,0},{0,-1,0},-.5f));assert(!q.ray({NAN,5,0},{0,-1,0}));assert(!q.ray({0,5,0},{INFINITY,0,0}));assert(!q.ray({0,5,0},{0,-1,0},NAN));assert(q.last_triangle_tests()==0);
 Geometry transformed;transformed.materials=g.materials;auto m=ground_face();m.transform=multiply(translate({7,3,-9}),multiply(rotate_x(.4f),rotate_y(.7f)));m.transform[0]*=2;m.transform[1]*=2;m.transform[2]*=2;transformed.meshes.push_back(m);EditorSceneQuery tq(transformed);Vec center=point(m.transform,{0,0,0}),normal=unit(cross(direction(m.transform,{1,0,0}),direction(m.transform,{0,0,1})));compare(transformed,tq,center+normal*5,normal*-1);compare(transformed,tq,center-normal*5,normal);
 auto snapshot=std::move(tq);transformed.meshes.clear();assert(snapshot.ray(center+normal*5,normal*-1));assert(!tq.ray(center,normal));EditorSceneQuery assigned(empty);assigned=std::move(snapshot);assert(assigned.triangle_count()==1&&assigned.ray(center+normal*5,normal*-1));
 Geometry filtered;filtered.materials={{"road"},{"Water_surface"},{"road_SHADOW"},{"liquid"}};filtered.materials.back().shader=25;filtered.meshes.push_back(ground_face(-8));for(int k=0;k<8;++k){auto x=ground_face(float(k));if(k==0)x.prop_index=0;if(k==1)x.captured=true;if(k==2)x.billboard=true;if(k==3)x.solid=false;if(k>=4&&k<=6)x.material=size_t(k-3);if(k==7)x.material=999;filtered.meshes.push_back(x);}EditorSceneQuery fq(filtered);assert(fq.triangle_count()==1);compare(filtered,fq,{0,20,0},{0,-1,0});
 Geometry tiny;tiny.materials=g.materials;tiny.meshes.push_back(triangle({0,0,0},{.00001f,0,0},{0,0,.00001f}));auto bad=ground_face(-2);bad.indices={0,1,999,0,1,2,0};tiny.meshes.push_back(bad);tiny.meshes.push_back(triangle({0,0,0},{0,0,0},{0,0,0}));auto nan=ground_face();nan.vertices[0].position.x=NAN;tiny.meshes.push_back(nan);EditorSceneQuery small(tiny);assert(small.triangle_count()==2);auto tinyHit=small.ray({.000002f,1,.000002f},{0,-1,0});assert(tinyHit&&near(*tinyHit,{.000002f,0,.000002f}));
 Geometry large;large.materials=g.materials;for(int x=0;x<80;++x)for(int z=0;z<80;++z){auto tile=ground_face();tile.transform=translate({float(x*8),float((x+z)%3),float(z*8)});large.meshes.push_back(std::move(tile));}EditorSceneQuery bvh(large);assert(bvh.triangle_count()==6400);for(int k=0;k<20;++k){Vec o={float((k*7%80)*8)+.1f,20,float((k*13%80)*8)+.1f};compare(large,bvh,o,{0,-1,0});assert(bvh.last_triangle_tests()<bvh.triangle_count()/20);}bvh.ray({-100,20,-100},{0,-1,0});assert(bvh.last_triangle_tests()==0);
 std::cout<<"editor scene query: oracle, filters, snapshot, transforms, tiny triangles and BVH candidate reduction passed\n";
}
