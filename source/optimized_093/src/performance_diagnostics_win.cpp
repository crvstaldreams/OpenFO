#define NOMINMAX
#include "performance_diagnostics_win.hpp"
#include "performance_drop.hpp"
#include "diagnostics_win.hpp"
#include "frontend.hpp"
#include "png_writer.hpp"
#include "render_quality.hpp"
#define PSAPI_VERSION 2
#include <psapi.h>
#include <array>
#include <cstdio>
#include <deque>
#include <fstream>
#include <future>
#include <iomanip>
#include <locale>
#include <optional>
#include <utility>

namespace ofo {
namespace {
using Clock=std::chrono::steady_clock;
using Ms=std::chrono::duration<double,std::milli>;
constexpr unsigned max_snapshots=24;
constexpr size_t history_limit=120;

std::string utf8(const std::wstring& text){
 if(text.empty())return {};
 const int size=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
 if(size<=0)return {};
 std::string result(size,'\0');
 WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),size,nullptr,nullptr);
 return result;
}

std::string registry_string(const wchar_t* key,const wchar_t* name){
 wchar_t value[512]{};
 DWORD bytes=sizeof(value);
 if(RegGetValueW(HKEY_LOCAL_MACHINE,key,name,RRF_RT_REG_SZ,nullptr,value,&bytes)!=ERROR_SUCCESS)return {};
 return utf8(value);
}

std::string utc_stamp(bool for_path){
 SYSTEMTIME time{};
 GetSystemTime(&time);
 char result[48]{};
 if(for_path)std::snprintf(result,sizeof(result),"%04u%02u%02uT%02u%02u%02uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);
 else std::snprintf(result,sizeof(result),"%04u-%02u-%02uT%02u:%02u:%02uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);
 return result;
}

unsigned physical_core_count(){
 DWORD size=0;
 GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&size);
 if(!size||size>1024*1024)return 0;
 std::vector<unsigned char> bytes(size);
 auto* first=reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(bytes.data());
 if(!GetLogicalProcessorInformationEx(RelationProcessorCore,first,&size))return 0;
 unsigned cores=0;
 for(size_t offset=0;offset<size;){
  const auto* entry=reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(bytes.data()+offset);
  if(!entry->Size||entry->Size>size-offset)break;
  cores+=entry->Relationship==RelationProcessorCore;
  offset+=entry->Size;
 }
 return cores;
}

Json host_info(){
 constexpr auto windows_key=L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
 constexpr auto processor_key=L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
 SYSTEM_INFO system{};
 GetNativeSystemInfo(&system);
 MEMORYSTATUSEX ram{};
 ram.dwLength=sizeof(ram);
 const bool has_ram=GlobalMemoryStatusEx(&ram)!=0;
 Json display=Json::array();
 for(DWORD i=0;i<32;++i){
  DISPLAY_DEVICEW adapter{};
  adapter.cb=sizeof(adapter);
  if(!EnumDisplayDevicesW(nullptr,i,&adapter,0))break;
  auto id=std::wstring(adapter.DeviceID);
  if(auto instance=id.find(L'\\',4);instance!=id.npos)id.resize(instance);
  Json item={{"name",utf8(adapter.DeviceString)},{"hardware_id",utf8(id)},
             {"primary",(adapter.StateFlags&DISPLAY_DEVICE_PRIMARY_DEVICE)!=0},
             {"desktop_attached",(adapter.StateFlags&DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)!=0}};
  DEVMODEW mode{};
  mode.dmSize=sizeof(mode);
  if(EnumDisplaySettingsExW(adapter.DeviceName,ENUM_CURRENT_SETTINGS,&mode,0))
   item["desktop_mode"]={{"width",mode.dmPelsWidth},{"height",mode.dmPelsHeight},{"refresh_hz",mode.dmDisplayFrequency}};
  display.push_back(std::move(item));
 }
 return {{"windows",{{"edition",registry_string(windows_key,L"EditionID")},
                     {"release",registry_string(windows_key,L"DisplayVersion")},
                     {"build",registry_string(windows_key,L"CurrentBuildNumber")}}},
         {"cpu",{{"model",registry_string(processor_key,L"ProcessorNameString")},
                 {"logical_threads",GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)},
                 {"physical_cores",physical_core_count()},{"architecture_id",system.wProcessorArchitecture}}},
         {"ram_total_bytes",has_ram?Json(ram.ullTotalPhys):Json()},
         {"display_adapters",std::move(display)}};
}

