#include "track_surface_maps.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace ofo;

static void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static int channel(uint32_t pixel,unsigned shift){return int((pixel>>shift)&255);}
static uint32_t gray(int value){return 0xff000000u|uint32_t(value)*0x010101u;}

int main(){try{
 const auto temp=fs::temp_directory_path()/("openfo_track_surfaces_"+std::to_string(
  std::chrono::steady_clock::now().time_since_epoch().count()));
 struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{temp};
 std::map<std::string,TrackSurfaceImage> source;
 auto add=[&](const std::string& path,TrackSurfaceImage image){
  const auto file=temp/fs::u8path(path);fs::create_directories(file.parent_path());std::ofstream(file).put('x');
  source.emplace(path,std::move(image));
 };
 add("tracks/a/road_nrm.dds",{1,1,{0xffb06bff}});
 add("tracks/a/road_height.dds",{4,1,{gray(192),gray(128),gray(64),gray(128)}});
 add("tracks/a/road_spec.dds",{1,1,{gray(91)}});
 add("tracks/a/road_roughness.dds",{1,1,{gray(170)}});
 add("tracks/a/road_gloss.dds",{1,1,{gray(250)}});
 add("tracks/a/road_normal.dds",{}); // unsupported image: try the next exact sibling
 add("tracks/b/road_normal.dds",{1,1,{0xffe0e0ff}}); // never borrowed across directories
 add("tracks/a/gravel_height.dds",{4,1,{gray(192),gray(128),gray(64),gray(128)}});
 add("tracks/a/gravel_glossiness.tga",{1,1,{gray(90)}});
 add("tracks/a/lighting/lightmap1_w2_normal.dds",{1,1,{0xffb06bff}});
 const Resources resources(temp);
 int decode_calls=0;
 auto decoder=[&](const std::string& path){++decode_calls;return source.at(path);};
 TrackSurfaceImage flat{4,1,std::vector<uint32_t>(4,gray(127))};
 auto authored=build_track_surface_map(resources,"DATA\\Tracks\\A\\road.dds","road",flat,decoder);
 need(authored.width==4&&authored.height==1&&authored.argb.size()==4,"output follows diffuse UV dimensions");
 for(auto value:authored.argb)
  need(channel(value,16)==176&&channel(value,8)==107&&channel(value,0)==91&&channel(value,24)==170,
       "normal RG + authored spec + roughness; roughness takes precedence over gloss");
 const int before_lighting=decode_calls;
 auto lighting=build_track_surface_map(resources,"tracks/a/lighting/lightmap1_w2.dds","road",flat,decoder);
 need(lighting.argb.empty()&&decode_calls==before_lighting,"lightmap cannot seed or select relief maps");
 auto height=build_track_surface_map(resources,"tracks/a/gravel.dds","gravel",flat,decoder);
 need(channel(height.argb[0],16)==128&&channel(height.argb[1],16)>135&&
      channel(height.argb[2],16)==128&&channel(height.argb[3],16)<120,
      "authored height differentiates both directions across the repeating tile");
 for(auto value:height.argb)need(channel(value,24)==165&&channel(value,8)==128,"inverse gloss and neutral vertical derivative");
 TrackSurfaceImage contrast{4,1,{gray(255),gray(128),gray(0),gray(128)}};
 auto inferred=build_track_surface_map(resources,"tracks/c/road.dds","asphalt",contrast,decoder);
 need(channel(inferred.argb[1],16)>128&&channel(inferred.argb[1],16)<=135&&channel(inferred.argb[3],16)<128,
      "diffuse contrast makes restrained relief; identically named textures from other folders do not leak");
 auto ice=build_track_surface_map(resources,"tracks/a/ice.dds","ice",flat,decoder);
 need(channel(ice.argb[0],0)>channel(inferred.argb[0],0)&&channel(ice.argb[0],24)<channel(inferred.argb[0],24),
      "unmapped ice is more specular and less rough than asphalt");
 TrackSurfaceImage large{512,128,std::vector<uint32_t>(512*128,gray(90))};
 auto reduced=build_track_surface_map(resources,"tracks/a/wood.dds","plank",large,decoder);
 need(reduced.width==128&&reduced.height==32&&reduced.argb.size()==128*32,"preserve inferred tile aspect within 128x128 budget");
 for(auto value:reduced.argb)need(channel(value,16)==128&&channel(value,8)==128,"flat fill remains flat at tile boundaries");
 auto authored_large=build_track_surface_map(resources,"tracks/a/road.dds","road",large,decoder);
 need(authored_large.width==256&&authored_large.height==64&&authored_large.argb.size()==256*64,
      "retain authored detail within the 256x256 budget");
 std::cout<<"track surface packing, authored precedence, tile derivatives, resource isolation and size budget passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
