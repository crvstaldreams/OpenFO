#pragma once
#include "assets.hpp"
#include <deque>
namespace ofo {
// Speeds are m/s: slip is longitudinal tread speed minus ground speed.
struct WheelContact {bool grounded=false;Vec point{},normal{0,1,0},velocity{};float water_depth=0,load=0,slip=0,lateral_speed=0,speed=0,width=.185f;size_t material=size_t(-1);int surface_id=-1;};
struct WheelSurface {int skid_type=0,dirt_type=0;bool water=false;std::string label;int sound_id=0;};
struct WheelDirtParameters {std::array<float,2> life{1.5f,2},size{.1f,.23f},grow{.2f,.25f},up{1.75f,3.25f};std::array<float,3> longitudinal{2,5,1},lateral{6,20,1},linear{10,60,0};float probability=1;};
struct WheelEffectConfig {
 bool loaded=false;std::string particle_texture,skid_texture;
 std::array<float,2> water_speed{5,20},water_life{.3f,.6f},water_size{.08f,.1f},water_grow{1.8f,2.5f},water_up{1.5f,8.5f};
 std::array<WheelDirtParameters,5> dirt{};std::vector<WheelSurface> materials;std::map<int,WheelSurface> surfaces;
};
WheelEffectConfig load_wheel_effect_config(const Resources&,const Geometry&,const std::string&stage);
struct WheelEffectVertices {std::vector<Vertex> skids,particles;};
class WheelEffects {
 struct Skid {Vec a,b,n;float width,alpha;double born;int type;};
 struct Particle {Vec position,velocity;float age=0,life=1,size=.1f,grow=.2f;int x=0,y=0;uint64_t serial=0;};
 WheelEffectConfig config_;std::deque<Skid> skids_;std::deque<Particle> particles_;std::array<WheelContact,4> previous_{};std::array<bool,4> connected_{};std::array<float,4> credit_{};double time_=0;bool started_=false;uint64_t generation_=0,serial_=0;
public:
 static constexpr size_t max_skids=2048,max_particles=512;
 explicit WheelEffects(WheelEffectConfig config={}):config_(std::move(config)){}
 void configure(WheelEffectConfig config){config_=std::move(config);clear();}
 void clear();void reset(){clear();}
 void update(const std::array<WheelContact,4>&,double time,uint64_t recovery_generation=0);
 WheelEffectVertices vertices(Vec view_right,Vec view_up)const;
 // Reduces cosmetic particles and retains a contiguous tail of skid segments.
 void vertices(Vec view_right,Vec view_up,WheelEffectVertices& out,int detail_stride=1)const;
 size_t skid_count()const{return skids_.size();}size_t particle_count()const{return particles_.size();}
 const WheelEffectConfig&config()const{return config_;}
};
}
