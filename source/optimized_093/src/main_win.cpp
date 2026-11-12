#include "vehicle_camera_ini.hpp"
#define NOMINMAX
#include "alpha_driver_seat.hpp"
#include "alpha_track.hpp"
#include "menu_key_repeat.hpp"
#include "recovery_flow.hpp"
#include "startup_win.hpp"
#include "render_dx9.hpp"
#include "audio_win.hpp"
#include "music_win.hpp"
#include "frame_clock.hpp"
#include "performance_profile.hpp"
#include "cpu_parallel.hpp"
#include "display_modes_win.hpp"
#include "presentation_camera.hpp"
#include "race_camera.hpp"
#include "controls.hpp"
#include "content.hpp"
#include "captured_scene.hpp"
#include "track_editor.hpp"
#include "frontend.hpp"
#include "course_ui.hpp"
#include "race_participants.hpp"
#include "race_battle.hpp"
#include "legacy_ui/race_ui.hpp"
#include "vehicle_upgrades.hpp"
#include "vehicle_dynamics.hpp"
#include "replay.hpp"
#include "file_io.hpp"
#include "diagnostics_win.hpp"
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
#include "performance_diagnostics_win.hpp"
#endif
#define PSAPI_VERSION 2
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <future>
#include <atomic>
#include <xinput.h>
#include "lan_voice_win.hpp"
#include <mmsystem.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
extern "C" {__declspec(dllexport) DWORD NvOptimusEnablement=1;__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;}
using namespace ofo;
#ifdef OPENFO_OPENGL_RENDERER
constexpr const wchar_t* build_window_title=L"OpenFO Reboot 0.9.0 — OpenGL 3.3";
#else
constexpr const wchar_t* build_window_title=L"OpenFO Reboot 0.9.0 — Direct3D 9Ex";
#endif

