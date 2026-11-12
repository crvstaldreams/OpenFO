#include "collision_audio.hpp"
#include <stdexcept>
namespace ofo {
int collision_category_for_material(const Material&m){auto n=lower(m.name);auto has=[&](const char*s){return n.find(s)!=std::string::npos;};if(has("water")||has("nocollision"))return 0;if(has("wood")||has("timber")||has("log"))return 7;if(has("metal")||has("steel")||has("iron"))return 10;if(has("plastic"))return 11;if(has("tyre")||has("tire")||has("rubber"))return 6;if(has("barrel"))return 5;if(has("wall")||has("concrete")||has("stone")||has("rock"))return 4;return 2;}
CollisionAudioConfig load_collision_audio_config(const Resources&r){
 auto j=bed_file(r,"sound/collision_sounds.bed");CollisionAudioConfig c;
 for(auto it=j.at("CollisionSoundTypes").begin();it!=j.at("CollisionSoundTypes").end();++it){
  int id=std::stoi(it.key());auto name=it.value().get<std::string>();Json group;std::string source;
  for(const char*key:{"HitToTruck","HitToGround"})if(j.contains(key)&&j[key].contains(name)){group=j[key][name];source=key;break;}
  if(!group.is_object())continue;
  CollisionSoundBank bank;bank.name=name;bank.source=source+"."+name;bank.delay=std::clamp(group.value("Delay",200.f)/1000.f,0.f,10.f);
  for(auto s=group.begin();s!=group.end();++s)if(s.value().is_object()&&s.value().contains("Wave")){const auto&v=s.value();CollisionSample a;a.wave=clean_path("sound/"+v.at("Wave").get<std::string>());a.trigger_speed=std::max(0.f,v.value("TriggerSpeed",0.f));a.gain=std::clamp(v.value("Gain",1.f),0.f,1.f);a.pitch_random=std::clamp(v.value("PitchRandom",.15f),0.f,.5f);a.gain_random=std::clamp(v.value("GainRandom",.10f),0.f,.5f);a.min_distance=std::max(.1f,v.value("MinDistance",1.f));bank.samples.push_back(std::move(a));}
  std::stable_sort(bank.samples.begin(),bank.samples.end(),[](const auto&a,const auto&b){return a.trigger_speed<b.trigger_speed;});if(!bank.samples.empty())c.banks.emplace(id,std::move(bank));
 }return c;
}
CollisionAudioMixer::CollisionAudioMixer(const Resources&r,CollisionAudioConfig c):config_(std::move(c)){
 size_t budget=0;voices_.reserve(max_voices);for(const auto&b:config_.banks)for(const auto&s:b.second.samples)if(!waves_.count(s.wave))try{auto w=decode_surface_wave(r.read(s.wave));if(w.samples.size()*sizeof(float)>64u*1024*1024-budget)throw std::runtime_error("Collision PCM memory budget exceeded");budget+=w.samples.size()*sizeof(float);waves_.emplace(s.wave,std::move(w));}catch(const std::exception&e){errors_.push_back(s.wave+": "+e.what());}
}
float CollisionAudioMixer::random_signed(){random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;return float(random_&0xffff)/32767.5f-1;}
void CollisionAudioMixer::update(const std::vector<CollisionEvent>&events,double time,uint64_t generation,bool active,Vec listener){
 bool reset=started_&&(!std::isfinite(time)||time<time_||generation!=generation_||time-time_>.5);if(reset){voices_.clear();seen_.clear();last_event_=0;last_play_.clear();}if(!active)voices_.clear();
 uint64_t newest=last_event_;for(const auto&e:events){if(!e.id||e.id<=last_event_||seen_.count(e.id))continue;newest=std::max(newest,e.id);seen_.insert(e.id);while(seen_.size()>max_seen_events)seen_.erase(seen_.begin());
  if(!active||reset||!std::isfinite(time)||!std::isfinite(e.closing_speed)||!std::isfinite(e.impulse)||!std::isfinite(e.point.x)||!std::isfinite(e.point.y)||!std::isfinite(e.point.z)){++suppressed_;continue;}
  auto b=config_.banks.find(e.sound_category);if(b==config_.banks.end()){++suppressed_;continue;}auto prev=last_play_.find(e.sound_category);if(prev!=last_play_.end()&&time-prev->second<b->second.delay){++suppressed_;continue;}
  const CollisionSample*chosen=nullptr;for(const auto&s:b->second.samples)if(e.closing_speed>=s.trigger_speed)chosen=&s;if(!chosen){++suppressed_;continue;}auto wave=waves_.find(chosen->wave);if(wave==waves_.end()||wave->second.samples.empty()){++suppressed_;continue;}
  float dx=e.point.x-listener.x,dy=e.point.y-listener.y,dz=e.point.z-listener.z;float distance=std::sqrt(dx*dx+dy*dy+dz*dz);if(!std::isfinite(distance)){++suppressed_;continue;}
  float energy=std::clamp(.35f+std::max(0.f,e.impulse)*.00008f,.35f,1.f);float gain=std::clamp(chosen->gain*std::max(1.f,e.closing_speed/std::max(.1f,chosen->trigger_speed)),0.f,1.f)*energy*(1+random_signed()*chosen->gain_random)*std::min(1.f,chosen->min_distance/std::max(.1f,distance));
  Voice v;v.wave=&wave->second;v.gain=std::clamp(gain,0.f,1.f);v.pitch=1+random_signed()*chosen->pitch_random;if(voices_.size()==max_voices)voices_.erase(voices_.begin());voices_.push_back(v);last_play_[e.sound_category]=time;++played_;last_sample_=chosen->wave;last_gain_=v.gain;last_pitch_=v.pitch;
 }last_event_=newest;time_=std::isfinite(time)?time:0;generation_=generation;started_=true;active_=active;
}
float CollisionAudioMixer::frame(unsigned rate){if(rate<8000||rate>192000)throw std::runtime_error("Invalid collision output sample rate");if(!active_)return 0;float value=0;for(auto&v:voices_){size_t a=size_t(v.cursor);if(a>=v.wave->samples.size())continue;float t=float(v.cursor-a),next=a+1<v.wave->samples.size()?v.wave->samples[a+1]:0;float edge=std::min(1.f,float(v.cursor/32.))*std::min(1.f,float((v.wave->samples.size()-v.cursor)/64.));value+=(v.wave->samples[a]+(next-v.wave->samples[a])*t)*v.gain*edge;v.cursor+=double(v.wave->rate)/rate*v.pitch;}voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice&v){return v.cursor>=v.wave->samples.size();}),voices_.end());return std::clamp(value,-4.f,4.f);}
Json CollisionAudioMixer::diagnostics()const{return{{"banks",config_.banks.size()},{"samples_loaded",waves_.size()},{"active_voices",voices_.size()},{"voice_limit",max_voices},{"played",played_},{"suppressed",suppressed_},{"last_sample",last_sample_},{"last_gain",last_gain_},{"last_pitch",last_pitch_},{"errors",errors_},{"sources","native collision_sounds.bed CollisionSoundTypes/HitToTruck/HitToGround + PCM WAV"},{"policy","bounded one-shots; physics event IDs; native speed thresholds/category delay; impulse gain, seeded pitch/gain variation and distance attenuation"}};}
}
