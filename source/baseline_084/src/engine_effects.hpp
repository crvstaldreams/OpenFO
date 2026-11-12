#pragma once
#include "assets.hpp"
namespace ofo {
struct EffectParameters {
 bool loaded=false;float fire_start=.7f,fire_radius=.2f,fire_alpha=1,fire_grow=-.99f,fire_life_power=2;
 int fire_iterations=2;Vec fire_offset{0,.5f,1.5f};
 std::array<float,2>fire_size{1.2f,1.8f},fire_velocity{.8f,2.8f},fire_life{.35f,.65f},fire_rotation{-1024,1024};
};
EffectParameters load_effect_parameters(const Resources&);
int engine_damage_stage(float,const EffectParameters&);
struct EngineEffectVertices {std::vector<Vertex>smoke,fire;};
EngineEffectVertices engine_effect_vertices(const EffectParameters&,float damage,Vec origin,Vec velocity,Vec right,Vec up,double clock,bool menu=false);
// Reuses output capacity; detail_stride selects a stable subset of cosmetic particles.
void engine_effect_vertices(const EffectParameters&,float damage,Vec origin,Vec velocity,Vec right,Vec up,double clock,EngineEffectVertices& out,bool menu=false,int detail_stride=1);
// OpenGL prototype's basic impact gain, not reconstructed native damage equations.
class ImpactDamage {float amount_=0,cooldown_=0;public:
 void tick(float dt){if(std::isfinite(dt)&&dt>0)cooldown_=std::max(0.f,cooldown_-dt);}
 void hit(float closing,float impulse,float mass,float fraction=1.f){if(!std::isfinite(closing)||!std::isfinite(impulse)||!std::isfinite(mass)||mass<=0||closing<2||impulse<1170||cooldown_>0)return;amount_=std::clamp(amount_+.025f*(impulse/mass)*fraction,0.f,1.f);cooldown_=.12f;}
 float value()const{return amount_;}void reset(){amount_=cooldown_=0;}
};
}