namespace {
Renderer*renderer=nullptr;bool focused=true,running=true;int resize_w=1280,resize_h=720;bool needs_resize=false,mouse_click=false,display_modes_dirty=true;int mouse_x=0,mouse_y=0;std::vector<uint32_t> text_input;
LRESULT CALLBACK window_proc(HWND w,UINT m,WPARAM a,LPARAM b){switch(m){case WM_PAINT:if(startup_window_paint(w))return 0;break;case WM_ERASEBKGND:if(startup_visible(w))return 1;break;case WM_CHAR:if(text_input.size()<128)text_input.push_back(uint32_t(a));return 0;case WM_CLOSE:running=false;return 0;case WM_DESTROY:PostQuitMessage(0);return 0;case WM_ACTIVATEAPP:focused=a!=0;return 0;case WM_DISPLAYCHANGE:display_modes_dirty=true;return 0;case WM_DEVICECHANGE:display_modes_dirty=true;break;case WM_SIZE:if(a!=SIZE_MINIMIZED){resize_w=LOWORD(b);resize_h=HIWORD(b);needs_resize=true;if(startup_visible(w))InvalidateRect(w,nullptr,FALSE);}return 0;case WM_LBUTTONDOWN:mouse_click=true;mouse_x=short(LOWORD(b));mouse_y=short(HIWORD(b));return 0;case WM_SYSKEYDOWN:if(a==VK_RETURN)return 0;break;}return DefWindowProcW(w,m,a,b);}
bool down(int k){return focused&&(GetAsyncKeyState(k)&0x8000)!=0;}
struct Pad{HMODULE module=nullptr;using Get=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);Get get=nullptr;XINPUT_GAMEPAD state{};bool connected=false;Pad(){for(auto name:{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"})if((module=LoadLibraryW(name))){get=(Get)GetProcAddress(module,"XInputGetState");if(get)break;FreeLibrary(module);module=nullptr;}}~Pad(){if(module)FreeLibrary(module);}void poll(){XINPUT_STATE s{};state={};connected=focused&&get&&get(0,&s)==ERROR_SUCCESS;if(connected)state=s.Gamepad;}bool button(WORD b)const{return(state.wButtons&b)!=0;}float steer(float deadzone,float sensitivity)const{return controller_steering(state.sThumbLX/32767.f,deadzone,sensitivity);}};
Mat interpolate(const Mat&a,const Mat&b,float t){Mat m=identity();Vec f=unit(direction(a,{0,0,1})*(1-t)+direction(b,{0,0,1})*t,{0,0,1});Vec up=unit(direction(a,{0,1,0})*(1-t)+direction(b,{0,1,0})*t);Vec r=unit(cross(up,f),{1,0,0});up=unit(cross(f,r));for(int i=0;i<3;i++){m[i]=r[i];m[i+4]=up[i];m[i+8]=f[i];m[i+12]=a[i+12]*(1-t)+b[i+12]*t;}return m;}
State smooth(const State&a,const State&b,float t){State s=b;s.car=interpolate(a.car,b.car,t);for(int i=0;i<4;i++){s.wheels[i]=interpolate(a.wheels[i],b.wheels[i],t);s.wheel_mounts[i]=interpolate(a.wheel_mounts[i],b.wheel_mounts[i],t);}return s;}
Json process_memory(){PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m)))return {{"available",false}};return {{"available",true},{"working_set_bytes",m.WorkingSetSize},{"peak_working_set_bytes",m.PeakWorkingSetSize},{"private_commit_bytes",m.PrivateUsage}};}
void report(const fs::path&path,const Json&j){try{write_file_atomic(path,j.dump(2));}catch(const std::exception&e){OutputDebugStringA(e.what());}}
std::wstring wide(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
fs::path choose_data(HWND owner){auto com=OleInitialize(nullptr);BROWSEINFOW bi{};bi.hwndOwner=owner;bi.lpszTitle=L"Choose unpacked FlatOut data (cars, tracks, global, equipmnt)";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;auto item=SHBrowseForFolderW(&bi);wchar_t path[MAX_PATH]{};bool ok=item&&SHGetPathFromIDListW(item,path);if(item)CoTaskMemFree(item);if(SUCCEEDED(com))OleUninitialize();return ok?fs::path(path):fs::path{};}

}
int main(int argc,char**argv){fs::path exe_dir,report_dir;HWND window=nullptr;bool timer=false;StartupBitmap copyright;ExclusiveDisplayMode selected_display;try{wchar_t exe[32768];DWORD count=GetModuleFileNameW(nullptr,exe,32768);if(!count||count==32768)throw std::runtime_error("Executable path unavailable");exe_dir=fs::path(std::wstring(exe,count)).parent_path();report_dir=exe_dir;fs::path data=exe_dir/"data",initial_project;bool uncapped=false,explicit_data=false,present_probe=false,render_baseline=false;int count_args=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count_args);if(!args)throw std::runtime_error("Cannot read Unicode command line");for(int i=1;i<count_args;i++){std::wstring arg=args[i];if(arg==L"--data"){if(i+1>=count_args){LocalFree(args);throw std::runtime_error("--data requires a directory");}data=fs::path(args[++i]);explicit_data=true;}else if(arg==L"--track-project"){if(i+1>=count_args){LocalFree(args);throw std::runtime_error("--track-project requires an .oftrack file");}initial_project=fs::absolute(fs::path(args[++i]));}else if(arg==L"--present-probe")present_probe=true;else if(arg==L"--render-baseline")render_baseline=true;else if(arg==L"--uncapped")uncapped=true;else{LocalFree(args);throw std::runtime_error("Unknown argument");}}LocalFree(args);
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
#ifdef OPENFO_OPENGL_RENDERER
 const char* performance_backend="OpenGL";
#else
 const char* performance_backend="D3D9Ex";
#endif
 PerformanceSession performance(exe_dir,performance_backend,uncapped,!render_baseline);
 report_dir=performance.directory();
 std::cout<<"OpenFO performance logs: "<<report_dir.u8string()<<'\n';
#endif
 install_diagnostics(report_dir);
 const auto cpu_policy=detect_cpu_parallel_policy();
 auto diagnostic_report=[&](const char*name){return report_dir/(std::string(present_probe?"probe_":"")+name);};
 if(!explicit_data&&fs::exists(exe_dir/"data_path.txt")){auto bytes=read_file(exe_dir/"data_path.txt",65536);std::string saved(bytes.begin(),bytes.end());if(!saved.empty())data=fs::u8path(saved);}

 WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"OpenFOReboot";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register window class");const bool startup_fullscreen=!present_probe;
 RECT rect{0,0,1280,720};int x=CW_USEDEFAULT,y=CW_USEDEFAULT;
 if(startup_fullscreen){
  MONITORINFO mi{sizeof mi};
  if(GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&mi))rect=mi.rcMonitor;
  else rect={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
  x=rect.left;y=rect.top;
 }else AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
 window=CreateWindowW(wc.lpszClassName,L"OpenFO Reboot 0.9.0",startup_fullscreen?WS_POPUP:WS_OVERLAPPEDWINDOW,x,y,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);if(!window)throw std::runtime_error("CreateWindow failed");ShowWindow(window,SW_SHOW);UpdateWindow(window);
 SetWindowTextW(window,L"OpenFO — selecting and indexing game files");startup_paint(window,nullptr,L"LOADING...");
 if(fs::is_directory(data/"data"))data/="data";
 if(!fs::is_directory(data/"cars")||!fs::is_directory(data/"tracks")||!fs::exists(data/"scripts/cardealer.bed")||!fs::exists(data/"cars/car_3/body.bgm")){if(explicit_data)throw std::runtime_error("Invalid --data folder: cars and tracks required");auto selected=choose_data(window);if(selected.empty()){DestroyWindow(window);return 0;}data=fs::is_directory(selected/"data")?selected/"data":selected;}
 report(diagnostic_report("startup_status.json"),{{"phase","index_resources"},{"data_path",data.u8string()}});
 Resources resources(data);auto hd_count=resources.overlay_textures(exe_dir/"data");trace("hd_texture_overrides",std::to_string(hd_count));if(!present_probe){startup_sequence(window,exe_dir,data,resources,copyright,[&]{MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=false;TranslateMessage(&msg);DispatchMessageW(&msg);}return running;});if(!running){DestroyWindow(window);return 0;}mouse_click=false;text_input.clear();}resources.mount(exe_dir/"alpha_content","alpha_content");resources.mount(exe_dir/"hud_assets","hud_assets");resources.mount(exe_dir/"capture_assets","capture_assets");resources.mount(exe_dir/"track_edits","track_edits");UI ui;auto stages=stage_catalog(resources);auto cars=car_catalog(resources);if(cars.empty())throw std::runtime_error("No complete original car resources found");int selected_car=0;for(size_t i=0;i<cars.size();++i)if(cars[i].id==3)selected_car=int(i);ui.car=cars[selected_car].id;ui.car_name=cars[selected_car].name;ui.skin=cars[selected_car].skins.front();if(stages.empty())throw std::runtime_error("No playable stage found: geometry plus stage startpoints required");int selected_stage=0;for(size_t i=0;i<stages.size();++i)if(stages[i]=="tracks/forest/forest1/a")selected_stage=int(i);ui.track=stages[selected_stage];ui.label=ui.track;
 try{if(fs::exists(exe_dir/"openfo_settings.json")){auto bytes=read_file(exe_dir/"openfo_settings.json",65536);auto saved=Json::parse(bytes.begin(),bytes.end());ui.volume=std::clamp(saved.value("effects",.55f),0.f,1.f);ui.music=std::clamp(saved.value("music",.45f),0.f,1.f);ui.automatic=saved.value("automatic",true);}}catch(const std::exception&e){trace("settings_read_failed",e.what());}
 Json catalog={{"cars",Json::array()},{"stages",stages}};for(auto&c:cars)catalog["cars"].push_back({{"id",c.id},{"name",c.name},{"skins",c.skins}});report(exe_dir/"content_catalog.json",catalog);
 try{write_file_atomic(exe_dir/"data_path.txt",fs::absolute(data).u8string());}catch(const std::exception&e){OutputDebugStringA(e.what());}
 #ifdef OPENFO_OPENGL_RENDERER
 report(diagnostic_report("startup_status.json"),{{"phase","create_opengl"},{"resources",resources.paths().size()}});
#else
 report(diagnostic_report("startup_status.json"),{{"phase","create_direct3d9"},{"resources",resources.paths().size()}});
#endif
 ui.frontend=std::make_shared<Frontend>(resources,exe_dir/"profile",stage_geometry(ui.track));
 // Honor the profile's saved fullscreen choice. New installations keep native fullscreen.
 if(!fs::exists(ui.frontend->settings_path)){
  ui.frontend->fullscreen=startup_fullscreen;
  MONITORINFOEXW info{};info.cbSize=sizeof(info);
  if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),reinterpret_cast<MONITORINFO*>(&info))){
   DEVMODEW desktop{};desktop.dmSize=sizeof(desktop);
   if(EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&desktop,0)){
    ui.frontend->screen_width=int(desktop.dmPelsWidth);ui.frontend->screen_height=int(desktop.dmPelsHeight);
   }
  }
 }
 if(present_probe)ui.frontend->fullscreen=false;
 ui.frontend->set_display_resolutions(query_monitor_resolutions(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST)));
 ui.frontend->active_cpu_lanes=cpu_policy.lanes;
 ui.frontend->project_directory=exe_dir/"track_projects";
 ui.frontend->setup_master_server(exe_dir/"online_server.txt");
 Renderer render(window,resources,present_probe,!render_baseline);render.set_vsync(ui.frontend->vsync);SetWindowTextW(window,build_window_title);
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 performance.renderer_ready(render);
 SetWindowTextW(window,(std::wstring(build_window_title)+L" — Diagnostics").c_str());
