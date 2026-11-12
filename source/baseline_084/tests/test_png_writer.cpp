#include "png_writer.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace ofo;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static uint32_t be(const std::vector<uint8_t>& b,size_t p){return(uint32_t(b[p])<<24)|(uint32_t(b[p+1])<<16)|(uint32_t(b[p+2])<<8)|b[p+3];}
static uint32_t crc(const std::vector<uint8_t>& b,size_t begin,size_t end){uint32_t c=~0u;for(size_t p=begin;p<end;++p){c^=b[p];for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);}return~c;}
int main(){try{
 auto b=encode_png(2,2,{0xff123456u,0x80778899u,0x00112233u,0xffffffffu});
 need(b.size()>50&&b[0]==137&&b[1]=='P'&&b[2]=='N'&&b[3]=='G',"PNG signature");std::vector<uint8_t> z;bool ih=false,done=false;
 for(size_t p=8;p<b.size();){auto n=be(b,p);need(p+n+12<=b.size(),"chunk bounds");need(crc(b,p+4,p+8+n)==be(b,p+8+n),"chunk CRC");std::string type(b.begin()+p+4,b.begin()+p+8);if(type=="IHDR"){ih=true;need(n==13&&be(b,p+8)==2&&be(b,p+12)==2&&b[p+16]==8&&b[p+17]==2,"RGB8 size");}if(type=="IDAT")z.insert(z.end(),b.begin()+p+8,b.begin()+p+8+n);if(type=="IEND")done=true;p+=n+12;}
 need(ih&&done&&z.size()>6&&z[0]==0x78,"PNG required chunks");std::vector<uint8_t> raw;size_t p=2;bool final=false;while(!final){auto head=z[p++];final=(head&1)!=0;need((head&6)==0,"stored zlib block");uint16_t n=z[p]|(z[p+1]<<8),inverse=z[p+2]|(z[p+3]<<8);p+=4;need(uint16_t(~n)==inverse&&p+n<=z.size()-4,"stored length");raw.insert(raw.end(),z.begin()+p,z.begin()+p+n);p+=n;}
 const std::vector<uint8_t> expected={0,0x12,0x34,0x56,0x77,0x88,0x99,0,0x11,0x22,0x33,0xff,0xff,0xff};need(raw==expected,"topdown ARGB pixels become lossless RGB scanlines");uint32_t a=1,d=0;for(auto c:raw){a=(a+c)%65521;d=(d+a)%65521;}need(be(z,p)==((d<<16)|a),"zlib Adler checksum");
 for(auto size:std::vector<std::pair<uint32_t,uint32_t>>{{0,1},{1,0},{8193,1},{1,8193},{2,2}}){bool threw=false;try{encode_png(size.first,size.second,{});}catch(const std::invalid_argument&){threw=true;}need(threw,"invalid screenshot is rejected");}
 auto big=encode_png(257,100,std::vector<uint32_t>(25700,0xffabcdef));need(big.size()>65535,"supports multiple zlib blocks");
 std::cout<<"PNG chunks, checksums, pixels and validation passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
