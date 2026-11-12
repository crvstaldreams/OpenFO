#include "texture_filtering070.hpp"
#include <cassert>
#include <iostream>
using namespace ofo;
int main(){
 assert(texture_anisotropy070(0,16,true,true)==2);assert(texture_anisotropy070(1,16,true,true)==4);assert(texture_anisotropy070(2,16,true,true)==8);assert(texture_anisotropy070(3,16,true,true)==16);assert(texture_anisotropy070(3,6,true,true)==6);assert(texture_anisotropy070(3,0,true,true)==1);assert(texture_anisotropy070(3,16,false,true)==1);assert(texture_anisotropy070(3,16,true,false)==16);assert(texture_anisotropy070(3,16,true,true,true)==1);
 assert(foliage_texture070("Forest/LEAVES.dds")&&!foliage_texture070("global/overlay/meters.dds"));
 std::vector<uint32_t> leaf(64*64);for(int y=0;y<64;++y)for(int x=0;x<64;++x)if(x%11==0||x==y)leaf[y*64+x]=0xff289c37;const double coverage=foliage_coverage070(leaf);auto mips=foliage_mips070(64,64,leaf);assert(mips.size()==7);size_t bytes=0;for(auto&m:mips){bytes+=m.pixels.size()*4;assert(std::abs(foliage_coverage070(m.pixels)-coverage)<=.50001/m.pixels.size());for(auto c:m.pixels)if(c>>24)assert(((c>>8)&255)>90);}assert(bytes<64*64*6);assert((mips[0].pixels[1]>>24)==0&&(mips[0].pixels[1]&0xffffff)!=0);
 for(auto c:{0u,0x00287834u,0xff287834u}){std::vector<uint32_t> p(7*3,c);auto chain=foliage_mips070(7,3,p);assert(chain.back().width==1&&chain.back().height==1);for(auto&m:chain)assert(foliage_coverage070(m.pixels)==((c>>24)>38?1:0));}
 assert(foliage_mips070(1,1,{0xffffffff}).size()==1);bool invalid=false;try{foliage_mips070(0,0,{});}catch(const std::invalid_argument&){invalid=true;}assert(invalid);invalid=false;try{foliage_mips070(2049,1,{});}catch(const std::invalid_argument&){invalid=true;}assert(invalid);assert(foliage_coverage070({})==0);
 std::cout<<"texture filtering070: anisotropy caps/UI fallback, thin foliage coverage, RGB borders, odd/degenerate/opaque/transparent mips passed\n";
}