#endif
 report(diagnostic_report("startup_status.json"),{{"phase","renderer_ready"},{"renderer",render.diagnostics()}});
 if(startup_visible(window)){InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);}
 renderer=&render;std::unique_ptr<Music>music;PresentedAudioScene audio_scene;std::vector<uint8_t> menu_cycle_source,menu_cycle_sound;try{for(auto path:{"sound/gearshift/menu_select.wav","sound/gearshift/menu_select_2.wav"})if(resources.exists(path)){menu_cycle_source=resources.read(path);menu_cycle_sound=menu_cycle_source;break;}}catch(const std::exception&){}struct StopMenuSound {~StopMenuSound(){PlaySoundW(nullptr,nullptr,0);}} stop_menu_sound;uint64_t played_navigation=0;Pad pad;LanVoice voice;std::unique_ptr<Simulation>simulation;std::unique_ptr<Audio>audio;Replay replay;std::unique_ptr<CourseDirector>course;std::unique_ptr<RaceBattle>battle;Json battle_results;
 Json cached_course_standings=Json::array(); double standings_last_update=-1.;
 std::optional<StuntSession> stunt;bool eject_pending=false;RaceEndFlow race_end;auto course_pose=[](const State&s){CoursePose p;p.chassis=s.car;p.speed_kmh=s.speed*3.6;p.driver_ejected=s.driver_ejected;return p;};std::vector<CoursePose>course_poses(1);State previous{},current{};for(auto&w:current.wheels)w=identity();previous=current;State last_display=current,photo_frozen=current;PhotoCamera photo_start;int camera=0;std::array<bool,256>keyboard_old{};bool fullscreen=startup_fullscreen;bool display_mode_applied=false;std::wstring display_device;int applied_width=ui.frontend->screen_width,applied_height=ui.frontend->screen_height;auto started=std::chrono::steady_clock::now(),last=started,report_start=started;FrameClock physics_clock;RaceStartClock start_clock;RecoveryFlow recovery;double off_route_time=0;bool automatic_recovery=false;double sim_ms=0,race_offset=0;unsigned frames=0,total_frames=0,total_presented=0;std::array<bool,16>old{};old[0]=down(VK_RETURN);keyboard_old[VK_RETURN]=down(VK_RETURN);timeBeginPeriod(1);timer=true;
 std::vector<RaceOpponentConfig> remote_cars;
 auto merge_peers=[&](State& state,const LanSession& session,bool colliders){
  auto opponents=std::make_shared<std::vector<OpponentState>>();
  std::vector<RemoteCollider> bodies;
  opponents->reserve(remote_cars.size());
  for(const auto& remote:remote_cars){
   const auto player=std::find_if(session.players().begin(),session.players().end(),[&](const auto& p){return p.id==remote.id;});
   OpponentState pose;pose.id=remote.id;pose.car_id=remote.car_id;pose.skin=remote.skin;pose.name=remote.name;
   pose.car=remote.spawn;pose.lan_remote=true;pose.connected=player!=session.players().end()&&player->connected;
   if(auto net=pose.connected?session.peer_pose(pose.id):std::nullopt){
    pose.car=net->car;pose.wheels=net->wheels;pose.velocity=net->velocity;pose.speed=length(net->velocity);
    pose.damage=net->damage;pose.recovery_generation=net->recovery_generation;pose.driver_ejected=net->ejected;pose.stunt_score=net->score;
   }else{
    if(!pose.connected)pose.damage=1;
    for(size_t i=0;i<pose.wheels.size();++i)
     pose.wheels[i]=multiply(pose.car,translate(remote.vehicle.wheels[i].mount));
   }
   opponents->push_back(std::move(pose));
   if(colliders&&opponents->back().connected){
    const auto& current=opponents->back();
    bodies.push_back({current.id,current.car,current.velocity,remote.vehicle.lo,remote.vehicle.hi,remote.vehicle.mass});
   }
  }
  if(colliders)simulation->set_remote_colliders(bodies);
  state.opponents=std::move(opponents);
 };
 struct Loaded {RaceMode mode=RaceMode::Circuit;std::optional<StuntSetup> stunt;TrackEnvironment environment;VehicleConfig cfg;Geometry track,body,tire;Json route;std::unique_ptr<Simulation> sim;std::shared_ptr<const NativeDriver>driver_model;int skin;std::string stage;std::vector<OpponentVisual> opponents;std::vector<RaceOpponentConfig> peers;};std::atomic<unsigned> cpu_load_progress{0};std::future<Loaded> loading;
 auto enter_photo=[&]{ui.photo.active=true;ui.photo.capture_requested=false;photo_frozen=last_display;photo_start=render.photo_camera();ui.photo.camera=photo_start;ui.photo.status.clear();};
 auto show_loading=[&](unsigned percent){ui.loading.progress=percent;render.draw(last_display,ui,camera);render.present();};
 auto load=[&]{
 ui.loading=race_loading_presentation(*ui.frontend,ui.track);
 ui.environment=ui.frontend->choice.environment;ui.environment.validate();ui.photo.active=false;replay.clear();
 audio_scene.loading();music.reset();audio.reset();PlaySoundW(nullptr,nullptr,0);
 ui.page=UI::Loading;ui.message.clear();cpu_load_progress.store(5);show_loading(5);
 simulation.reset();course.reset();battle.reset();battle_results=Json();cached_course_standings=Json::array();standings_last_update=-1.;stunt.reset();eject_pending=false;ui.stunt={};ui.event_intro=false;ui.event_hint.clear();race_end.reset();ui.race_finished=false;ui.race_results=false;int id=ui.car,skin=ui.skin;std::string stage=ui.track;auto upgrades=ui.frontend->car_upgrades(id);auto choice=ui.frontend->choice;const auto session=ui.frontend->lan;
 const bool lan_race=session&&session->phase()==LanSession::Phase::Loading;
 const auto lan_players=lan_race?session->players():std::vector<LanPlayer>{};
 const int local_id=lan_race?session->local_id():0;
 const auto mode=lan_race?session->rules().mode:ui.frontend->mode();MenuTrack stunt_slot;
 if(mode==RaceMode::Stunt){
  if(ui.frontend->track_slot>=ui.frontend->track_slots.size())throw std::runtime_error("Missing stunt selection");
  stunt_slot=ui.frontend->track_slots[ui.frontend->track_slot];
  if(stunt_slot.level!=choice.level)throw std::runtime_error("Mismatched stunt selection");
 }
 int car_class=choice.ai_profile_class;loading=std::async(std::launch::async,[&,id,skin,stage,upgrades,choice,car_class,mode,stunt_slot,lan_race,lan_players,local_id]{Loaded x;x.mode=mode;x.environment=choice.environment;x.environment.validate();x.skin=skin;x.stage=stage;if(mode==RaceMode::Circuit&&choice.track_project.empty())x.route=race_course(resources,stage);x.cfg=load_vehicle(resources,id);apply_selected_upgrades(x.cfg,resources,upgrades);apply_handling_difficulty(x.cfg,resources,choice.handling_difficulty);cpu_load_progress.store(16);trace("track_geometry_read",stage_geometry(stage));x.track=load_geometry(resources,stage_geometry(stage),stage);cpu_load_progress.store(38);trace("track_geometry_ready",geometry_summary(x.track).dump());trace("car_geometry_read",x.cfg.body);x.body=load_geometry(resources,x.cfg.body);trace("wheel_geometry_read",x.cfg.tire);x.tire=load_geometry(resources,x.cfg.tire);auto captured=append_captured_scene(x.track,resources);cpu_load_progress.store(51);Mat spawn=alpha_track_path(stage)?alpha_track_spawn(resources,stage,x.track):load_spawn(resources,stage);if(auto edit=lan_race?std::optional<TrackDocument>{}:choice.track_project.empty()?load_track_document(exe_dir,stage):std::optional<TrackDocument>(load_track_project(choice.track_project))){if(edit->stage!=stage)throw std::runtime_error("Track project base stage changed");bind_track_document(*edit,x.track,resources);apply_track_document(x.track,*edit);if(edit->route.size()>=2){
  auto custom=track_route_json(*edit);
  if(mode==RaceMode::Circuit&&custom.value("available",false))x.route=std::move(custom);
  if(!choice.track_project.empty()||(mode==RaceMode::Circuit&&track_project_has_circuit(*edit))){
   const auto p=edit->route[edit->start_index];
   const auto next=edit->route[(edit->start_index+1)%edit->route.size()];
   const auto v=next-p;
   spawn=multiply(translate(p+Vec{0,.6f,0}),rotate_y(std::atan2(v.x,v.z)));
  }
 }}if(mode==RaceMode::Stunt){x.stunt=load_stunt_setup(resources,x.track,stunt_slot);spawn=x.stunt->spawn;}
  if(lan_race){
  auto grid=[&](size_t slot){
   if(mode==RaceMode::Stunt)return spawn;
   if(!alpha_track_path(stage)&&slot>0){
    try{return load_spawn(resources,stage,int(slot)+1);}catch(const std::exception&){}
   }
   return lan_grid_spawn(spawn,int(slot),mode==RaceMode::Circuit);
  };
  Mat local_spawn=spawn;
  for(size_t i=0;i<lan_players.size();++i){
   const auto& player=lan_players[i];
   if(player.id==local_id){local_spawn=grid(i);continue;}
   RaceOpponentConfig c;c.id=player.id;c.car_id=player.car_id;c.skin=player.skin;c.name=player.name;
   c.vehicle=load_vehicle(resources,c.car_id);c.spawn=grid(i);c.behavior.native=Json::object();
   OpponentVisual visual;visual.config=c;visual.body=load_geometry(resources,c.vehicle.body);
   visual.tire=load_geometry(resources,c.vehicle.tire);
   x.opponents.push_back(std::move(visual));x.peers.push_back(std::move(c));
  }
  spawn=local_spawn;
 }
 int count=(lan_race||mode==RaceMode::FreeRoam||mode==RaceMode::Stunt||choice.time_trial)?0:std::clamp(choice.bots,mode==RaceMode::Battle?1:0,7);
 captured["complete_scene"]=geometry_summary(x.track);write_file_atomic(diagnostic_report("captured_scene_report.json"),captured.dump(2));x.driver_model=std::make_shared<const NativeDriver>(load_native_driver(resources,driver_model_from_id(choice.driver_gender)));const auto placement=load_driver_placement(resources,id,x.body,x.driver_model.get());add_alpha_driver_seat(resources,x.body,placement.seat);x.sim=std::make_unique<Simulation>(x.track,x.cfg,spawn,load_vehicle_panels(resources,x.body),x.driver_model,placement);for(int i=0;i<200;i++)x.sim->step({});cpu_load_progress.store(68);
 if(count){auto route=race_ai_route(x.route);route.derby=mode==RaceMode::Battle;if(!route.derby&&route.points.size()<4)throw std::runtime_error("Track has no native AI race route");auto roster=load_race_opponents(resources,car_class,choice.ai_handicap_level,choice.ai_upgrade_level,count,stage);for(auto& c:roster){OpponentVisual v;v.body=load_geometry(resources,c.vehicle.body);c.panels=load_vehicle_panels(resources,v.body);v.config=c;v.tire=load_geometry(resources,c.vehicle.tire);x.opponents.push_back(std::move(v));}x.sim->set_opponents(std::move(roster),std::move(route));}
 x.sim->set_grip_scale(x.environment.rain?.92f:1.f);cpu_load_progress.store(76);return x;});physics_clock.advance(0,false);};
 if(!initial_project.empty()){
  if(present_probe)throw std::runtime_error("Track project cannot be combined with a presentation probe");
  const auto project=load_track_project(initial_project);
  auto& menu=*ui.frontend;
  if(!resources.exists(stage_geometry(project.stage)))throw std::runtime_error("Track project base geometry is unavailable");
  auto car=std::find_if(menu.cars.begin(),menu.cars.end(),[&](const auto& entry){return entry.number==ui.car;});
  if(car!=menu.cars.end())menu.select_car(size_t(car-menu.cars.begin()));
  menu.select_track_project({initial_project,initial_project.stem().u8string(),project.stage,track_project_has_circuit(project)});
  ui.track=project.stage;ui.label=initial_project.stem().u8string();
  load();
 }
 std::array<MenuKeyRepeat,4>menu_repeat;
 while(running){auto frame_start=std::chrono::steady_clock::now();MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=false;TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running)break;if(needs_resize){trace("frame_resize");render.resize(resize_w,resize_h);needs_resize=false;}pad.poll();auto&menu=*ui.frontend;
 menu.poll_room_directory();if(menu.lan)menu.lan->poll();
 voice.update(menu.lan.get(),down(VK_F8));ui.lan_talking=menu.lan_talking=voice.transmitting();
 if(menu.page==MenuPage::Controls){
  auto axis=[](SHORT value){return std::clamp(value/32767.f,-1.f,1.f);};
  const auto& p=pad.state;
  menu.xbox={pad.connected,p.wButtons,axis(p.sThumbLX),axis(p.sThumbLY),axis(p.sThumbRX),axis(p.sThumbRY),
            p.bLeftTrigger/255.f,p.bRightTrigger/255.f,pad.steer(float(menu.controls.deadzone),float(menu.controls.sensitivity))};
 }auto held=[&](int action){
  auto active=[&](int key){return key&&(!menu.lan||key!=VK_F8)&&down(key);};
  return active(menu.controls.keys[action])||active(menu.controls.secondary[action]);
 };bool menu_navigation=ui.page!=UI::Race&&ui.page!=UI::Replay&&ui.page!=UI::Loading&&!ui.photo.active;std::array<bool,16>keys{down(VK_RETURN)||pad.button(XINPUT_GAMEPAD_A),down(VK_ESCAPE)||pad.button(XINPUT_GAMEPAD_START)||(ui.page!=UI::Race&&pad.button(XINPUT_GAMEPAD_B)),down(VK_LEFT)||pad.button(XINPUT_GAMEPAD_DPAD_LEFT)||(menu_navigation&&pad.state.sThumbLX<-18000),down(VK_RIGHT)||pad.button(XINPUT_GAMEPAD_DPAD_RIGHT)||(menu_navigation&&pad.state.sThumbLX>18000),down(VK_UP)||pad.button(XINPUT_GAMEPAD_DPAD_UP)||(menu_navigation&&pad.state.sThumbLY>18000),down(VK_DOWN)||pad.button(XINPUT_GAMEPAD_DPAD_DOWN)||(menu_navigation&&pad.state.sThumbLY<-18000),held(ControlSettings::Camera)||pad.button(XINPUT_GAMEPAD_Y),held(ControlSettings::Recover)||pad.button(XINPUT_GAMEPAD_B),down(VK_F11),held(ControlSettings::ShiftUp)||pad.button(XINPUT_GAMEPAD_RIGHT_SHOULDER),held(ControlSettings::ShiftDown)||pad.button(XINPUT_GAMEPAD_LEFT_SHOULDER),down('P')||down(VK_F3),down(VK_SPACE),down(VK_HOME),down('N'),down('F')};auto press=[&](int i){return keys[i]&&!old[i];};
 if(!menu_navigation||!focused)for(auto&r:menu_repeat)r.update(false,ui.clock);
 if(!focused&&ui.page==UI::Race){menu.open_pause();ui.page=UI::Pause;ui.row=0;physics_clock.advance(0,false);}
 if(press(8)){menu.fullscreen=!menu.fullscreen;menu.save_settings();}
 // Enumerate modes for the monitor currently containing the window, not a fixed preset list.
 HMONITOR current_monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
 std::wstring current_device;monitor_device_name(current_monitor,current_device);
 if(display_modes_dirty||display_device!=current_device){
  display_device=current_device;menu.set_display_resolutions(query_monitor_resolutions(current_monitor));display_modes_dirty=false;
 }
 if(menu.fullscreen!=fullscreen||menu.screen_width!=applied_width||menu.screen_height!=applied_height||(menu.fullscreen&&!display_mode_applied)){
  if(menu.fullscreen){
   if(!selected_display.apply(current_monitor,menu.screen_width,menu.screen_height)){
    menu.message="Requested monitor mode unavailable; switched to windowed mode";
    menu.fullscreen=false;menu.save_settings();
   }else{
    MONITORINFO mi{sizeof mi};GetMonitorInfoW(current_monitor,&mi);
    SetWindowLongPtrW(window,GWL_STYLE,WS_POPUP|WS_VISIBLE);
    SetWindowPos(window,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);
    display_mode_applied=true;
   }
  }
  if(!menu.fullscreen){
   selected_display.restore();display_mode_applied=false;
   MONITORINFO mi{sizeof mi};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&mi);
   SetWindowLongPtrW(window,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
   RECT client{0,0,menu.screen_width,menu.screen_height};AdjustWindowRect(&client,WS_OVERLAPPEDWINDOW,FALSE);
   int w=std::min(int(client.right-client.left),int(mi.rcWork.right-mi.rcWork.left));
   int h=std::min(int(client.bottom-client.top),int(mi.rcWork.bottom-mi.rcWork.top));
   SetWindowPos(window,nullptr,mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-w)/2,mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-h)/2,w,h,SWP_FRAMECHANGED|SWP_NOZORDER);
  }
  fullscreen=menu.fullscreen;applied_width=menu.screen_width;applied_height=menu.screen_height;
 }

 bool photo_transition=false;bool photo_consumed=ui.photo.active;bool photo_toggle=down(VK_F6)&&!keyboard_old[VK_F6];if(photo_toggle&&menu.binding<0&&!menu.lan&&simulation&&(ui.page==UI::Race||ui.page==UI::Pause||ui.page==UI::Replay)){if(ui.photo.active){ui.photo.active=false;ui.photo.capture_requested=false;}else enter_photo();photo_transition=true;photo_consumed=true;}if(ui.photo.active&&press(1)){ui.photo.active=false;ui.photo.capture_requested=false;photo_transition=true;photo_consumed=true;}
 if(ui.photo.active&&focused){auto edge=[&](int key){return down(key)&&!keyboard_old[key];};PhotoInput photo_input;photo_input.move_x=float(down('D'))-float(down('A'));photo_input.move_z=float(down('W'))-float(down('S'));photo_input.move_y=float(down('E'))-float(down('Q'));photo_input.yaw=(float(down(VK_RIGHT))-float(down(VK_LEFT)))*1.2f;photo_input.pitch=(float(down(VK_UP))-float(down(VK_DOWN)))*1.2f;photo_input.roll=(float(down('X'))-float(down('Z')))*.7f;photo_input.zoom=(float(down(VK_NEXT))-float(down(VK_PRIOR)))*40;photo_input.fast=down(VK_SHIFT);if(down(VK_RBUTTON)){RECT rect;GetClientRect(window,&rect);POINT center{(rect.left+rect.right)/2,(rect.top+rect.bottom)/2},cursor;ClientToScreen(window,&center);GetCursorPos(&cursor);if(keyboard_old[VK_RBUTTON]){photo_input.mouse_x=float(cursor.x-center.x);photo_input.mouse_y=float(cursor.y-center.y);}SetCursorPos(center.x,center.y);}photo_camera_step(ui.photo.camera,photo_input,std::chrono::duration<double>(frame_start-last).count());if(edge('C'))ui.photo.settings.preset=(ui.photo.settings.preset+1)%6;if(edge('1'))ui.photo.settings.exposure-=.25f;if(edge('2'))ui.photo.settings.exposure+=.25f;if(edge('3'))ui.photo.settings.contrast-=.1f;if(edge('4'))ui.photo.settings.contrast+=.1f;if(edge('5'))ui.photo.settings.vignette-=.1f;if(edge('6'))ui.photo.settings.vignette+=.1f;if(edge('7'))ui.photo.settings.saturation-=.1f;if(edge('8'))ui.photo.settings.saturation+=.1f;if(edge(VK_TAB))ui.photo.panel=!ui.photo.panel;if(edge(VK_HOME)){ui.photo.camera=photo_start;ui.photo.settings={};}photo_sanitize(ui.photo.settings);if(edge(VK_F12))ui.photo.capture_requested=true;}
 bool change_time=photo_transition;try{
 if(!photo_consumed){
 if(ui.page==UI::Race){
  if(press(0)&&ui.event_intro){ui.event_intro=false;change_time=true;}
  else if(press(0)&&stunt&&stunt->display().attempt_done){
   if(stunt->next_attempt()){
    simulation->reset_stunt_attempt(stunt->initial_props());
    for(int i=0;i<200;++i)simulation->step({});
    current=simulation->state();previous=current;race_offset=current.time;
    start_clock.reset();recovery.reset();off_route_time=0;automatic_recovery=false;replay.clear();eject_pending=false;change_time=true;
   }else{
    const auto& result=stunt->display();
    menu.complete_career_stunt(result.medal,result.award,result.total);
    if(menu.choice.career_race&&!menu.profile_save_ok)ui.message="Career result not saved: check write access to profile folder";
    else{
     simulation.reset();stunt.reset();audio.reset();menu.pause_context=false;
     menu.page=menu.lan?MenuPage::LanStart:menu.career?MenuPage::Garage:MenuPage::RaceSetup;if(menu.lan){menu.lan->close();menu.lan.reset();}menu.row=0;ui.page=UI::Select;change_time=true;
    }
   }
  }
  if(((course&&course->finished())||(battle&&battle->finished()))&&press(0)){if(race_end.stage==RaceEndStage::Finish)race_end.show_results();else if(menu.choice.career_race){if(menu.profile_save_ok){simulation.reset();audio.reset();course.reset();battle.reset();menu.pause_context=false;menu.page=MenuPage::CareerAward;menu.row=0;ui.page=UI::Select;change_time=true;}else ui.message="Career result not saved: check write access to profile folder";}else if(!menu.lan&&replay.available()){replay.begin();ui.page=UI::Replay;change_time=true;}}if(press(11)&&!menu.lan&&replay.available()){replay.begin();ui.page=UI::Replay;change_time=true;}else if(press(1)){menu.open_pause();ui.page=UI::Pause;change_time=true;}if(press(6)&&ui.countdown<=0)camera=next_race_camera(camera,2+std::max(1,int(load_vehicle_cameras_ini(resources,ui.car).size())));if(press(7)&&simulation&&!stunt&&!ui.event_intro&&!ui.photo.active&&ui.countdown<=0&&!ui.race_finished){automatic_recovery=false;recovery.request();}}
 else if(ui.page==UI::Replay){if(press(1)||press(11)){menu.open_pause();menu.row=4;ui.page=UI::Pause;change_time=true;}if(press(2))ui.replay_selected=(ui.replay_selected+4)%5;if(press(3))ui.replay_selected=(ui.replay_selected+1)%5;if(press(12))replay.toggle();if(press(13))replay.rewind();if(press(4))replay.speed(replay.speed()*2);if(press(5))replay.speed(replay.speed()*.5f);if(press(9))replay.seek(5);if(press(10))replay.seek(-5);if(press(0)){if(ui.replay_selected==0){menu.open_pause();menu.row=4;ui.page=UI::Pause;change_time=true;}if(ui.replay_selected==1)replay.rewind();if(ui.replay_selected==2)replay.toggle();if(ui.replay_selected==3)replay.speed(replay.speed()*2);if(ui.replay_selected==4)replay.speed(replay.speed()*.5f);}}
 else if(ui.page!=UI::Loading){const bool choosing_profile=menu.page==MenuPage::Profile;const bool session_fullscreen=menu.fullscreen;menu.clear_actions();if(menu.binding>=0){if(press(1))menu.key(VK_ESCAPE);else for(int k=1;k<256;k++)if(down(k)&&!keyboard_old[k]){menu.key(k);break;}}else{if(press(0))menu.key(VK_RETURN);if(press(1))menu.key(VK_ESCAPE);if(menu_repeat[0].update(keys[2],ui.clock))menu.key(VK_LEFT);if(menu_repeat[1].update(keys[3],ui.clock))menu.key(VK_RIGHT);if(menu_repeat[2].update(keys[4],ui.clock))menu.key(VK_UP);if(menu_repeat[3].update(keys[5],ui.clock))menu.key(VK_DOWN);if(down(VK_DELETE)&&!keyboard_old[VK_DELETE])menu.key(VK_DELETE);if(down(VK_TAB)&&!keyboard_old[VK_TAB])menu.key(down(VK_SHIFT)?VK_PRIOR:VK_TAB);if((down(VK_NEXT)&&!keyboard_old[VK_NEXT])||press(9))menu.key(VK_NEXT);if((down(VK_PRIOR)&&!keyboard_old[VK_PRIOR])||press(10))menu.key(VK_PRIOR);}if(mouse_click){RECT rect;GetClientRect(window,&rect);menu.click(mouse_x,mouse_y,rect.right,rect.bottom);}for(auto c:text_input){if(menu.lan_page())menu.lan_character(c);else menu.profile_character(c);}
 if(menu.lan&&menu.lan->take_load_request()){if(!menu.apply_lan_match())throw std::runtime_error(menu.message);menu.start=true;}if(choosing_profile&&menu.page==MenuPage::Main)menu.fullscreen=session_fullscreen;if(menu.quit)running=false;
 ui.car=menu.selected_car_id();ui.car_name=menu.cars.empty()?"":menu.cars[menu.car_index].name;ui.skin=menu.choice.skin;ui.track=menu.selected_stage();ui.label=ui.track;ui.automatic=menu.choice.automatic;
 if(menu.start||menu.restart){if(menu.restart)menu.cancel_career_race();if(!menu.restart||!menu.lan)if(menu.begin_career_race()){load();change_time=true;}}else if(menu.retire){menu.cancel_career_race();simulation.reset();audio.reset();course.reset();battle.reset();stunt.reset();ui.event_intro=false;menu.pause_context=false;menu.page=menu.lan?MenuPage::LanStart:menu.career?MenuPage::Garage:MenuPage::SinglePlayer;if(menu.lan){menu.lan->close();menu.lan.reset();}ui.page=UI::Select;change_time=true;}else if(menu.resume){ui.page=UI::Race;change_time=true;}else if(menu.photo&&simulation&&!menu.lan){enter_photo();menu.clear_actions();change_time=true;}else if(menu.replay){if(!menu.lan&&replay.available()){replay.begin();ui.page=UI::Replay;change_time=true;}else menu.message="No replay recorded yet";}else ui.page=simulation&&menu.pause_context?UI::Pause:UI::Select;
 }
 }}catch(const std::exception&e){ui.page=UI::Select;menu.pause_context=false;menu.page=alpha_track_path(ui.track)?MenuPage::AlphaTracks:MenuPage::RaceSetup;if(menu.lan){menu.lan->close();menu.lan.reset();menu.page=MenuPage::LanStart;}menu.message=e.what();ui.message=e.what();trace("load_failed",e.what());menu.cancel_career_race();simulation.reset();audio.reset();change_time=true;}if(menu.navigation_serial!=played_navigation){played_navigation=menu.navigation_serial;if(ui.page!=UI::Loading&&(audio_scene.ready(AudioScene::Menu)||audio_scene.ready(AudioScene::Race))&&!menu_cycle_source.empty()&&menu.effects_volume>0){
 // Stop before replacing the asynchronous PCM buffer. Apply the profile's
 // effects gain to PCM data, without changing system-wide waveOut volume.
 PlaySoundW(nullptr,nullptr,0);menu_cycle_sound=menu_cycle_source;float gain=std::clamp(menu.effects_volume/100.f,0.f,1.f);size_t offset=12;unsigned bits=0,format=0;
 auto u16=[&](size_t n){return unsigned(menu_cycle_sound[n])|unsigned(menu_cycle_sound[n+1])<<8;};
 auto u32=[&](size_t n){return uint32_t(menu_cycle_sound[n])|uint32_t(menu_cycle_sound[n+1])<<8|uint32_t(menu_cycle_sound[n+2])<<16|uint32_t(menu_cycle_sound[n+3])<<24;};
 while(offset+8<=menu_cycle_sound.size()){auto n=u32(offset+4);size_t first=offset+8;if(n>menu_cycle_sound.size()-first)break;
 if(std::memcmp(menu_cycle_sound.data()+offset,"fmt ",4)==0&&n>=16){format=u16(first);bits=u16(first+14);}
 if(std::memcmp(menu_cycle_sound.data()+offset,"data",4)==0&&format==1){if(bits==16)for(size_t j=first;j+1<first+n;j+=2){int16_t value=int16_t(u16(j));value=int16_t(value*gain);menu_cycle_sound[j]=uint8_t(value);menu_cycle_sound[j+1]=uint8_t(uint16_t(value)>>8);}else if(bits==8)for(size_t j=first;j<first+n;++j)menu_cycle_sound[j]=uint8_t(128+(int(menu_cycle_sound[j])-128)*gain);}
 offset=first+n+(n&1);}
 PlaySoundW(reinterpret_cast<LPCWSTR>(menu_cycle_sound.data()),nullptr,SND_ASYNC|SND_MEMORY|SND_NODEFAULT);
 }}mouse_click=false;text_input.clear();for(int k=0;k<256;k++)keyboard_old[k]=down(k);

 if(ui.page==UI::Loading)ui.loading.progress=cpu_load_progress.load();
 if(ui.page==UI::Loading&&loading.valid()&&loading.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{
 auto x=loading.get();ui.environment=x.environment;show_loading(76);
 render.scene(x.track,x.body,x.tire,x.cfg,x.skin,x.stage,x.driver_model);show_loading(89);
 render.opponents(x.opponents);show_loading(95);current=x.sim->state();previous=current;race_offset=current.time;simulation=std::move(x.sim);simulation->set_cpu_lanes(cpu_policy.lanes);ui.mode=x.mode;
 remote_cars=std::move(x.peers);
 if(menu.lan&&menu.lan->phase()==LanSession::Phase::Loading){
  merge_peers(current,*menu.lan,false);previous=current;menu.lan->loaded();
 }
 if(x.mode==RaceMode::Circuit&&x.route.value("available",false))course=std::make_unique<CourseDirector>(x.route,1+x.opponents.size(),menu.choice.laps,int(x.opponents.size()));
 if(x.mode==RaceMode::Battle)battle=std::make_unique<RaceBattle>(1+x.opponents.size());
 if(x.stunt)stunt.emplace(std::move(*x.stunt),menu.bed_settings.stunts);
 ui.event_intro=!menu.lan&&(bool(battle)||bool(stunt)||arena_circuit(x.stage));
 if(ui.event_intro)ui.event_hint=event_intro_hint(resources,x.mode,stunt?stunt->display().kind:StuntKind::None,menu.language);
 ui.message.clear();course_poses=participant_poses(current);race_end.reset();audio=std::make_unique<Audio>(resources,ui.car,x.track,x.stage);show_loading(100);ui.page=focused?UI::Race:UI::Pause;start_clock.reset();if(menu.choice.bonus_free_roam)start_clock.advance(3,true);recovery.reset();off_route_time=0;automatic_recovery=false;ui.race_finished=false;ui.race_results=false;ui.off_route_distance=-1;ui.countdown=start_clock.remaining();menu.clear_actions();if(!focused)menu.open_pause();ui.row=0;change_time=true;}catch(const std::exception&e){ui.page=UI::Select;menu.pause_context=false;menu.page=alpha_track_path(ui.track)?MenuPage::AlphaTracks:MenuPage::RaceSetup;if(menu.lan){menu.lan->close();menu.lan.reset();menu.page=MenuPage::LanStart;}menu.message=e.what();ui.message=e.what();trace("load_failed",e.what());menu.cancel_career_race();simulation.reset();audio.reset();change_time=true;}}
 ui.volume=menu.effects_volume*.01f;ui.music=(simulation?menu.race_music_volume:menu.music_volume)*.01f;
 const auto next_audio_scene=ui.page==UI::Loading?AudioScene::Silent:simulation?AudioScene::Race:AudioScene::Menu;
 const bool skip_music=press(14);
 if(music&&(!focused||!audio_scene.ready(next_audio_scene)))music->update(next_audio_scene==AudioScene::Race,ui.music,false,false);
 const bool manual_eject=press(15)&&!stunt;const int shift=int(press(9))-int(press(10));old=keys;if(total_frames<3)trace("frame_controls_done");auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-last).count();last=now;ui.clock=std::chrono::duration<double>(now-started).count();Input input;input.steer=std::clamp(float(held(ControlSettings::Right))-float(held(ControlSettings::Left))+pad.steer(float(menu.controls.deadzone),float(menu.controls.sensitivity)),-1.f,1.f);input.throttle=std::max(float(held(ControlSettings::Gas)),pad.state.bRightTrigger/255.f);input.brake=std::max(float(held(ControlSettings::Reverse)||held(ControlSettings::Brake)),pad.state.bLeftTrigger/255.f);direction_pedals(input,current);input.handbrake=held(ControlSettings::Handbrake)||pad.button(XINPUT_GAMEPAD_X);input.nitro=held(ControlSettings::Nitro)||pad.button(XINPUT_GAMEPAD_A);input.automatic=ui.automatic;input.shift=shift;if((course&&course->finished())||(battle&&battle->finished())){input.throttle=0;input.brake=1;input.nitro=false;}
 auto sim_start=std::chrono::steady_clock::now();
 const bool network_race=simulation&&menu.lan&&!remote_cars.empty();
 const bool lan_waiting=network_race&&menu.lan->phase()!=LanSession::Phase::Playing;
 if(lan_waiting)ui.message=menu.lan->phase()==LanSession::Phase::Failed?menu.lan->error():"WAITING FOR LAN PLAYERS";
 else if(ui.message=="WAITING FOR LAN PLAYERS")ui.message.clear();
 if(network_race&&!lan_waiting)merge_peers(current,*menu.lan,false);
 const bool event_active=!lan_waiting&&ui.page==UI::Race&&simulation&&focused&&!ui.photo.active&&!ui.event_intro&&(!stunt||!stunt->display().attempt_done)&&(!battle||!battle->finished());
 double race_dt=start_clock.advance(dt,event_active&&!change_time);
 ui.countdown=start_clock.remaining();
 const bool driving=event_active&&ui.countdown<=0;
 if(!driving)eject_pending=false;
 else if(manual_eject)eject_pending=true;
 if(stunt){
  if(driving&&!change_time)stunt->aim(input.nitro,race_dt);
  else stunt->cancel_aim();
  input.nitro=false;
 }
 int steps=physics_clock.advance(race_dt,driving,change_time,stunt?stunt->time_scale():1.);
 for(int i=0;i<steps;i++){previous=current;
 if(network_race&&!stunt)merge_peers(current,*menu.lan,true);
 input.eject=eject_pending;eject_pending=false;
 if(stunt)stunt->launch_input(input);
 const bool result_locked=(course&&course->finished())||(battle&&(battle->finished()||current.damage>=.999f));
 simulation->set_prop_rewards_enabled(!result_locked&&!stunt);
 if(course&&!result_locked&&course->off_route()&&!current.driver_ejected){
  off_route_time+=.01;
  if(off_route_time>=3&&!recovery.active()){automatic_recovery=true;recovery.request();off_route_time=0;}
 }else{off_route_time=0;if(automatic_recovery){automatic_recovery=false;if(!current.driver_ejected)recovery.cancel();}}
 if(recovery.active()){input.throttle=0;input.brake=1;input.steer=0;input.nitro=false;input.eject=false;}simulation->step(input);input.shift=0;input.eject=false;current=simulation->state();if(recovery.step(.01,current.driver_ejected,(course&&course->finished())||(battle&&(battle->finished()||current.damage>=.999f)),!stunt.has_value())){bool restored=false;if(course){auto target=course->departure_transform();const bool anchored=bool(target);if(!target)target=course->recovery_transform(course_pose(current));if(target){restored=course->recovery_candidate_valid(*target)&&simulation->recover(*target,anchored);for(float distance:{4.f,8.f,12.f}){if(restored)break;for(float side:{-1.f,1.f}){auto nearby=*target;nearby[12]+=side*distance*nearby[8];nearby[14]+=side*distance*nearby[10];if(course->recovery_candidate_valid(nearby)&&simulation->recover(nearby,anchored)){restored=true;break;}}}}}else if(!battle||(!battle->finished()&&current.damage<.999f)){simulation->recover();restored=true;}automatic_recovery=false;off_route_time=0;
 if(restored){ui.message.clear();current=simulation->state();previous=current;if(course)course->relocated(0,course_pose(current));}
 else ui.message="RETURN POINT BLOCKED — RETRY";}if(network_race)merge_peers(current,*menu.lan,false);if(course){course_poses=participant_poses(current);if(current.opponents&&previous.opponents)for(size_t b=0;b<std::min(current.opponents->size(),previous.opponents->size());++b)if((*current.opponents)[b].recovery_generation!=(*previous.opponents)[b].recovery_generation)course->relocated(b+1,course_poses[b+1]);course->update(.01,course_poses);if(current.opponents)for(size_t b=0;b<current.opponents->size();++b)if(course->finished(b+1))simulation->set_opponent_finished(b); }if(battle){battle->update(.01,current);if(current.opponents)for(size_t b=0;b<current.opponents->size();++b)if((*current.opponents)[b].damage>=.999f)simulation->set_opponent_finished(b);}if(stunt){stunt->update(current,.01);if(stunt->display().attempt_done){previous=current;break;}}
 else{
  replay.record(current);
  if(battle&&battle->finished()){previous=current;break;}
 }}
 if(stunt)ui.stunt=stunt->display();
 if(network_race&&!lan_waiting){
  LanPose pose;pose.car=current.car;pose.wheels=current.wheels;pose.velocity=current.velocity;
  pose.damage=current.damage;pose.recovery_generation=current.recovery_generation;
  pose.ejected=current.driver_ejected;pose.finished=ui.race_finished;
  pose.score=stunt?stunt->display().total:0;
  menu.lan->publish(pose);
 }
 sim_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-sim_start).count();if(audio){auto sound_state=current;if(ui.race_finished)sound_state.nitro_active=false;audio->update(sound_state,input,ui.volume,audio_scene.ready(AudioScene::Race)&&ui.page==UI::Race&&focused&&!ui.photo.active,ui.countdown<=0&&!ui.event_intro);}if(ui.page==UI::Replay&&focused&&!ui.photo.active)replay.tick(dt);State display=ui.photo.active?photo_frozen:ui.page==UI::Replay?replay.sample():ui.page==UI::Race?smooth(previous,current,physics_clock.alpha()):current;if(!ui.photo.active)display.time=std::max(0.,display.time-race_offset);last_display=display;ui.race_state=Json::object();ui.guidance_available=false;ui.off_route_distance=-1;if(course){auto states=course->summary();if(!states.empty()){ui.race_state=states.at(0);ui.race_finished=ui.race_state.value("finished",false);if(ui.page==UI::Race&&focused&&!ui.photo.active)race_end.update(ui.race_finished,std::clamp(dt,0.,.1));ui.race_results=race_end.stage==RaceEndStage::Results;ui.finish_age=race_end.age;}course_poses=participant_poses(current);
 if(gpu_display_sample_due(ui.clock,standings_last_update,ui.race_finished||cached_course_standings.empty())){
  cached_course_standings=name_standings(course->leaderboard(course_poses),current);
  for(auto&row:cached_course_standings){
   int model=row.value("car",0)==0?ui.car:row.value("car_id",0);
   for(const auto&c:menu.cars)if(c.number==model){row["car_name"]=c.name;break;}
   if(row.value("car",0)==0)row["name"]=menu.profile_name;
  }
  standings_last_update=ui.clock;
 }
 const auto& standings=cached_course_standings;
 ui.race_state["standings"]=standings;
 if(ui.race_finished)menu.complete_career_race(standings,current.prop_hits,current.prop_bonus,current.prop_hits_by_type,current.prop_bonus_cents_by_type);if(ui.page==UI::Race&&!ui.race_finished)ui.off_route_distance=course->off_route_distance(course_pose(display));auto guide=course->guidance(0,course_pose(display));ui.guidance_available=guide.available;ui.guidance_yaw=float(guide.yaw);
 if(ui.off_route_distance>=0)if(auto anchor=course->departure_transform()){
  auto delta=point(*anchor,{})-point(display.car,{});
  const auto right=direction(display.car,{1,0,0}),forward=direction(display.car,{0,0,1});
  ui.guidance_yaw=std::atan2(dot(delta,right),dot(delta,forward));ui.guidance_available=true;
 }}if(battle){
  ui.race_state["time"]=battle->elapsed();
  ui.race_state["survivors"]=battle->survivors();
  ui.race_state["participants"]=1+(current.opponents?current.opponents->size():0);
  ui.race_finished=battle->finished();
  if(ui.page==UI::Race&&focused&&!ui.photo.active)
   race_end.update(ui.race_finished,std::clamp(dt,0.,.1));
  ui.race_results=race_end.stage==RaceEndStage::Results;
  ui.finish_age=race_end.age;
  if(ui.race_finished){
   if(battle_results.is_null()){
    battle_results=name_standings(battle->standings(current),current);
    for(auto& row:battle_results){
     if(row.value("car",0)==0)row["name"]=menu.profile_name;
    }
   }
   ui.race_state["standings"]=battle_results;
   menu.complete_career_race(battle_results,current.prop_hits,current.prop_bonus,current.prop_hits_by_type,current.prop_bonus_cents_by_type);
  }
 }
 ui.race_state["prop_hits"]=current.prop_hits;ui.race_state["prop_bonus"]=current.prop_bonus;ui.race_state["off_route_distance"]=ui.off_route_distance;ui.recovery_countdown=ui.page==UI::Race&&!ui.race_finished?recovery.remaining():0;ui.replay_state=replay.diagnostics();ui.music_state=music?music->diagnostics():Json::object();if(total_frames<3)trace("frame_draw_begin");
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 const auto perf_draw_start=std::chrono::steady_clock::now();
#endif
 render.set_vsync(menu.vsync);render.draw(display,ui,camera);
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 const auto perf_draw_end=std::chrono::steady_clock::now();
 const bool active_menu=!simulation&&ui.page!=UI::Loading&&!startup_visible(window);
 const bool active_race=event_active&&ui.countdown<=0&&ui.recovery_countdown<=0&&!ui.race_finished;
 const bool perf_eligible=focused&&!present_probe&&!ui.photo.active&&
                          (active_menu||active_race||ui.page==UI::Replay);
 const auto perf_capture_ms=performance.capture_pending(render,ui,perf_eligible,total_frames+1);
