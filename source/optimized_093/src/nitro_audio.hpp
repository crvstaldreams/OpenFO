#pragma once
#include "surface_audio.hpp"
namespace ofo {
struct NitroAudioConfig {std::string shot,loop;float shot_gain=1,loop_gain=1;double delay=2.5;};
inline NitroAudioConfig load_nitro_audio_config(const Resources&r){auto j=bed_file(r,"sound/equip_sounds.bed").at("Nitro");NitroAudioConfig c;c.shot=clean_path("sound/"+j.at("ShotSample").get<std::string>());c.loop=clean_path("sound/"+j.at("LoopSample").get<std::string>());c.shot_gain=j.value("ShotGain",1.f);c.loop_gain=j.value("LoopGain",1.f);c.delay=std::max(0.,j.value("Delay",2500.)/1000.);return c;}
// Native onset is a one-shot with the script's retrigger delay. The sustain
// voice loops for the duration of actual simulation activation, without restarts.
class NitroAudioMixer {
 NitroAudioConfig config_;PcmWave shot_,loop_;double shot_cursor_=0,loop_cursor_=0,last_shot_=-1e20,time_=0;bool active_=false,shot_active_=false,initialized_=false;uint64_t generation_=0,shots_=0;
 static float sample(const PcmWave&w,double&cursor,bool loop,unsigned rate){if(w.samples.empty()||!rate)return 0;if(!loop&&cursor>=w.samples.size())return 0;size_t a=size_t(cursor)%w.samples.size(),b=loop?(a+1)%w.samples.size():std::min(a+1,w.samples.size()-1);float v=w.samples[a]+(w.samples[b]-w.samples[a])*float(cursor-std::floor(cursor));cursor+=double(w.rate)/rate;if(loop)cursor=std::fmod(cursor,double(w.samples.size()));return v;}
public:
 NitroAudioMixer(NitroAudioConfig c,PcmWave shot,PcmWave loop):config_(std::move(c)),shot_(std::move(shot)),loop_(std::move(loop)){}
 NitroAudioMixer(const Resources&r,NitroAudioConfig c):NitroAudioMixer(c,decode_surface_wave(r.read(c.shot)),decode_surface_wave(r.read(c.loop))){}
 void update(bool nitro_active,bool enabled,double time,uint64_t generation){if(!initialized_||generation!=generation_||time+1e-6<time_){active_=shot_active_=false;shot_cursor_=loop_cursor_=0;last_shot_=-1e20;initialized_=true;generation_=generation;}time_=time;bool on=nitro_active&&enabled;if(!on){active_=shot_active_=false;shot_cursor_=loop_cursor_=0;return;}if(!active_){loop_cursor_=0;if(time-last_shot_+1e-9>=config_.delay){shot_cursor_=0;shot_active_=true;last_shot_=time;++shots_;}}active_=true;}
 float frame(unsigned rate=44100){if(!active_)return 0;float value=sample(loop_,loop_cursor_,true,rate)*config_.loop_gain;if(shot_active_){value+=sample(shot_,shot_cursor_,false,rate)*config_.shot_gain;if(shot_cursor_>=shot_.samples.size())shot_active_=false;}return value;}
 Json diagnostics()const{return{{"shot",config_.shot},{"loop",config_.loop},{"shot_gain",config_.shot_gain},{"loop_gain",config_.loop_gain},{"delay_seconds",config_.delay},{"active",active_},{"shots",shots_},{"volume_channel","EFFECTS"}};}
};
}