const char* page_name(UI::Page page){
 switch(page){
  case UI::Title:return "title";
  case UI::Select:return "selection";
  case UI::Race:return "race";
  case UI::Pause:return "pause";
  case UI::Confirm:return "confirmation";
  case UI::Settings:return "settings";
  case UI::Loading:return "loading";
  case UI::Replay:return "replay";
 }
 return "unknown";
}

Json graphics_settings(const UI& ui,const RendererFrameMetrics& renderer,bool uncapped,bool optimized){
 Json settings={{"backbuffer_width",renderer.width},{"backbuffer_height",renderer.height},
                {"app_fps_limit",uncapped?Json():Json(60)},{"optimized_renderer",optimized},
                {"night",ui.environment.night},{"rain",ui.environment.rain},
                {"rain_strength",ui.environment.rain_strength},{"sun_intensity",ui.environment.sun_intensity},
                {"fog_density",ui.environment.fog_density}};
 if(const auto& menu=ui.frontend){
  settings["detail_level"]=render_quality_name(menu->detail_level);
  settings["fullscreen"]=menu->fullscreen;
  settings["requested_window_width"]=menu->screen_width;
  settings["requested_window_height"]=menu->screen_height;
  settings["lod"]=menu->lod_enabled;
  settings["ambient_occlusion"]=menu->ambient_occlusion;
  settings["water_ssr"]=menu->water_ssr;
  settings["bloom_intensity"]=menu->bloom_intensity;
 }
 return settings;
}

uint64_t file_ticks(FILETIME value){
 return (uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;
}

struct CpuSample {
 uint64_t total=0,idle=0,process=0;
};

std::optional<CpuSample> cpu_sample(){
 FILETIME idle{},kernel{},user{},created{},exited{},process_kernel{},process_user{};
 if(!GetSystemTimes(&idle,&kernel,&user)||
    !GetProcessTimes(GetCurrentProcess(),&created,&exited,&process_kernel,&process_user))return std::nullopt;
 return CpuSample{file_ticks(kernel)+file_ticks(user),file_ticks(idle),
                  file_ticks(process_kernel)+file_ticks(process_user)};
}

Json system_load(const std::optional<CpuSample>& previous,const std::optional<CpuSample>& current){
 Json result=Json::object();
 MEMORYSTATUSEX ram{};
 ram.dwLength=sizeof(ram);
 if(GlobalMemoryStatusEx(&ram)){
  result["ram_available_bytes"]=ram.ullAvailPhys;
  result["ram_load_pct"]=ram.dwMemoryLoad;
 }
 PROCESS_MEMORY_COUNTERS_EX memory{};
 memory.cb=sizeof(memory);
 if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))){
  result["openfo_working_set_bytes"]=memory.WorkingSetSize;
  result["openfo_private_commit_bytes"]=memory.PrivateUsage;
 }
 if(previous&&current&&current->total>previous->total&&current->idle>=previous->idle&&current->process>=previous->process){
  const auto total=double(current->total-previous->total);
  result["system_cpu_pct"]=std::clamp(100.*(1.-double(current->idle-previous->idle)/total),0.,100.);
  result["openfo_cpu_pct_of_system"]=std::clamp(100.*double(current->process-previous->process)/total,0.,100.);
 }
 return result;
}

struct HistoryFrame {
 uint64_t number;
 double elapsed_ms,interval_ms,simulation_ms,draw_ms,present_ms;
};

struct Snapshot {
 std::string stem;
 Json metadata;
 UI::Page page;
 std::string track;
 int car;
};

DiagnosticPixels resize_snapshot(DiagnosticPixels image){
 if(image.width<=1920&&image.height<=1080)return image;
 const double scale=std::min(1920./image.width,1080./image.height);
 const auto width=std::max(1u,unsigned(image.width*scale));
 const auto height=std::max(1u,unsigned(image.height*scale));
 std::vector<uint32_t> pixels(size_t(width)*height);
 for(unsigned y=0;y<height;++y)
  for(unsigned x=0;x<width;++x)
   pixels[size_t(y)*width+x]=image.argb[size_t(y*image.height/height)*image.width+x*image.width/width];
 return {width,height,std::move(pixels),{}};
}

