#include "minimap_composite.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace ofo;
namespace {
void word(Bytes&b,int at,uint32_t v,int n=4){for(int i=0;i<n;i++)b.at(at+i)=uint8_t(v>>(8*i));}
Bytes tga(int w,int h,std::vector<uint32_t>p,bool top=true){Bytes b(18+w*h*4);b[2]=2;word(b,12,w,2);word(b,14,h,2);b[16]=32;b[17]=top?0x28:8;for(int i=0;i<w*h;i++)word(b,18+i*4,p[i]);return b;}
Bytes dds(const char*type){Bytes b(144);word(b,0,0x20534444);word(b,4,124);word(b,12,4);word(b,16,4);word(b,76,32);word(b,80,4);for(int i=0;i<4;i++)b[84+i]=type[i];return b;}
void write(const fs::path&p,const Bytes&b){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write((const char*)b.data(),b.size());}
Mesh floor_mesh(float y,float u){Mesh m;m.vertices={{{0,y,10},{0,1,0},0xffffffff,u,0,1,1},{{10,y,10},{0,1,0},0xffffffff,u,0,1,1},{{0,y,0},{0,1,0},0xffffffff,u,0,1,1},{{10,y,0},{0,1,0},0xffffffff,u,0,1,1}};m.indices={0,1,2,2,1,3};return m;}
}
int main(int argc,char**argv){
 auto image=decode_minimap_image(tga(1,2,{0xffff0000,0xff00ff00},false));assert(image.pixels.size()==2);assert(image.pixels[0]==0xff00ff00&&image.pixels[1]==0xffff0000);
 auto d=dds("DXT1");word(d,128,0xf800,2);word(d,130,0x07e0,2);assert(decode_minimap_image(d).pixels[0]==0xffff0000);
 d=dds("DXT3");for(int i=128;i<136;i++)d[i]=0x88;word(d,136,0x001f,2);assert(decode_minimap_image(d).pixels[0]==0x880000ff);
 d=dds("DXT5");d[128]=200;d[129]=100;word(d,136,0x07e0,2);assert(decode_minimap_image(d).pixels[0]==0xc800ff00);
 d=dds("    ");word(d,80,0x41);word(d,88,32);word(d,92,0xff0000);word(d,96,0xff00);word(d,100,0xff);word(d,104,0xff000000);d.resize(192);word(d,128,0x80402010);assert(decode_minimap_image(d).pixels[0]==0x80402010);
 bool rejected=false;try{decode_minimap_image(Bytes(4));}catch(...){rejected=true;}assert(rejected);
 auto temp=fs::temp_directory_path()/(argc>1?"openfo_minimap_composite_real_test":"openfo_minimap_composite_test");fs::remove_all(temp);
 write(temp/"stage/lighting/lightmap1_w2.dds",tga(2,2,{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff}));
 write(temp/"scheme.tga",tga(2,2,{0,0,0,0}));Resources r(temp);Geometry g;g.source="world.w32";g.materials.push_back({"ground",{"colormap.tga","grass.dds",""},0,0});g.meshes.push_back(floor_mesh(0,0));
 Json config={{"MapTexture","scheme.tga"},{"WorldTopLeft",{{"1",0},{"2",10}}},{"WorldBottomRight",{{"1",10},{"2",0}}}};
 auto map=build_minimap(r,g,"stage",config,32);assert(map.width==32&&map.height==32);assert((map.pixels[0]>>24)==0);assert((map.pixels[16*32+16]>>24)==255);assert((map.pixels[1*32+16]>>24)>0&&(map.pixels[1*32+16]>>24)<255);assert(map.diagnostics.at("rasterized_triangles")==2);assert((map.pixels[4*32+4]&0xff0000)>(map.pixels[4*32+4]&0xff));
 // UV0 interpolation and positive-world-Z at the top must survive projection.
 for(auto&v:g.meshes[0].vertices){v.u=v.position.x/10;v.v=(10-v.position.z)/10;}
 map=build_minimap(r,g,"stage",config,32);assert(((map.pixels[4*32+4]>>16)&255)>(map.pixels[4*32+4]&255));assert((map.pixels[27*32+4]&255)>((map.pixels[27*32+4]>>16)&255));
 // Topmost surface wins; captured instances and props never cover terrain.
 g.meshes.push_back(floor_mesh(5,1));g.meshes.push_back(floor_mesh(10,0));g.meshes.back().captured=true;
 map=build_minimap(r,g,"stage",config,32);assert(((map.pixels[4*32+4]>>8)&255)>((map.pixels[4*32+4]>>16)&255));
 g.meshes[1].prop_index=0;g.meshes[1].captured=false;map=build_minimap(r,g,"stage",config,32);assert(((map.pixels[4*32+4]>>16)&255)>((map.pixels[4*32+4]>>8)&255));
 // Terrain is gently filtered before the crisp original route is overlaid.
 g.meshes.resize(1);std::vector<uint32_t> impulse(32*32,0xff000000);impulse[16*32+16]=0xffff0000;write(temp/"stage/lighting/lightmap1_w2.dds",tga(32,32,impulse));Resources filtered(temp);map=build_minimap(filtered,g,"stage",config,32);assert(((map.pixels[16*32+17]>>16)&255)>20);
 // A missing atlas preserves the complete original schematic (including alpha).
 write(temp/"scheme2.tga",tga(1,1,{0x80abcdef}));Resources fallback(temp);config["MapTexture"]="scheme2.tga";map=build_minimap(fallback,g,"missing",config,32);assert((map.pixels[0]>>24)==0);assert(map.pixels[16*32+16]==0x80abcdef);assert(map.diagnostics.at("mode")=="schematic_fallback");
 fs::remove_all(temp);
 if(argc>1){Resources real(argv[1]);auto stage=std::string("tracks/forest/forest1/a");auto world=load_geometry(real,stage_geometry(stage),stage);auto composed=build_minimap(real,world,stage,bed_file(real,stage+"/data/map.bed"));assert(composed.diagnostics.at("rasterized_triangles").get<int>()>100);assert(composed.diagnostics.at("covered_pixels").get<int>()>100000);// Compare authored road triangle centroids to the original schematic route.
 auto map_config=bed_file(real,stage+"/data/map.bed");auto scheme=decode_minimap_image(real.read(real.texture(world.source,map_config["MapTexture"])));std::vector<Vec> route;
 for(int y=0;y<scheme.height;y++)for(int x=0;x<scheme.width;x++){auto c=scheme.pixels[size_t(y)*scheme.width+x];if((c&255)>180&&((c>>24)&255)>180)route.push_back({float(x),0,float(y)});}
 size_t roads=0,aligned=0;double distance_sum=0;for(const auto&m:world.meshes)if(world.materials[m.material].name.find("road_medium")!=std::string::npos)for(size_t k=0;k+2<m.indices.size();k+=3){Vec p{};for(int i=0;i<3;i++)p+=point(m.transform,m.vertices[m.indices[k+i]].position);p=p/3;float x=(p.x-map_config["WorldTopLeft"]["1"].get<float>())/(map_config["WorldBottomRight"]["1"].get<float>()-map_config["WorldTopLeft"]["1"].get<float>())*scheme.width,y=(p.z-map_config["WorldTopLeft"]["2"].get<float>())/(map_config["WorldBottomRight"]["2"].get<float>()-map_config["WorldTopLeft"]["2"].get<float>())*scheme.height;float nearest=1e10f;for(auto q:route)nearest=std::min(nearest,(q.x-x)*(q.x-x)+(q.z-y)*(q.z-y));aligned+=nearest<9;distance_sum+=std::sqrt(nearest);++roads;}
 assert(roads>1000&&aligned>roads*.65&&distance_sum/roads<5);
 std::cout<<composed.diagnostics.dump(2)<<'\n';}
 std::cout<<"minimap composite passed\n";
}
