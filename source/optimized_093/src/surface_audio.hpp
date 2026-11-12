#pragma once
#include "wheel_effects.hpp"
namespace ofo {
struct PcmWave {unsigned rate=44100;std::vector<float>samples;};
PcmWave decode_surface_wave(const Bytes&);
struct SurfaceSound {std::string roll,slide;float smooth=.2f,fade_start=3,fade_end=20,slide_start=5,slide_end=10,gain_roll=.75f,gain_slide=1;};
struct SurfaceAudioConfig {WheelEffectConfig surfaces;std::map<int,SurfaceSound>sounds;float nominal_pitch=20,pitch_scale=.25f,min_pitch=.5f,max_pitch=2,splash_speed=5,splash_gain=1,splash_delay=3;std::string splash;};
SurfaceAudioConfig load_surface_audio_config(const Resources&,WheelEffectConfig);
struct SurfaceAudioControl {std::map<int,std::array<float,3>>voices;bool water=false;float entry_speed=0;};
SurfaceAudioControl surface_audio_control(const SurfaceAudioConfig&,const std::array<WheelContact,4>&);
class SurfaceAudioMixer {struct Voice{PcmWave wave;double cursor=0;float gain=0,target=0,pitch=1,smooth=.2f,response=0;unsigned output_rate=0;};SurfaceAudioConfig config_;std::map<std::string,Voice>voices_;PcmWave splash_;double splash_cursor_=0,time_=0,last_splash_=-1e20;bool playing_=false,wet_=false,started_=false,active_=false;uint64_t generation_=0;size_t splash_count_=0;std::vector<std::string>errors_;public:
 SurfaceAudioMixer(const Resources&,SurfaceAudioConfig);
 void update(const std::array<WheelContact,4>&,double time,uint64_t generation,bool active);
 float frame(unsigned output_rate=44100);
 Json diagnostics()const;
};
}