void save_snapshot(const fs::path& dir,Snapshot snapshot,DiagnosticPixels image){
 auto& meta=snapshot.metadata;
 if(!image.argb.empty()){
  try{
   meta["backbuffer_size"]={image.width,image.height};
   image=resize_snapshot(std::move(image));
   const auto bytes=encode_png(image.width,image.height,image.argb);
   const auto name=snapshot.stem+".png";
   write_file_atomic(dir/name,std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
   meta["image"]=name;
   meta["image_size"]={image.width,image.height};
   meta["capture_status"]="saved";
  }catch(const std::exception& e){image.error=e.what();}
 }
 if(!image.error.empty())meta["capture_status"]=image.error;
 write_file_atomic(dir/(snapshot.stem+".json"),meta.dump(2,-1,false,Json::error_handler_t::replace));
}
}

struct PerformanceSession::Impl {
 const fs::path dir;
 const std::string backend;
 const bool uncapped,optimized;
 const Clock::time_point start=Clock::now();
 Clock::time_point previous_frame{},sample_start{};
 std::ofstream csv,samples,stutters;
 Json system,adapter;
 PerformanceDropDetector detector;
 std::deque<HistoryFrame> history;
 std::optional<Snapshot> pending;
 std::future<void> writer;
 std::optional<CpuSample> previous_cpu;
 uint64_t window_presented=0,window_attempts=0,attempts=0,presented=0,drops=0;
 unsigned snapshots=0;
 double rolling_fps=0;
 bool previous_eligible=false,previous_presented=false,window_race_only=true,complete=false,write_error=false;
 UI::Page previous_page=UI::Loading;
 std::string previous_track;
 int previous_car=0;

 Impl(const fs::path& root,const char* name,bool unlimited,bool optimize):
  dir(root/"OpenFO_Diagnostics"/(std::string(name)+"_"+utc_stamp(true)+"_"+std::to_string(GetCurrentProcessId()))),
  backend(name),uncapped(unlimited),optimized(optimize){
  fs::create_directories(dir/"snapshots");
  csv.open(dir/"frames.csv",std::ios::binary);
  samples.open(dir/"samples.jsonl",std::ios::binary);
  stutters.open(dir/"stutters.jsonl",std::ios::binary);
  if(!csv||!samples||!stutters)throw std::runtime_error("Unable to open OpenFO performance logs");
  csv.imbue(std::locale::classic());
  csv<<std::fixed<<std::setprecision(3);
  csv<<"frame,elapsed_ms,interval_ms,instant_fps,rolling_fps,presented,page,simulation_ms,draw_ms,present_ms,"
       "snapshot_readback_ms,draw_calls,triangles,last_gpu_ms,gpu_sample_id\n";
  system={{"schema",1},{"engine","OpenFO Reboot 0.8.4"},{"backend",backend},
          {"session_started_utc",utc_stamp(false)},{"host",host_info()},
          {"snapshot_policy",{{"cooldown_ms",8000},{"warmup_frames",30},{"max_images",max_snapshots},
                              {"max_saved_width",1920},{"max_saved_height",1080}}}};
  save_system();
 }

 void save_system(){
  write_file_atomic(dir/"system.json",system.dump(2,-1,false,Json::error_handler_t::replace));
 }

 void write_json(std::ofstream& stream,const Json& value){
  stream<<value.dump(-1,-1,false,Json::error_handler_t::replace)<<'\n';
 }

 void flush(){
  csv.flush();samples.flush();stutters.flush();
  if((!csv||!samples||!stutters)&&!write_error){
   write_error=true;
   trace("performance_log_write_failed");
  }
 }

 void reap_writer(){
  if(!writer.valid())return;
  try{writer.get();}
  catch(const std::exception& e){trace("performance_snapshot_write_failed",e.what());}
 }

