#include "png_writer.hpp"
#include <algorithm>
#include <stdexcept>
namespace ofo {
namespace {
using Bytes=std::vector<uint8_t>;
void big_endian(Bytes& out,uint32_t n){out.push_back(uint8_t(n>>24));out.push_back(uint8_t(n>>16));out.push_back(uint8_t(n>>8));out.push_back(uint8_t(n));}
uint32_t crc32(const Bytes& b,size_t start){uint32_t crc=~0u;for(size_t i=start;i<b.size();++i){crc^=b[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}return ~crc;}
void chunk(Bytes& out,const char* type,const Bytes& data){big_endian(out,uint32_t(data.size()));size_t begin=out.size();out.insert(out.end(),type,type+4);out.insert(out.end(),data.begin(),data.end());big_endian(out,crc32(out,begin));}
}
std::vector<uint8_t> encode_png(uint32_t width,uint32_t height,const std::vector<uint32_t>& argb){
 if(!width||!height||width>8192||height>8192||argb.size()!=size_t(width)*height)throw std::invalid_argument("Invalid screenshot dimensions or pixel count");
 const size_t stride=size_t(width)*3+1,raw_size=stride*height;
 // Emit uncompressed DEFLATE blocks directly; only one scanline is assembled at a time.
 Bytes z;z.reserve(raw_size+6+5*((raw_size+65534)/65535));z.push_back(0x78);z.push_back(0x01);
 uint32_t adler_a=1,adler_b=0;size_t emitted=0;
 while(emitted<raw_size){uint16_t count=uint16_t(std::min(size_t(65535),raw_size-emitted));z.push_back(emitted+count==raw_size?1:0);z.push_back(uint8_t(count));z.push_back(uint8_t(count>>8));uint16_t inverse=uint16_t(~count);z.push_back(uint8_t(inverse));z.push_back(uint8_t(inverse>>8));
  for(size_t j=0;j<count;++j){size_t at=emitted+j,row=at/stride,col=at%stride;uint8_t byte=0;if(col){size_t channel=(col-1)%3,pixel=(col-1)/3;uint32_t value=argb[row*width+pixel];byte=uint8_t(value>>(16-8*channel));}z.push_back(byte);adler_a=(adler_a+byte)%65521;adler_b=(adler_b+adler_a)%65521;}emitted+=count;
 }
 big_endian(z,(adler_b<<16)|adler_a);
 Bytes out={137,80,78,71,13,10,26,10};out.reserve(z.size()+57);Bytes header;big_endian(header,width);big_endian(header,height);header.insert(header.end(),{8,2,0,0,0});chunk(out,"IHDR",header);chunk(out,"IDAT",z);chunk(out,"IEND",{});return out;
}
}
