#pragma once
#include "legacy_ui/font_assets.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ofo {
inline int ui_font_pixels(FontStyle s){switch(s){case FontStyle::Regular20:return 20;case FontStyle::Regular10:return 10;case FontStyle::Condensed12:return 12;case FontStyle::Italic14:return 14;case FontStyle::Italic24:case FontStyle::Navigation:return 24;case FontStyle::Italic36:return 36;default:return 18;}}
inline bool ui_font_bold(FontStyle s){return s==FontStyle::Italic24||s==FontStyle::Italic36||s==FontStyle::Navigation;}
#ifdef _WIN32
inline std::wstring ui_wide(const std::string&s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(size_t(std::max(0,n)),L' ');if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
// Private process font installation: never modifies the user's Windows fonts.
class UiTrueType {
 HDC dc_=nullptr;std::map<std::pair<int,bool>,HFONT>fonts_;std::vector<fs::path>files_;std::wstring actual_=L"Segoe UI";
public:
 UiTrueType(){wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);auto dir=fs::path(exe).parent_path();for(auto folder:{dir/"fonts",dir.parent_path()/"fonts",fs::current_path()/"fonts"}){for(auto name:{"Play-Regular.ttf","Play-Bold.ttf"}){auto path=folder/name;if(fs::exists(path)&&AddFontResourceExW(path.c_str(),FR_PRIVATE,nullptr)>0)files_.push_back(path);}if(!files_.empty())break;}if(!files_.empty())actual_=L"Play";dc_=CreateCompatibleDC(nullptr);if(!dc_)throw std::runtime_error("TrueType GDI DC unavailable");SetBkMode(dc_,TRANSPARENT);SetTextColor(dc_,RGB(255,255,255));SetTextAlign(dc_,TA_LEFT|TA_TOP);select(18,false);wchar_t selected[LF_FACESIZE]{};if(GetTextFaceW(dc_,LF_FACESIZE,selected))actual_=selected;}
 ~UiTrueType(){if(dc_)DeleteDC(dc_);for(auto&f:fonts_)DeleteObject(f.second);for(auto&p:files_)RemoveFontResourceExW(p.c_str(),FR_PRIVATE,nullptr);}
 UiTrueType(const UiTrueType&)=delete;UiTrueType&operator=(const UiTrueType&)=delete;
 HDC select(int pixels,bool bold){pixels=std::clamp(pixels,6,256);auto key=std::make_pair(pixels,bold);auto i=fonts_.find(key);if(i==fonts_.end()){auto f=CreateFontW(-pixels,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,actual_.c_str());if(!f)throw std::runtime_error("TrueType GDI font unavailable");i=fonts_.emplace(key,f).first;}SelectObject(dc_,i->second);return dc_;}
 const std::vector<fs::path>&files()const{return files_;}std::string face()const{return std::string(actual_.begin(),actual_.end());}size_t sizes()const{return fonts_.size();}
};
inline UiTrueType&ui_ttf(){static UiTrueType value;return value;}
#endif
// Measure at the same rounded physical font size used by the renderer. This
// matters at 720p/1080p: scaling a 24px metric is not GDI's 36px metric.
inline float ui_text_width_scaled(FontStyle style,const std::string&s,float scale=1){
 const int pixels=std::clamp(int(std::lround(ui_font_pixels(style)*scale)),6,256);
#ifdef _WIN32
 auto dc=ui_ttf().select(pixels,ui_font_bold(style));auto w=ui_wide(s);SIZE size{};GetTextExtentPoint32W(dc,w.data(),int(w.size()),&size);return float(size.cx);
#else
 // Bundled Play advance widths, generated from its 1000-unit em hmtx table.
 static constexpr unsigned short regular[]={230,280,443,580,580,855,636,256,331,331,398,500,256,358,256,421,580,580,580,580,580,580,580,580,580,580,256,256,500,500,500,533,737,652,613,564,679,580,560,666,707,265,403,609,520,861,723,679,570,676,632,569,585,676,652,968,604,584,580,330,421,330,321,580,399,538,579,464,571,535,380,569,573,246,247,514,246,890,573,568,579,579,351,483,402,573,463,792,479,460,454,330,335,330,580};
 static constexpr unsigned short bold[]={230,295,443,580,606,855,656,256,348,348,446,580,256,388,256,431,580,580,580,580,580,580,580,580,580,580,256,256,580,580,580,529,787,668,635,597,710,610,610,705,728,296,445,682,580,889,751,728,627,740,668,606,595,699,664,985,677,644,612,372,431,372,341,580,449,564,610,482,608,559,388,608,595,282,274,540,282,876,595,592,609,608,397,525,407,595,510,822,568,534,512,384,265,384,580};
 const auto* advances=ui_font_bold(style)?bold:regular;float units=0;
 for(auto cp:font_codepoints(s))units+=cp>=32&&cp<127?advances[cp-32]:650;
 return units*pixels/1000.f;
#endif
}
inline int ui_raster_pixels(float renderedscale,bool heading=false){return std::clamp(int(std::lround((heading?24.f:18.f)*renderedscale)),6,256);}
inline float ui_text_width_pixels(int pixels,bool bold,const std::string&s){return ui_text_width_scaled(bold?FontStyle::Italic24:FontStyle::Regular18,s,float(pixels)/(bold?24.f:18.f));}
inline std::string ui_fit_text(const std::string&s,float width,float renderedscale,bool heading=false){
 int px=ui_raster_pixels(renderedscale,heading);if(ui_text_width_pixels(px,heading,s)<=width)return s;
 std::string result=s;const std::string suffix="...";if(ui_text_width_pixels(px,heading,suffix)>width)return {};
 while(!result.empty()) {size_t end=result.size()-1;while(end>0&&(static_cast<unsigned char>(result[end])&0xc0)==0x80)--end;result.resize(end);if(ui_text_width_pixels(px,heading,result+suffix)<=width)return result+suffix;}return {};
}
inline std::vector<std::string> ui_wrap_text(const std::string&s,float width,float renderedscale,bool heading=false,size_t maxlines=6){
 std::vector<std::string> lines;if(width<=0||!maxlines)return lines;
 const int px=ui_raster_pixels(renderedscale,heading);std::string line,word;std::istringstream input(s);
 auto fits=[&](const std::string&t){return ui_text_width_pixels(px,heading,t)<=width;};
 while(input>>word){
  if(!line.empty()&&fits(line+" "+word)){line+=" "+word;continue;}
  if(!line.empty()){lines.push_back(line);line.clear();}
  while(!word.empty()&&!fits(word)){
   size_t end=0,last=0;while(end<word.size()){last=end;unsigned char c=static_cast<unsigned char>(word[end]);end+=c<0x80?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;end=std::min(end,word.size());if(!fits(word.substr(0,end))){end=last;break;}}
   if(!end){word.clear();break;}lines.push_back(word.substr(0,end));word.erase(0,end);
  }
  line=word;
 }
 if(!line.empty())lines.push_back(line);
 if(lines.size()>maxlines){lines.resize(maxlines);lines.back()=ui_fit_text(lines.back()+" ...",width,renderedscale,heading);}
 return lines;
}
inline float ui_text_width(FontStyle style,const std::string&s){return ui_text_width_scaled(style,s);}
}
