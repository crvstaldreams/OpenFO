#pragma once
#define NOMINMAX
#include <windows.h>
#include "display_resolution_policy.hpp"
#include <string>
#include <vector>
namespace ofo {
inline bool monitor_device_name(HMONITOR monitor,std::wstring&out){
 MONITORINFOEXW info{};info.cbSize=sizeof(info);
 if(!GetMonitorInfoW(monitor,reinterpret_cast<MONITORINFO*>(&info)))return false;
 out=info.szDevice;return !out.empty();
}
inline std::vector<DisplayResolution> query_monitor_resolutions(HMONITOR monitor){
 std::wstring device;if(!monitor_device_name(monitor,device))return {};
 std::vector<DisplayResolution> out;DEVMODEW mode{};mode.dmSize=sizeof(mode);
 for(DWORD i=0;EnumDisplaySettingsExW(device.c_str(),i,&mode,0);++i){
  if((mode.dmFields&DM_PELSWIDTH)&&(mode.dmFields&DM_PELSHEIGHT)&&mode.dmBitsPerPel>=16)
   out.emplace_back(int(mode.dmPelsWidth),int(mode.dmPelsHeight));
  mode={};mode.dmSize=sizeof(mode);
 }
 // Include the active desktop mode, even when the GPU driver does not enumerate it.
 if(EnumDisplaySettingsExW(device.c_str(),ENUM_CURRENT_SETTINGS,&mode,0))
  out.emplace_back(int(mode.dmPelsWidth),int(mode.dmPelsHeight));
 return normalize_display_resolutions(std::move(out));
}
class ExclusiveDisplayMode {
 std::wstring device_;DEVMODEW original_{};bool active_=false;
public:
 ExclusiveDisplayMode()=default;ExclusiveDisplayMode(const ExclusiveDisplayMode&)=delete;
 ExclusiveDisplayMode&operator=(const ExclusiveDisplayMode&)=delete;
 ~ExclusiveDisplayMode(){restore();}
 bool active()const{return active_;}
 void restore(){if(active_){ChangeDisplaySettingsExW(device_.c_str(),&original_,nullptr,0,nullptr);active_=false;device_.clear();}}
 bool apply(HMONITOR monitor,int width,int height){
  std::wstring device;if(!monitor_device_name(monitor,device))return false;
  if(active_&&device!=device_)restore();
  DEVMODEW current{};current.dmSize=sizeof(current);
  if(!EnumDisplaySettingsExW(device.c_str(),ENUM_CURRENT_SETTINGS,&current,0))return false;
  // Store the actual pre-game mode once: never restore to an intermediate game mode.
  if(!active_){original_=current;device_=device;}
  if(int(current.dmPelsWidth)==width&&int(current.dmPelsHeight)==height){
   // The desktop already has the requested dimensions, so no display switch is needed.
   if(!active_)device_.clear();return true;
  }
  DEVMODEW target{};bool found=false;unsigned best_difference=0xffffffffu;
  for(DWORD i=0;EnumDisplaySettingsExW(device.c_str(),i,&target,0);++i){
   if(int(target.dmPelsWidth)==width&&int(target.dmPelsHeight)==height&&target.dmBitsPerPel>=16){
    unsigned hz=(target.dmFields&DM_DISPLAYFREQUENCY)?target.dmDisplayFrequency:60;
    unsigned desired=(current.dmFields&DM_DISPLAYFREQUENCY)?current.dmDisplayFrequency:60;
    unsigned delta=hz>desired?hz-desired:desired-hz;
    if(delta<best_difference){best_difference=delta;found=true;}
   }
   target={};target.dmSize=sizeof(target);
  }
  if(!found){if(!active_)device_.clear();return false;}
  // Read a matching driver-provided DEVMODE again (never synthesize an unadvertised timing).
  DEVMODEW choice{};choice.dmSize=sizeof(choice);bool chosen=false;
  for(DWORD i=0;EnumDisplaySettingsExW(device.c_str(),i,&choice,0);++i){
   if(int(choice.dmPelsWidth)==width&&int(choice.dmPelsHeight)==height&&choice.dmBitsPerPel>=16){
    unsigned hz=(choice.dmFields&DM_DISPLAYFREQUENCY)?choice.dmDisplayFrequency:60;
    unsigned desired=(current.dmFields&DM_DISPLAYFREQUENCY)?current.dmDisplayFrequency:60;
    unsigned delta=hz>desired?hz-desired:desired-hz;
    if(delta==best_difference){chosen=true;break;}
   }
   choice={};choice.dmSize=sizeof(choice);
  }
  if(!chosen){if(!active_)device_.clear();return false;}
  if(ChangeDisplaySettingsExW(device.c_str(),&choice,nullptr,CDS_TEST,nullptr)!=DISP_CHANGE_SUCCESSFUL){if(!active_)device_.clear();return false;}
  if(ChangeDisplaySettingsExW(device.c_str(),&choice,nullptr,CDS_FULLSCREEN,nullptr)!=DISP_CHANGE_SUCCESSFUL){if(!active_)device_.clear();return false;}
  active_=true;return true;
 }
};
}
