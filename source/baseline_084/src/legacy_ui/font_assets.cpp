#include "font_assets.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <regex>
#include <stdexcept>
namespace ofo {
namespace {
struct Reader {
 const Bytes&b;size_t p=0;
 uint32_t integer(size_t n){if(n>b.size()-std::min(p,b.size()))throw std::runtime_error("truncated bitmap font");uint32_t v=0;for(size_t i=0;i<n;++i)v|=uint32_t(b[p++])<<(i*8);return v;}
 float real(){uint32_t v=integer(4);float f;std::memcpy(&f,&v,4);if(!std::isfinite(f))throw std::runtime_error("non-finite bitmap font metric");return f;}
};
bool valid_uv(const FontGlyph&g){return g.u0>=0&&g.v0>=0&&g.u1>=g.u0&&g.v1>=g.v0&&g.u1<=1.00001f&&g.v1<=1.00001f;}
}
BitmapFont parse_bitmap_font(const Bytes&bytes){
 Reader r{bytes};if(r.integer(4)!=5)throw std::runtime_error("unsupported bitmap font version");
 BitmapFont f;f.line_height=r.real();f.spacing_x=r.real();f.spacing_y=r.real();f.baseline=r.real();
 if(f.line_height<=0||f.line_height>1024||f.baseline<0||f.baseline>2048)throw std::runtime_error("invalid bitmap font dimensions");
 const auto count=r.integer(4);if(!count||count>65536||count>(bytes.size()-r.p)/2)throw std::runtime_error("invalid bitmap font character map");
 f.character_map.reserve(count);for(uint32_t i=0;i<count;++i)f.character_map.push_back(uint16_t(r.integer(2)));
 const auto n=r.integer(4);if(!n||n>65535||n>(bytes.size()-r.p)/24||bytes.size()-r.p!=size_t(n)*24)throw std::runtime_error("invalid bitmap font glyph table");
 f.glyphs.reserve(n);for(uint32_t i=0;i<n;++i){FontGlyph g;g.u0=r.real();g.v0=r.real();g.u1=r.real();g.v1=r.real();g.bearing=int16_t(r.integer(2));g.width=int16_t(r.integer(2));g.advance=int16_t(r.integer(2));g.reserved=int16_t(r.integer(2));if(g.width<0||g.width>2048||g.advance<0||g.advance>2048||g.u1<g.u0||g.v1<g.v0)throw std::runtime_error("invalid bitmap font glyph metrics");f.glyphs.push_back(g);}
 for(auto id:f.character_map)if(id>=n)throw std::runtime_error("bitmap font character references missing glyph");return f;
}
const FontGlyph*BitmapFont::glyph(uint32_t codepoint)const{
 auto lookup=[&](uint32_t cp)->const FontGlyph*{if(cp==0||cp>character_map.size())return nullptr;const auto id=character_map[cp-1];if(id==0&&cp!=32)return nullptr;return &glyphs[id];};
 auto g=lookup(codepoint);if(!g)g=lookup('?');if(!g)g=lookup(' ');return g;
}
bool BitmapFont::ascii_atlas_valid()const{
 for(uint32_t cp=32;cp<127;++cp){if(cp>character_map.size()||!character_map[cp-1])continue;auto g=glyph(cp);if(!g||!valid_uv(*g))return false;}return !glyphs.empty();
}
BitmapFont load_bitmap_font(const Resources&r,FontStyle style){
 const char*names[]={"Ingame18","Ingame20","Ingame10","Ingame12","Ingame14","Ingame24","Ingame36","Title14"};
 int i=int(style);if(i<0||i>=8)throw std::runtime_error("invalid bitmap font style");
 const auto bed=text_decode(r.read("data/global/fonts/fonts.bed"));
 std::regex block(std::string("Fonts\\.")+names[i]+R"rx(\s*=\s*\{([^}]*)\})rx");std::smatch m;if(!std::regex_search(bed,m,block))throw std::runtime_error("font style missing from fonts.bed");
 const std::string section=m[1];std::regex data(R"rx(Data\s*=\s*Fonts\.DataPath\s*\.\.\s*"([^"]+)")rx"),texture(R"rx(Texture\s*=\s*Fonts\.DataPath\s*\.\.\s*"([^"]+)")rx");std::smatch d,t;
 if(!std::regex_search(section,d,data)||!std::regex_search(section,t,texture))throw std::runtime_error("invalid fonts.bed references");
 std::regex directory(R"rx(DataPath\s*=\s*"([^"]+)")rx");std::smatch path;if(!std::regex_search(bed,path,directory))throw std::runtime_error("font data path missing");
 const auto base=ui_canonical(path[1]);auto f=parse_bitmap_font(r.read(base+"/"+d[1].str()));f.texture=ui_canonical(base+"/"+t[1].str());if(!r.exists(f.texture))f.texture=fs::path(f.texture).replace_extension(".dds").generic_string();return f;
}
std::vector<uint32_t> font_codepoints(const std::string&s){
 std::vector<uint32_t> out;out.reserve(s.size());
 for(size_t i=0;i<s.size();){const unsigned char a=s[i];if(a<128){out.push_back(a);++i;continue;}int n=a>=0xc2&&a<=0xdf?2:a>=0xe0&&a<=0xef?3:a>=0xf0&&a<=0xf4?4:0;uint32_t cp=n?a&((1u<<(7-n))-1):0;bool valid=n&&i+size_t(n)<=s.size();
  if(valid)for(int j=1;j<n;++j){unsigned char c=s[i+j];if((c&0xc0)!=0x80){valid=false;break;}cp=(cp<<6)|(c&63);}
  if(valid&&(cp<(n==2?128u:n==3?2048u:65536u)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)))valid=false;
  out.push_back(valid?cp:uint32_t('?'));i+=valid?n:1;
 }return out;
}
float bitmap_text_width(const BitmapFont&f,const std::string&s,float scale){float x=0,longest=0;for(auto cp:font_codepoints(s)){if(cp=='\n'){longest=std::max(longest,x);x=0;}else if(cp=='\t'){auto g=f.glyph(' ');if(g)x+=4*g->advance;}else if(cp!='\r'){auto g=f.glyph(cp);if(g)x+=g->advance;}}return std::max(longest,x)*scale;}
}