 void submit(Snapshot snapshot,DiagnosticPixels image){
  if(writer.valid()&&writer.wait_for(std::chrono::seconds(0))!=std::future_status::ready){
   // Never queue a second pixel buffer behind a slow disk.
   image={0,0,{},"previous_snapshot_still_writing"};
   save_snapshot(dir/"snapshots",std::move(snapshot),std::move(image));
   return;
  }
  reap_writer();
  writer=std::async(std::launch::async,[output=dir/"snapshots",snapshot=std::move(snapshot),image=std::move(image)]() mutable {
   save_snapshot(output,std::move(snapshot),std::move(image));
  });
 }

 void finish_pending(const char* reason){
  if(!pending)return;
  auto snapshot=std::exchange(pending,std::nullopt);
  submit(std::move(*snapshot),{0,0,{},reason});
 }

 void summary(bool clean){
  write_file_atomic(dir/"summary.json",Json({
   {"clean_exit",clean},{"rendered_attempts",attempts},{"successful_presents",presented},
   {"detected_drops",drops},{"capture_attempts",snapshots},{"session_ms",Ms(Clock::now()-start).count()}
  }).dump(2));
 }
};

PerformanceSession::PerformanceSession(const fs::path& dir,const char* backend,bool uncapped,bool optimized):
 p_(std::make_unique<Impl>(dir,backend,uncapped,optimized)){}

PerformanceSession::~PerformanceSession(){
 if(p_->complete)return;
 try{p_->finish_pending("session_ended_before_capture");p_->reap_writer();p_->flush();p_->summary(false);}
 catch(const std::exception& e){trace("performance_log_finalize_failed",e.what());}
}

const fs::path& PerformanceSession::directory()const{return p_->dir;}

void PerformanceSession::renderer_ready(const Renderer& renderer){
 p_->adapter=renderer.performance_adapter();
 p_->system["active_graphics_adapter"]=p_->adapter;
 p_->save_system();
}

