// Standalone native WYSIWYG track editor. No engine renderer dependency.
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include "track_projects.hpp"
#include "alpha_track.hpp"
#include "legacy_ui/menu_assets.hpp"
#include <d3d9.h>
#include "assets.hpp"
#include "content.hpp"
#include "captured_scene.hpp"
#include "track_editor.hpp"
#include "water.hpp"
#include "native_shader_win.hpp"
#include "file_io.hpp"
#include "diagnostics_win.hpp"
#include <memory>
#include <chrono>
#include <cstring>
#include <sstream>
#include <limits>
#include <cwctype>
#include <commctrl.h>
#include "editor_gizmo.hpp"
#include "editor_transform.hpp"
#include <set>
#include "editor_scene_query.hpp"
extern "C" {__declspec(dllexport) DWORD NvOptimusEnablement=1;__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;}
using namespace ofo;
namespace {
template<class T>void release(T*&p){if(p){p->Release();p=nullptr;}}
void hr(HRESULT h,const char*w){if(FAILED(h))throw std::runtime_error(std::string(w)+" HRESULT="+std::to_string(uint32_t(h)));}
uint32_t u32(const Bytes&b,size_t o){if(o+4>b.size())throw std::runtime_error("Truncated texture");uint32_t x;std::memcpy(&x,b.data()+o,4);return x;}
std::wstring wide(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
fs::path exe_dir(){wchar_t b[32768];GetModuleFileNameW(nullptr,b,32768);return fs::path(b).parent_path();}
fs::path pick_folder(HWND owner){BROWSEINFOW b{};b.hwndOwner=owner;b.lpszTitle=L"Оберіть папку з оригінальними ресурсами (Resources)";b.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;auto id=SHBrowseForFolderW(&b);if(!id)return {};wchar_t path[MAX_PATH];bool ok=SHGetPathFromIDListW(id,path);CoTaskMemFree(id);return ok?fs::path(path):fs::path{};}
}
#include "editor_render_win.hpp"
#include "editor_controller_win.hpp"
namespace {

Editor*active=nullptr;
void showError(HWND hwnd,const std::exception&e){try{write_file_atomic(exe_dir()/"editor_error.json",Json{{"error",e.what()},{"application","OpenFOTrackEditor"}}.dump(2));}catch(...){}MessageBoxW(hwnd,wide(e.what()).c_str(),L"OpenFO Track Editor — помилка",MB_OK|MB_ICONERROR);}
LRESULT CALLBACK viewportProc(HWND h,UINT m,WPARAM w,LPARAM l){Editor*e=(Editor*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){e=(Editor*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)e);}if(!e)return DefWindowProcW(h,m,w,l);try{switch(m){
case WM_ERASEBKGND:return 1;
case WM_CTLCOLORSTATIC:SetTextColor((HDC)w,RGB(232,237,243));SetBkColor((HDC)w,RGB(27,30,35));return (LRESULT)e->panelBrush;
case WM_LBUTTONDOWN:{SetFocus(h);if(e->modal.active){e->finishModal(true);return 0;}int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);e->last={x,y};if(e->boxArmed){e->boxStart=e->boxEnd={x,y};e->boxActive=true;e->boxAdd=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;SetCapture(h);return 0;}if(e->inPreview(x,y)){e->previewDrag=true;SetCapture(h);}else if(!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&e->beginGizmo(x,y))SetCapture(h);else e->click(x,y);return 0;}
case WM_LBUTTONUP:if(e->boxActive){e->finishBox();return 0;}if(e->modal.active&&!e->modal.keyboard)e->finishDrag(true);if(e->modal.active)return 0;if(e->previewDrag){e->previewDrag=false;}if(GetCapture()==h)ReleaseCapture();return 0;
case WM_RBUTTONDOWN:if(e->modal.active){e->finishModal(false);return 0;}e->boxActive=e->boxArmed=false;SetFocus(h);e->last={GET_X_LPARAM(l),GET_Y_LPARAM(l)};e->previewDrag=e->inPreview(e->last.x,e->last.y);e->rightDrag=!e->previewDrag;SetCapture(h);return 0;
case WM_RBUTTONUP:e->rightDrag=false;e->previewDrag=false;if(GetCapture()==h)ReleaseCapture();return 0;
case WM_CAPTURECHANGED:e->boxActive=e->boxArmed=e->middleDrag=false;e->rightDrag=false;e->previewDrag=false;e->finishDrag(false);return 0;
case WM_MOUSEMOVE:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};if(e->boxActive)e->boxEnd=p;else if(e->modal.active)e->modalTo(p.x,p.y);else if(e->middleDrag)e->updateViewportOrbit(p,(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);else if(e->previewDrag){e->previewOrbit+=(p.x-e->last.x)*.01f;e->previewPitch=std::clamp(e->previewPitch+(p.y-e->last.y)*.01f,-1.45f,1.45f);}else if(e->rightDrag){e->yaw+=(p.x-e->last.x)*.005f;e->pitch=std::clamp(e->pitch-(p.y-e->last.y)*.005f,-1.5f,1.5f);}e->last=p;e->hoverDirty=true;return 0;}
case WM_MBUTTONDOWN:if(!e->modal.active){SetFocus(h);e->beginViewportOrbit({GET_X_LPARAM(l),GET_Y_LPARAM(l)});SetCapture(h);}return 0;case WM_MBUTTONUP:e->middleDrag=false;if(GetCapture()==h&&!e->modal.active)ReleaseCapture();return 0;
case WM_MOUSEWHEEL:{if(e->modal.active)return 0;POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);float delta=GET_WHEEL_DELTA_WPARAM(w)/120.f;if(e->inPreview(p.x,p.y))e->previewZoom=std::clamp(e->previewZoom*std::pow(1.12f,delta),.35f,3.f);else e->dollyViewport(delta);e->hoverDirty=true;return 0;}
case WM_KEYUP:if(e->modal.active)e->modalTo(e->modal.mouseLast.x,e->modal.mouseLast.y);return 0;case WM_SYSKEYDOWN:case WM_KEYDOWN:if(!(l&(1ll<<30)))e->key(w);return 0;}}catch(const std::exception&x){e->finishDrag(false);showError(h,x);}return DefWindowProcW(h,m,w,l);}
LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){if(!active)return DefWindowProcW(h,m,w,l);try{switch(m){
case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetTextColor((HDC)w,RGB(232,237,243));SetBkColor((HDC)w,RGB(27,30,35));return (LRESULT)active->panelBrush;
case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:SetTextColor((HDC)w,RGB(240,243,248));SetBkColor((HDC)w,RGB(39,43,50));return (LRESULT)active->inputBrush;
case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);FillRect((HDC)w,&r,active->panelBrush);return 1;}
case WM_DRAWITEM:{auto*d=(DRAWITEMSTRUCT*)l;if(d->CtlType!=ODT_BUTTON)break;bool accent=d->CtlID==ID_SAVE||d->CtlID==ID_APPLY||d->CtlID==ID_MAP;bool pressed=d->itemState&ODS_SELECTED;HBRUSH b=CreateSolidBrush(pressed?RGB(212,160,45):accent?RGB(106,83,34):RGB(47,52,61));FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,d->itemState&ODS_DISABLED?RGB(115,120,130):accent?RGB(255,218,119):RGB(235,239,245));SelectObject(d->hDC,active->font);wchar_t text[256];GetWindowTextW(d->hwndItem,text,256);RECT r=d->rcItem;DrawTextW(d->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(d->itemState&ODS_FOCUS){InflateRect(&r,-3,-3);DrawFocusRect(d->hDC,&r);}return TRUE;}
case WM_COMMAND:{int id=LOWORD(w),code=HIWORD(w);if(id==ID_STAGE&&code==CBN_SELCHANGE){auto chosen=size_t(SendMessageW(active->stageBox,CB_GETCURSEL,0,0));if(active->dirty){int answer=MessageBoxW(h,L"Зберегти зміни перед зміною траси?",L"Track Studio",MB_YESNOCANCEL|MB_ICONQUESTION);if(answer==IDCANCEL){auto old=std::find(active->stages.begin(),active->stages.end(),active->doc.stage);SendMessageW(active->stageBox,CB_SETCURSEL,old-active->stages.begin(),0);return 0;}if(answer==IDYES)active->save(false);}try{active->loadStage(chosen);}catch(...){auto old=std::find(active->stages.begin(),active->stages.end(),active->doc.stage);SendMessageW(active->stageBox,CB_SETCURSEL,old==active->stages.end()?-1:old-active->stages.begin(),0);if(active->render)active->render->state.invalidate();active->status();throw;}}else if(id==ID_SEARCH&&code==EN_CHANGE)active->filter();else if(code==CBN_SELCHANGE){active->finishDrag(true);active->syncFields();active->hoverDirty=true;}else if(code==BN_CLICKED)active->command(id);return 0;}
case WM_SIZE:if(active->window){active->resize();active->renderFailed=false;}return 0;case WM_VSCROLL:active->scrollCommand(w);return 0;
case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize={900,540};return 0;
case WM_CLOSE:active->finishDrag(false);if(active->dirty){int answer=MessageBoxW(h,L"Зберегти зміни перед виходом?",L"Редактор траси",MB_YESNOCANCEL|MB_ICONQUESTION);if(answer==IDCANCEL)return 0;if(answer==IDYES)active->save(false);}DestroyWindow(h);return 0;
case WM_MOUSEWHEEL:{if(active->modal.active)return 0;POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);if(p.x<Editor::panelWidth)active->scrollSidebar(active->sidebarScroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*72);else SendMessageW(active->viewport,m,w,l);return 0;}
case WM_DESTROY:PostQuitMessage(0);return 0;}}catch(const std::exception&e){showError(h,e);}return DefWindowProcW(h,m,w,l);}

}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE,LPSTR,int){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);try{install_diagnostics(exe_dir());trace("editor_startup","0.6.5");fs::path data;int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);for(int i=1;i<argc;i++)if(std::wstring(argv[i])==L"--data"&&i+1<argc)data=argv[++i];if(argv)LocalFree(argv);if(data.empty()&&fs::exists(exe_dir()/"data_path.txt")){auto b=read_file(exe_dir()/"data_path.txt",65536);if(!b.empty()){std::string saved(b.begin(),b.end());if(saved.size()>=3&&(unsigned char)saved[0]==239&&(unsigned char)saved[1]==187&&(unsigned char)saved[2]==191)saved.erase(0,3);while(!saved.empty()&&(saved.back()=='\r'||saved.back()=='\n'||saved.back()==' '||saved.back()=='\t'))saved.pop_back();auto first=saved.find_first_not_of(" \t\r\n");if(first!=std::string::npos)data=fs::u8path(saved.substr(first));}}if(data.empty()){for(auto p:{exe_dir()/"Resources",exe_dir()/"resources",exe_dir()/"data"})if(fs::exists(p/"cars/car_3/body.bgm")||fs::exists(p/"data/cars/car_3/body.bgm")){data=p;break;}}if(data.empty())data=pick_folder(nullptr);if(data.empty()){CoUninitialize();return 0;}if(fs::is_directory(data/"data"))data/="data";if(!fs::is_directory(data/"tracks")||!fs::is_directory(data/"cars")||!fs::exists(data/"scripts/cardealer.bed")||!fs::exists(data/"cars/car_3/body.bgm"))throw std::runtime_error("Invalid resource folder: cars and tracks required");Editor app;active=&app;app.data_directory=data;app.resources=std::make_unique<Resources>(data);app.resources->overlay_textures(exe_dir()/"data");if(fs::exists(exe_dir()/"capture_assets"))app.resources->mount(exe_dir()/"capture_assets","capture_assets");WNDCLASSW wc{};wc.hInstance=inst;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpfnWndProc=windowProc;wc.lpszClassName=L"OpenFOTrackEditor";RegisterClassW(&wc);wc.lpfnWndProc=viewportProc;wc.lpszClassName=L"OpenFOEditorViewport";RegisterClassW(&wc);MONITORINFO monitor{sizeof(MONITORINFO)};POINT initialCursor;GetCursorPos(&initialCursor);GetMonitorInfoW(MonitorFromPoint(initialCursor,MONITOR_DEFAULTTOPRIMARY),&monitor);RECT work=monitor.rcWork;if(work.right<=work.left||work.bottom<=work.top)SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int initialWidth=std::min(1440,int(work.right-work.left)),initialHeight=std::min(960,int(work.bottom-work.top));app.window=CreateWindowW(L"OpenFOTrackEditor",L"OpenFO — 3D редактор траси",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VSCROLL,work.left+(work.right-work.left-initialWidth)/2,work.top+(work.bottom-work.top-initialHeight)/2,initialWidth,initialHeight,nullptr,nullptr,inst,nullptr);if(!app.window)throw std::runtime_error("Cannot create editor window");app.ui();if(auto dwm=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){using SetAttr=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);auto set=(SetAttr)GetProcAddress(dwm,"DwmSetWindowAttribute");if(set){BOOL dark=TRUE;set(app.window,20,&dark,sizeof(dark));}FreeLibrary(dwm);}app.resize();ShowWindow(app.window,SW_SHOW);UpdateWindow(app.window);app.resources->mount(exe_dir()/"alpha_content","alpha_content");app.stages=stage_catalog(*app.resources);for(const auto& alpha:alpha_track_catalog(*app.resources))app.stages.push_back(alpha.stage);if(app.stages.empty())throw std::runtime_error("No stages found in original resource folder");size_t initial=0;for(size_t i=0;i<app.stages.size();i++){SendMessageW(app.stageBox,CB_ADDSTRING,0,(LPARAM)wide(app.stages[i]).c_str());if(app.stages[i].find("forest1/a")!=std::string::npos)initial=i;}SendMessageW(app.stageBox,CB_SETCURSEL,initial,0);trace("editor_stage_load",app.stages[initial]);app.loadStage(initial);trace("editor_ready",app.doc.stage);SetFocus(app.viewport);auto then=std::chrono::steady_clock::now();MSG msg{};bool running=true;while(running){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}if(!app.modal.active&&msg.message==WM_KEYDOWN&&msg.wParam=='S'&&(GetAsyncKeyState(VK_CONTROL)&0x8000)){SendMessageW(app.window,WM_COMMAND,MAKEWPARAM(ID_SAVE,BN_CLICKED),0);continue;}if(msg.message==WM_KEYDOWN&&msg.wParam==VK_TAB&&GetFocus()!=app.viewport&&IsDialogMessageW(app.window,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running)break;if(IsIconic(app.window)){MsgWaitForMultipleObjects(0,nullptr,FALSE,100,QS_ALLINPUT);continue;}auto now=std::chrono::steady_clock::now();float dt=std::min(.05f,std::chrono::duration<float>(now-then).count());then=now;if(!app.renderFailed){try{app.frame(dt);}catch(const std::exception&e){app.renderFailed=true;app.message=std::string("Рендер призупинено: ")+e.what();app.status();trace("editor_frame_error",e.what());showError(app.window,e);}}else MsgWaitForMultipleObjects(0,nullptr,FALSE,32,QS_ALLINPUT);}active=nullptr;app.render.reset();CoUninitialize();return 0;}catch(const std::exception&e){showError(nullptr,e);CoUninitialize();return 1;}}
