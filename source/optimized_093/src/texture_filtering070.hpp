#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstddef>
#include <utility>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
namespace ofo {
inline unsigned texture_anisotropy070(int detail,unsigned maximum,bool min_supported,bool /*mag_supported*/,bool ui=false){
 static constexpr unsigned desired[]={2,4,8,16};
 return ui||!min_supported?1:std::max(1u,std::min(desired[std::clamp(detail,0,3)],maximum));
}
inline bool foliage_texture070(std::string name){for(auto&c:name)c=char(std::tolower(static_cast<unsigned char>(c)));for(auto token:{"leaf","leaves","grass","plant","foliage","bush","fern","tree","vegetation"})if(name.find(token)!=std::string::npos)return true;return false;}
struct FoliageMip070 {int width=0,height=0;std::vector<uint32_t> pixels;};
inline double foliage_coverage070(const std::vector<uint32_t>&p){if(p.empty())return 0;size_t n=0;for(auto c:p)n+=(c>>24)>38;return double(n)/p.size();}
// Native A8R8G8B8 pixels, native alpha-test threshold 38/255. Only foliage
// texture variants use this; UI atlases and ordinary compressed DDS stay native.
inline std::vector<FoliageMip070> foliage_mips070(int width,int height,const std::vector<uint32_t>&pixels){
 if(width<1||height<1||width>2048||height>2048||pixels.size()!=size_t(width)*height)throw std::invalid_argument("Foliage mip dimensions");
 std::vector<FoliageMip070> out;out.push_back({width,height,pixels});const double target=foliage_coverage070(pixels);
 // Extend dark, transparent borders with neighboring leaf colors. Preserve any
 // authored transparent RGB and every original alpha value.
 auto&base=out[0];for(int pass=0;pass<2;++pass){auto previous=base.pixels;for(int y=0;y<height;++y)for(int x=0;x<width;++x){size_t at=size_t(y)*width+x;if(previous[at]!=0)continue;unsigned r=0,g=0,b=0,n=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int sx=x+dx,sy=y+dy;if(sx<0||sy<0||sx>=width||sy>=height)continue;auto c=previous[size_t(sy)*width+sx];if(c&0xffffff){r+=(c>>16)&255;g+=(c>>8)&255;b+=c&255;++n;}}if(n)base.pixels[at]=((r/n)<<16)|((g/n)<<8)|(b/n);}}
 while(width>1||height>1){const auto&src=out.back();int w=std::max(1,width/2),h=std::max(1,height/2);FoliageMip070 dst{w,h,std::vector<uint32_t>(size_t(w)*h)};
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){uint64_t a=0,r=0,g=0,b=0,plain[3]{};unsigned n=0;int x0=x*width/w,x1=(x+1)*width/w,y0=y*height/h,y1=(y+1)*height/h;
   for(int sy=y0;sy<y1;++sy)for(int sx=x0;sx<x1;++sx){auto c=src.pixels[size_t(sy)*width+sx];unsigned alpha=c>>24;a+=alpha;r+=((c>>16)&255)*alpha;g+=((c>>8)&255)*alpha;b+=(c&255)*alpha;plain[0]+=(c>>16)&255;plain[1]+=(c>>8)&255;plain[2]+=c&255;++n;}
   dst.pixels[size_t(y)*w+x]=(uint32_t((a+n/2)/n)<<24)|(uint32_t(a?(r+a/2)/a:plain[0]/n)<<16)|(uint32_t(a?(g+a/2)/a:plain[1]/n)<<8)|uint32_t(a?(b+a/2)/a:plain[2]/n);
  }
  // Preserve silhouette area to the nearest representable texel. Deterministic
  // tie breaking handles thin, identical blades that scalar alpha scaling cannot.
  size_t wanted=size_t(std::llround(target*dst.pixels.size()));std::vector<size_t> order(dst.pixels.size());std::iota(order.begin(),order.end(),0);
  auto hash=[](size_t i){uint32_t v=uint32_t(i);v^=v>>16;v*=0x7feb352d;v^=v>>15;return v;};
  std::sort(order.begin(),order.end(),[&](size_t a,size_t b){unsigned aa=dst.pixels[a]>>24,bb=dst.pixels[b]>>24;return aa!=bb?aa>bb:hash(a)!=hash(b)?hash(a)<hash(b):a<b;});
  float scale=1;if(wanted&&wanted<=order.size()){unsigned pivot=dst.pixels[order[wanted-1]]>>24;if(pivot)scale=std::max(1.f,std::min(16.f,39.f/pivot));}
  for(size_t rank=0;rank<order.size();++rank){auto&c=dst.pixels[order[rank]];unsigned a=unsigned(std::clamp(std::lround((c>>24)*scale),0l,255l));a=rank<wanted?std::max(a,39u):std::min(a,38u);c=(c&0xffffff)|(a<<24);}
  out.push_back(std::move(dst));width=w;height=h;
 }
 return out;
}
}