double PerformanceSession::capture_pending(Renderer& renderer,const UI& ui,bool eligible,uint64_t frame)noexcept{
 if(!p_->pending)return 0;
 auto snapshot=std::exchange(p_->pending,std::nullopt);
 DiagnosticPixels image;
 double readback_ms=0;
 try{
  if(!eligible||snapshot->page!=ui.page||snapshot->track!=ui.track||snapshot->car!=ui.car)
   image.error="scene_changed_before_capture";
  else if(p_->writer.valid()&&p_->writer.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
   image.error="previous_snapshot_still_writing";
  else{
   const auto start=Clock::now();
   ++p_->snapshots;
   try{image=renderer.capture_performance_frame();}
   catch(const std::exception& e){image.error=e.what();}
   readback_ms=Ms(Clock::now()-start).count();
   snapshot->metadata["capture_frame"]=frame;
   snapshot->metadata["readback_ms"]=readback_ms;
  }
  p_->submit(std::move(*snapshot),std::move(image));
 }catch(const std::exception& e){trace("performance_snapshot_failed",e.what());}
 return readback_ms;
}

void PerformanceSession::record(const DiagnosticFrame& frame,const UI& ui,const State& state,int camera,const Renderer& renderer)noexcept{
 try{
  auto& p=*p_;
  const auto metrics=renderer.performance_metrics();
  const double elapsed=Ms(frame.end-p.start).count();
  const double interval=p.attempts?Ms(frame.end-p.previous_frame).count():0;
  if(!p.attempts){
   p.sample_start=frame.end;
   p.previous_cpu=cpu_sample();
  }
  if(p.attempts){
   ++p.window_attempts;
   p.window_presented+=frame.presented;
  }
  ++p.attempts;
  p.presented+=frame.presented;
  const bool same_scene=ui.page==p.previous_page&&ui.car==p.previous_car&&ui.track==p.previous_track;
  const bool comparable=frame.presented&&p.previous_presented&&same_scene;
  if(p.attempts>1)
   p.window_race_only=p.window_race_only&&same_scene&&ui.page==UI::Race&&frame.eligible&&
                      p.previous_eligible&&frame.capture_ms==0;
  const double instant_fps=comparable&&interval>0?1000./interval:0;
  const double sample_ms=Ms(frame.end-p.sample_start).count();
  if(sample_ms>=1000){
   p.rolling_fps=1000.*p.window_presented/sample_ms;
   const auto cpu=cpu_sample();
   p.write_json(p.samples,{{"elapsed_ms",elapsed},{"sample_ms",sample_ms},
                          {"frames_presented",p.window_presented},{"frames_attempted",p.window_attempts},{"fps",p.rolling_fps},
                          {"race_only",p.window_race_only},
                          {"page",page_name(ui.page)},{"track",ui.track},{"car_id",ui.car},
                          {"graphics",graphics_settings(ui,metrics,p.uncapped,p.optimized)},{"system_load",system_load(p.previous_cpu,cpu)}});
   p.previous_cpu=cpu;
   p.window_presented=p.window_attempts=0;
   p.window_race_only=true;
   p.sample_start=frame.end;
   p.flush();
  }
  p.csv<<frame.number<<','<<elapsed<<','<<interval<<','<<instant_fps<<','<<p.rolling_fps<<','
       <<frame.presented<<','<<page_name(ui.page)<<','<<frame.simulation_ms<<','<<frame.draw_ms<<','
       <<frame.present_ms<<','<<frame.capture_ms<<','<<metrics.draw_calls<<','<<metrics.triangles<<',';
  if(metrics.gpu_samples)p.csv<<metrics.gpu_ms;
  p.csv<<','<<metrics.gpu_samples<<'\n';
  const bool comparable_for_drop=comparable&&frame.eligible&&p.previous_eligible&&frame.capture_ms==0;
  if(!comparable_for_drop)p.history.clear();
  else{
   p.history.push_back({frame.number,elapsed,interval,frame.simulation_ms,frame.draw_ms,frame.present_ms});
   if(p.history.size()>history_limit)p.history.pop_front();
  }
  const auto drop=p.detector.observe(elapsed,interval,comparable_for_drop);
  p.previous_frame=frame.end;
  p.previous_presented=frame.presented;
  p.previous_eligible=frame.eligible;
  p.previous_page=ui.page;
  p.previous_car=ui.car;
  if(p.previous_track!=ui.track)p.previous_track=ui.track;
  if(drop==PerformanceDrop::None)return;
  ++p.drops;
  const auto stem="drop_"+std::to_string(p.drops);
  Json event={{"reason",performance_drop_name(drop)},{"trigger_frame",frame.number},{"elapsed_ms",elapsed},
              {"frame_interval_ms",interval},{"instant_fps",instant_fps},{"rolling_fps",p.rolling_fps},
              {"backend",p.backend},{"page",page_name(ui.page)},{"track",ui.track},{"car_id",ui.car},
              {"car_skin",ui.skin},{"camera",camera},{"race_time_s",state.time},{"speed_kmh",std::abs(state.speed)*3.6f},
              {"draw_calls",metrics.draw_calls},{"triangles",metrics.triangles},
              {"scene_cpu_ms",metrics.scene_cpu_ms},{"hud_cpu_ms",metrics.hud_cpu_ms},
              {"last_gpu_ms",metrics.gpu_samples?Json(metrics.gpu_ms):Json()},
              {"graphics",graphics_settings(ui,metrics,p.uncapped,p.optimized)}};
  if(p.snapshots>=max_snapshots){
   event["capture_status"]="session_image_limit";
   p.write_json(p.stutters,event);
   return;
  }
  event["snapshot_metadata"]="snapshots/"+stem+".json";
  p.write_json(p.stutters,event);
  auto history=Json::array();
  for(const auto& f:p.history)
   history.push_back({f.number,f.elapsed_ms,f.interval_ms,f.simulation_ms,f.draw_ms,f.present_ms});
  event["history_columns"]={"frame","elapsed_ms","interval_ms","simulation_ms","draw_ms","present_ms"};
  event["history"]=std::move(history);
  p.pending=Snapshot{stem,std::move(event),ui.page,ui.track,ui.car};
 }catch(const std::exception& e){trace("performance_log_record_failed",e.what());}
}

void PerformanceSession::finish()noexcept{
 try{
  p_->finish_pending("session_ended_before_capture");
  p_->reap_writer();
  p_->flush();
  p_->summary(true);
  p_->complete=true;
 }catch(const std::exception& e){trace("performance_log_finalize_failed",e.what());}
}
}
