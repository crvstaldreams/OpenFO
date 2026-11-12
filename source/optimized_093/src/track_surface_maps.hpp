#pragma once

#include "assets.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>

namespace ofo {
// Both backends use A8R8G8B8 words. This is linear material data:
// R/G = encoded tangent normal X/Y, B = specular level, A = roughness.
struct TrackSurfaceImage {
 int width=0,height=0;
 std::vector<uint32_t> argb;
 bool valid()const{
  return width>0&&height>0&&width<=16384&&height<=16384&&
         argb.size()==size_t(width)*size_t(height);
 }
};

struct TrackSurfaceProfile {float specular,roughness,bump;};
inline TrackSurfaceProfile track_surface_profile(const std::string& diffuse_path,const std::string& material_name){
 // Use the texture's name, not the track directory: Forest asphalt is still asphalt.
 auto name=lower(material_name+" "+fs::u8path(diffuse_path).filename().u8string());
 auto contains=[&](std::initializer_list<const char*> words){
  for(auto word:words)if(name.find(word)!=name.npos)return true;
  return false;
 };
 auto token=[&](const std::string& word){
  auto letter=[](char c){return c>='a'&&c<='z';};
  for(size_t pos=name.find(word);pos!=name.npos;pos=name.find(word,pos+word.size())){
   size_t end=pos+word.size();
   if((!pos||!letter(name[pos-1]))&&(end==name.size()||!letter(name[end])))return true;
  }
  return false;
 };
 if(token("ice")||token("icy")||token("snowice"))return{.51f,.35f,.13f};
 if(contains({"snow"}))return{.13f,.86f,.24f};
 if(contains({"rust","oxidiz"}))return{.23f,.77f,.25f};
 if(contains({"metal","steel","chrome","alumin"}))return{.43f,.49f,.20f};
 if(contains({"asphalt","tarmac","bitumen"}))return{.18f,.73f,.30f};
 if(contains({"concrete","cement","brick","rock","stone","cobble"}))return{.17f,.79f,.36f};
 if(contains({"wood","plank","bark"})||token("log"))return{.12f,.84f,.34f};
 if(contains({"sand","dirt","mud","gravel","soil","clay"}))return{.09f,.89f,.35f};
 if(contains({"grass","leaf","bush","foliage"}))return{.07f,.92f,.23f};
 if(token("road"))return{.18f,.73f,.30f};
 return{.12f,.84f,.23f};
}

namespace track_surface_detail {
inline bool lighting_map(const std::string& path){
 auto file=fs::u8path(path).filename().u8string();
 return path.find("/lighting/")!=path.npos||path.rfind("lighting/",0)==0||
        path.find("/lightmaps/")!=path.npos||path.rfind("lightmaps/",0)==0||
        file.find("lightmap")!=file.npos||file.find("light_map")!=file.npos||
        file.find("shadowmap")!=file.npos||file.find("shadow_map")!=file.npos||
        file.rfind("colormap",0)==0;
}

inline float luma(uint32_t pixel){
 return (.2126f*float((pixel>>16)&255)+.7152f*float((pixel>>8)&255)+
         .0722f*float(pixel&255))/255.f;
}
inline uint32_t unorm(float x){return uint32_t(std::lround(std::clamp(x,0.f,1.f)*255.f));}
inline int wrap(int i,int n){return (i%n+n)%n;}
template<class Channel>
inline float sample(const TrackSurfaceImage& im,float u,float v,Channel channel){
 // Texel centers and repeat semantics agree with the render sampler, including
 // when an authored companion is a different size than the diffuse tile.
 float x=u*im.width-.5f,y=v*im.height-.5f;
 int ix=int(std::floor(x)),iy=int(std::floor(y));
 float fx=x-ix,fy=y-iy;
 size_t row0=size_t(wrap(iy,im.height))*size_t(im.width),row1=size_t(wrap(iy+1,im.height))*size_t(im.width);
 size_t a=size_t(wrap(ix,im.width)),b=size_t(wrap(ix+1,im.width));
 auto top=channel(im.argb[row0+a])*(1-fx)+channel(im.argb[row0+b])*fx;
 auto bottom=channel(im.argb[row1+a])*(1-fx)+channel(im.argb[row1+b])*fx;
 return top*(1-fy)+bottom*fy;
}

inline TrackSurfaceImage sibling(const Resources&r,const std::string& key,
 const std::function<TrackSurfaceImage(const std::string&)>& decode,
 std::initializer_list<const char*> suffixes){
 if(key.empty()||!decode)return{};
 auto p=fs::u8path(key);
 auto stem=p.stem().u8string(),extension=p.extension().u8string();
 if(extension==".tga"&&stem.size()>4&&stem.compare(stem.size()-4,4,".dds")==0)stem.resize(stem.size()-4);
 // Never use Resources::texture here: its basename fallbacks can borrow an
 // unrelated track's image. A companion must sit beside its exact diffuse.
 std::vector<std::string> extensions;
 for(const auto&ext:{extension,std::string(".dds"),std::string(".tga"),std::string(".png")})
  if(!ext.empty()&&std::find(extensions.begin(),extensions.end(),ext)==extensions.end())extensions.push_back(ext);
 for(auto suffix:suffixes)for(const auto&ext:extensions){
  auto candidate=(p.parent_path()/fs::u8path(stem+suffix+ext)).generic_u8string();
  if(!r.exists(candidate))continue;
  try{auto result=decode(candidate);if(result.valid())return result;}
  catch(const std::exception&){/* optional malformed map: use a remaining candidate or fallback */}
 }
 return{};
}
}

inline TrackSurfaceImage build_track_surface_map(const Resources&r,const std::string& diffuse_path,
 const std::string& material_name,const TrackSurfaceImage& diffuse,
 const std::function<TrackSurfaceImage(const std::string&)>& decode){
 using namespace track_surface_detail;
 auto key=diffuse_path.empty()?std::string():clean_path(diffuse_path);
 if(lighting_map(key))return{};
 auto profile=track_surface_profile(key,material_name);
 auto normal=sibling(r,key,decode,{"_normal","_nrm","_normalmap","_norm"});
 auto height=normal.valid()?TrackSurfaceImage{}:sibling(r,key,decode,{"_height","_bump","_bumpmap"});
 auto spec=sibling(r,key,decode,{"_specular","_spec","_specmap"});
 auto rough=sibling(r,key,decode,{"_roughness","_rough"});
 bool invert_rough=false;
 if(!rough.valid()){
  rough=sibling(r,key,decode,{"_glossiness","_gloss"});
  invert_rough=rough.valid();
 }
 const TrackSurfaceImage* dimensions=nullptr;
 for(auto* image:std::array<const TrackSurfaceImage*,5>{&diffuse,&normal,&height,&spec,&rough})
  if(image->valid()){dimensions=image;break;}
 const bool has_normal=normal.valid(),has_height=height.valid(),has_spec=spec.valid(),has_rough=rough.valid();
 const int limit=(has_normal||has_height||has_spec||has_rough)?256:128;
 int w=1,h=1;
 if(dimensions){
  auto scale=std::min(1.f,float(limit)/std::max(dimensions->width,dimensions->height));
  w=std::clamp(int(std::lround(dimensions->width*scale)),1,limit);
  h=std::clamp(int(std::lround(dimensions->height*scale)),1,limit);
 }
 TrackSurfaceImage output{w,h,std::vector<uint32_t>(size_t(w)*h)};
 const auto* relief=has_height?&height:diffuse.valid()?&diffuse:nullptr;
 std::vector<float> heights;
 if(!has_normal&&relief){
  heights.resize(size_t(w)*h);
  for(int y=0;y<h;y++)for(int x=0;x<w;x++)
   heights[size_t(y)*w+x]=sample(*relief,(x+.5f)/w,(y+.5f)/h,luma);
 }
 auto at=[&](int x,int y){return heights[size_t(wrap(y,h))*w+size_t(wrap(x,w))];};
 auto signed_channel=[](unsigned shift){
  return [=](uint32_t p){return float((p>>shift)&255)*(2.f/255.f)-1.f;};
 };
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  float u=(x+.5f)/w,v=(y+.5f)/h,nx=0,ny=0;
  if(has_normal){
   nx=sample(normal,u,v,signed_channel(16));ny=sample(normal,u,v,signed_channel(8));
   float xy2=nx*nx+ny*ny;
   if(xy2>.999f){float scale=std::sqrt(.999f/xy2);nx*=scale;ny*=scale;}
  }else if(!heights.empty()){
   // Sobel suppresses isolated compression noise. Limit diffuse-derived slopes
   // independently of contrast so painted patterns do not become deep ridges.
   float gx=(at(x-1,y-1)+2*at(x-1,y)+at(x-1,y+1)-at(x+1,y-1)-2*at(x+1,y)-at(x+1,y+1))*.125f;
   float gy=(at(x-1,y-1)+2*at(x,y-1)+at(x+1,y-1)-at(x-1,y+1)-2*at(x,y+1)-at(x+1,y+1))*.125f;
   const float limit=has_height?.5f:.22f,strength=has_height?.9f:profile.bump*.30f;
   gx=std::clamp(gx,-limit,limit)*strength;gy=std::clamp(gy,-limit,limit)*strength;
   float scale=1/std::sqrt(1+gx*gx+gy*gy);nx=gx*scale;ny=gy*scale;
  }
  float s=has_spec?sample(spec,u,v,luma):profile.specular;
  float a=has_rough?sample(rough,u,v,luma):profile.roughness;
  if(invert_rough)a=1-a;
  output.argb[size_t(y)*w+x]=(unorm(a)<<24)|(unorm(nx*.5f+.5f)<<16)|(unorm(ny*.5f+.5f)<<8)|unorm(s);
 }
 return output;
}
}