#endif
 if(ui.photo.active&&ui.photo.capture_requested){try{auto path=render.save_photo(exe_dir/"Photos");ui.photo.status="Saved: "+path.filename().u8string();}catch(const std::exception&e){ui.photo.status=std::string("Save failed: ")+e.what();trace("photo_capture_failed",e.what());}ui.photo.capture_requested=false;}if(total_frames<3)trace("frame_present_begin");
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 const auto perf_present_start=std::chrono::steady_clock::now();
#endif
 bool presented=render.present();
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 const auto perf_end=std::chrono::steady_clock::now();
 using FrameMs=std::chrono::duration<double,std::milli>;
 performance.record({uint64_t(total_frames)+1,perf_end,sim_ms,FrameMs(perf_draw_end-perf_draw_start).count(),
                     FrameMs(perf_end-perf_present_start).count(),perf_capture_ms,presented,perf_eligible},
                    ui,display,camera,render);
#endif
 if(presented&&next_audio_scene!=AudioScene::Silent&&!present_probe){
  if(startup_visible(window)&&next_audio_scene==AudioScene::Menu){
   startup_finish(window);
   report(diagnostic_report("startup_status.json"),{{"phase","menu_presented"},{"renderer",render.diagnostics()}});
  }
  audio_scene.presented(next_audio_scene);
  if(!music)music=std::make_unique<Music>(resources);
 }
 if(music)music->update(next_audio_scene==AudioScene::Race,ui.music,focused&&audio_scene.ready(next_audio_scene),skip_music);
 if(total_frames<3){trace(presented?"frame_present_done":"frame_present_deferred");report(diagnostic_report("first_frame_report.json"),{{"frame",total_frames},{"presented",presented},{"renderer",render.diagnostics()}});}if(presented){++frames;++total_presented;}++total_frames;
 double interval=std::chrono::duration<double>(now-report_start).count();if(interval>=2){Json j={{"build","OpenFO Reboot 0.9.0"},{"fps",frames/interval},{"simulation_cpu_ms",sim_ms},{"dropped_simulation_wall_seconds",physics_clock.dropped()},{"renderer",render.diagnostics()},{"paused",ui.page!=UI::Race||ui.photo.active},{"physics_hz",100},{"cpu_parallel",{{"physical_cores",cpu_policy.eligible_physical},{"logical_processors",cpu_policy.eligible_logical},{"physical_detected",cpu_policy.physical_detected},{"ai_lanes",cpu_policy.lanes},{"workers",cpu_policy.lanes-1}}},{"data_path",data.u8string()}};j["process_memory"]=process_memory();j["replay"]=replay.diagnostics();if(course)j["course"]=course->summary();j["content"]={{"cars",cars.size()},{"stages",stages.size()}};if(simulation)j["simulation"]=simulation->diagnostics();if(audio)j["audio"]=audio->diagnostics();if(music)j["music"]=music->diagnostics();report(diagnostic_report("runtime_report.json"),j);frames=0;report_start=now;}
 if(!uncapped)std::this_thread::sleep_until(frame_start+std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0/std::clamp(menu.fps_limit,30,200))));
 if(!focused&&!present_probe)Sleep(85);if(present_probe&&total_frames>=120){if(total_presented!=120)throw std::runtime_error("Present probe did not display all 120 frames");running=false;}
 }
 if(!ui.frontend->fullscreen){RECT client;GetClientRect(window,&client);ui.frontend->screen_width=client.right;ui.frontend->screen_height=client.bottom;}ui.frontend->save_profile();ui.frontend->save_controls();Json final_report;try{std::ifstream f(diagnostic_report("runtime_report.json"));f>>final_report;}catch(...){final_report=Json::object();}final_report["build"]="OpenFO Reboot 0.9.0";final_report["process_memory"]=process_memory();final_report["clean_exit"]=true;final_report["successful_presentations"]=total_presented;final_report["renderer"]=render.diagnostics();final_report["simulation"]=simulation?simulation->diagnostics():Json();report(diagnostic_report("runtime_report.json"),final_report);renderer=nullptr;report(exe_dir/"openfo_settings.json",{{"effects",ui.volume},{"music",ui.music},{"automatic",ui.automatic}});if(timer){timeEndPeriod(1);timer=false;}
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 performance.finish();
#endif
 startup_finish(window);DestroyWindow(window);return 0;
 }catch(const std::exception&e){trace("exception",e.what());renderer=nullptr;if(timer)timeEndPeriod(1);if(!report_dir.empty())report(report_dir/"startup_error.json",{{"error",e.what()}});if(window)startup_finish(window);auto message=wide(e.what());MessageBoxW(window,message.c_str(),L"OpenFO startup/runtime error",MB_OK|MB_ICONERROR);if(window)DestroyWindow(window);return 1;}}
