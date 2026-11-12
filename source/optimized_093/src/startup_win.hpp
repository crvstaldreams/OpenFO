#pragma once
#include "startup_flow.hpp"
#include "assets.hpp"
#include "diagnostics_win.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <chrono>
#include <functional>
#include <cstring>
namespace ofo {
// Startup uses Win32 presentation before creating the GPU and loading the menu scene.
// MPEG-1 is handled by Windows MCI; no subprocess or external decoder is launched.
struct StartupBitmap {
 int width=0,height=0;std::vector<uint32_t> pixels;
 void load(const Resources&r,const std::string&path){auto b=r.read(path);if(b.size()<18)throw std::runtime_error("Startup TGA header");width=b[12]+256*b[13];height=b[14]+256*b[15];int bytes=b[16]/8,type=b[2];if(b[1]||(type!=2&&type!=10)||width<1||height<1||width>8192||height>8192||(bytes!=3&&bytes!=4))throw std::runtime_error("Unsupported startup TGA");pixels.resize(size_t(width)*height);size_t pos=18+b[0],i=0;auto pixel=[&]{if(pos+bytes>b.size())throw std::runtime_error("Startup TGA truncated");uint32_t p=0;std::memcpy(&p,b.data()+pos,bytes);pos+=bytes;return p|0xff000000u;};std::vector<uint32_t> source(pixels.size());while(i<source.size()){unsigned n=1;bool repeat=false;if(type==10){if(pos>=b.size())throw std::runtime_error("Startup TGA packet");auto p=b[pos++];n=(p&127)+1;repeat=(p&128)!=0;}if(n>source.size()-i)throw std::runtime_error("Startup TGA overflow");auto p=repeat?pixel():0;while(n--)source[i++]=repeat?p:pixel();}for(int y=0;y<height;++y)for(int x=0;x<width;++x)pixels[size_t(y)*width+x]=source[size_t((b[17]&32)?y:height-1-y)*width+((b[17]&16)?width-1-x:x)];}
};
// Retain the presentation for WM_PAINT; never clear the visible DC between frames.
struct StartupPresentation {HWND window=nullptr;const StartupBitmap*image=nullptr;std::wstring label;StartupPaintPolicy policy;};
inline StartupPresentation startup_presentation;
inline bool startup_visible(HWND window){return startup_presentation.window==window;}
inline void startup_finish(HWND window){if(startup_visible(window))startup_presentation={};}
inline void startup_draw(HDC dc,const RECT&area,const StartupBitmap*image,const wchar_t*label){FillRect(dc,&area,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));if(image&&!image->pixels.empty()){auto fit=startup_fit(area.right,area.bottom,image->width,image->height);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=image->width;info.bmiHeader.biHeight=-image->height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;SetStretchBltMode(dc,HALFTONE);StretchDIBits(dc,fit.x,fit.y,fit.w,fit.h,0,0,image->width,image->height,image->pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);}if(label&&*label){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(235,235,235));RECT text{0,area.bottom-72,area.right,area.bottom-24};DrawTextW(dc,label,-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
inline bool startup_window_paint(HWND window){if(!startup_visible(window))return false;PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);RECT area;GetClientRect(window,&area);if(dc&&area.right>0&&area.bottom>0){HDC memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,area.right,area.bottom);if(memory&&bitmap){auto old=SelectObject(memory,bitmap);startup_draw(memory,area,startup_presentation.image,startup_presentation.label.c_str());BitBlt(dc,0,0,area.right,area.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);}else startup_draw(dc,area,startup_presentation.image,startup_presentation.label.c_str());if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);}EndPaint(window,&paint);return true;}
inline void startup_paint(HWND window,const StartupBitmap*image=nullptr,const wchar_t*label=nullptr){auto&state=startup_presentation;std::wstring text=label?label:L"";bool changed=state.window!=window||state.image!=image||state.label!=text;state.window=window;state.image=image;state.label=std::move(text);if(state.policy.redraw(changed,false)){InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);}}
inline bool startup_play_movie(HWND window,const fs::path&path,const std::function<bool()>&pump,StartupEnter&enter){
 std::wstring alias=L"openfo_startup",reply(512,L'\0');auto command=[&](const std::wstring&line){std::fill(reply.begin(),reply.end(),L'\0');auto err=mciSendStringW(line.c_str(),reply.data(),UINT(reply.size()),window);if(err){wchar_t reason[256]{};mciGetErrorStringW(err,reason,256);trace("startup_mci_error",path.u8string()+" code="+std::to_string(err));OutputDebugStringW(reason);}return err;};
 auto type=path.extension()==L".mpg"?L"MPEGVideo":L"AVIVideo";
 std::wstring open=L"open \""+path.wstring()+L"\" type "+type+L" alias "+alias+L" style child parent "+std::to_wstring(reinterpret_cast<uintptr_t>(window));if(command(open))return false;
 struct Close {std::wstring alias;~Close(){mciSendStringW((L"close "+alias).c_str(),nullptr,0,nullptr);}} close{alias};
 auto size=[&]{RECT r;GetClientRect(window,&r);command(L"put "+alias+L" window at 0 0 "+std::to_wstring(r.right)+L" "+std::to_wstring(r.bottom));command(L"where "+alias+L" source");int x=0,y=0,w=640,h=480;swscanf(reply.c_str(),L"%d %d %d %d",&x,&y,&w,&h);auto fit=startup_fit(r.right,r.bottom,w,h);command(L"put "+alias+L" destination at "+std::to_wstring(fit.x)+L" "+std::to_wstring(fit.y)+L" "+std::to_wstring(fit.w)+L" "+std::to_wstring(fit.h));};size();if(command(L"play "+alias))return false;trace("startup_movie_play",path.u8string());RECT previous{};GetClientRect(window,&previous);while(pump()){if(enter.press((GetAsyncKeyState(VK_RETURN)&0x8000)!=0)){trace("startup_movie_skipped",path.u8string());break;}RECT now;GetClientRect(window,&now);if(now.right!=previous.right||now.bottom!=previous.bottom){size();previous=now;}if(command(L"status "+alias+L" mode")||reply.compare(0,7,L"playing")!=0)break;Sleep(10);}return true;
}
inline void startup_sequence(HWND window,const fs::path&exe_dir,const fs::path&data,const Resources&r,StartupBitmap&copyright,const std::function<bool()>&pump){StartupEnter enter((GetAsyncKeyState(VK_RETURN)&0x8000)!=0);startup_paint(window);for(auto name:{"bugbear","empire","nvidia","intro"}){if(!pump())return;bool played=false;for(auto candidate:{exe_dir/"startup_video"/(std::string(name)+".mpg"),exe_dir/"startup_video"/(std::string(name)+".avi"),data/"video"/(std::string(name)+".avi")}){if(fs::exists(candidate)&&startup_play_movie(window,candidate,pump,enter)){played=true;break;}}if(!played)trace("startup_movie_unavailable",name);}
 try{copyright.load(r,"menu/copyright.tga");}catch(const std::exception&e){trace("startup_copyright_error",e.what());}StartupFlow flow;flow.movies_finished();auto last=std::chrono::steady_clock::now();startup_paint(window,&copyright);last=std::chrono::steady_clock::now();while(pump()&&flow.phase==StartupFlow::Copyright){auto now=std::chrono::steady_clock::now();flow.advance(std::chrono::duration<double>(now-last).count());last=now;Sleep(10);}trace("startup_copyright_minimum_complete","4 seconds");
}
}
