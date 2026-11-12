#pragma once
#include "assets.hpp"
#include <cstring>
#include <stdexcept>
namespace ofo {
inline std::vector<unsigned> font_utf8(const std::string&s){std::vector<unsigned>out;for(size_t i=0;i<s.size();){unsigned char a=s[i];if(a<128){out.push_back(a);++i;continue;}int n=a>=0xc2&&a<=0xdf?2:a>=0xe0&&a<=0xef?3:a>=0xf0&&a<=0xf4?4:0;unsigned cp=n?a&((1u<<(7-n))-1):0;bool ok=n&&i+size_t(n)<=s.size();if(ok)for(int j=1;j<n;j++){unsigned char c=s[i+j];if((c&0xc0)!=0x80){ok=false;break;}cp=(cp<<6)|(c&63);}if(ok&&(cp<(n==2?128u:n==3?2048u:65536u)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)))ok=false;out.push_back(ok?cp:'?');i+=ok?n:1;}return out;}
struct Glyph {float u=0,v=0,u1=0,v1=0;int16_t left=0,width=0,advance=0,reserved=0;};
struct FontData {
 float height=0;std::vector<uint16_t> lookup;std::vector<Glyph> glyphs;
 const Glyph* glyph(unsigned code)const {auto find=[&](unsigned c)->const Glyph*{if(!c||c>lookup.size())return nullptr;auto id=lookup[c-1];return id<glyphs.size()?&glyphs[id]:nullptr;};auto g=find(code);if(!g)g=find('?');return g;}
};
inline FontData parse_font(const Bytes& b){
 auto u32=[&](size_t off){if(off>b.size()||b.size()-off<4)throw std::runtime_error("ABC header truncated");uint32_t n;std::memcpy(&n,b.data()+off,4);return n;};
 if(u32(0)!=5)throw std::runtime_error("ABC font version");FontData f;if(b.size()<24)throw std::runtime_error("ABC header truncated");std::memcpy(&f.height,b.data()+4,4);if(!std::isfinite(f.height)||f.height<=0||f.height>1024)throw std::runtime_error("ABC font height invalid");auto count=u32(20);if(!count||count>65536||count>(b.size()-24)/2)throw std::runtime_error("ABC character table invalid");f.lookup.resize(count);std::memcpy(f.lookup.data(),b.data()+24,count*2);size_t off=24+count*2;auto n=u32(off);off+=4;if(!n||n>65536||n>(b.size()-off)/24||b.size()-off!=size_t(n)*24)throw std::runtime_error("ABC glyph span invalid");f.glyphs.resize(n);static_assert(sizeof(Glyph)==24);std::memcpy(f.glyphs.data(),b.data()+off,n*24);
 for(auto id:f.lookup)if(id>=n)throw std::runtime_error("ABC glyph reference invalid");for(auto&g:f.glyphs)if(!std::isfinite(g.u)||!std::isfinite(g.v)||!std::isfinite(g.u1)||!std::isfinite(g.v1)||g.u1<g.u||g.v1<g.v||g.width<0||g.advance<0)throw std::runtime_error("ABC glyph metrics invalid");return f;
}
}
