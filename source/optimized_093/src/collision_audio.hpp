#pragma once
#include "surface_audio.hpp"
#include <set>
namespace ofo {
// IDs increase once per physics event, within a recovery generation. Category IDs
// are native CollisionSoundTypes (0=None, 1=Truck ... 11=Plastic).
struct CollisionEvent {uint64_t id=0;Vec point{};float impulse=0,closing_speed=0;int sound_category=0;};
struct CollisionSample {std::string wave;float trigger_speed=0,gain=1,pitch_random=.15f,gain_random=.10f,min_distance=1;};
struct CollisionSoundBank {std::string name,source;float delay=.2f;std::vector<CollisionSample>samples;};
struct CollisionAudioConfig {std::map<int,CollisionSoundBank>banks;};
CollisionAudioConfig load_collision_audio_config(const Resources&);
// Provisional fallback for static render materials without authored surface IDs.
int collision_category_for_material(const Material&);
class CollisionAudioMixer {
 struct Voice {const PcmWave*wave=nullptr;double cursor=0;float gain=0,pitch=1;};
 CollisionAudioConfig config_;std::map<std::string,PcmWave>waves_;std::vector<Voice>voices_;std::map<int,double>last_play_;std::set<uint64_t>seen_;std::vector<std::string>errors_;double time_=0;uint64_t generation_=0,last_event_=0;bool started_=false,active_=false;size_t played_=0,suppressed_=0;std::string last_sample_;float last_gain_=0,last_pitch_=1;uint32_t random_=0x973d5a1u;
 float random_signed();
public:
 static constexpr size_t max_voices=24,max_seen_events=2048;
 CollisionAudioMixer(const Resources&,CollisionAudioConfig);
 void update(const std::vector<CollisionEvent>&,double time,uint64_t generation,bool active,Vec listener={});
 float frame(unsigned output_rate=44100);
 Json diagnostics()const;
};
}
