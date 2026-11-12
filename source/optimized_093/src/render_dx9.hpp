#pragma once
#include "simulation.hpp"
#include "photo_mode.hpp"
#include "track_environment.hpp"
#include "race_loading.hpp"
#include "stunt_event.hpp"
#include <memory>
#include <windows.h>
namespace ofo {
class Frontend;struct NativeDriver;
struct PhotoUI {bool active=false,panel=true,capture_requested=false;PhotoCamera camera;PhotoSettings settings;std::string status;};
struct UI {RaceMode mode=RaceMode::Circuit;StuntDisplay stunt;bool event_intro=false,lan_talking=false;std::string event_hint;TrackEnvironment environment;RaceLoadingPresentation loading;PhotoUI photo;std::shared_ptr<Frontend>frontend;bool guidance_available=false;float guidance_yaw=0;double countdown=0,recovery_countdown=0,off_route_distance=-1;int replay_selected=2;Json replay_state,music_state,race_state;bool race_finished=false,race_results=false;double finish_age=0;enum Page{Title,Select,Race,Pause,Confirm,Settings,Loading,Replay};Page page=Title;int row=0,car=3,skin=1;std::string track,label,message,car_name="PEPPER";bool automatic=true;float music=.45f,volume=.55f;double clock=0;};
struct OpponentVisual {RaceOpponentConfig config;Geometry body,tire;};
struct DiagnosticPixels;
struct RendererFrameMetrics;
class Renderer {struct Impl;std::unique_ptr<Impl>p_;public:Renderer(HWND,const Resources&,bool present_probe=false,bool optimize=true);~Renderer();void scene(const Geometry&,const Geometry&,const Geometry&,const VehicleConfig&,int skin,const std::string&stage,std::shared_ptr<const NativeDriver>driver_model={});void opponents(const std::vector<OpponentVisual>&);void draw(const State&,const UI&,int camera);bool present();void resize(int,int);void set_vsync(bool enabled);PhotoCamera photo_camera()const;fs::path save_photo(const fs::path&);Json diagnostics()const;
#ifdef OPENFO_PERFORMANCE_DIAGNOSTICS
 RendererFrameMetrics performance_metrics()const noexcept;
 DiagnosticPixels capture_performance_frame();
 Json performance_adapter()const;
#endif
};
}
