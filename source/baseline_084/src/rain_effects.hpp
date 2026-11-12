#pragma once
#include "assets.hpp"
#include <memory>
namespace ofo {
struct RainEffectVertices {std::vector<Vertex> streaks,splashes,ripples;};
struct RainEffectStats {
 size_t rays=0,ray_budget=0,ground_impacts=0,water_impacts=0;
};
// Static world geometry is snapshotted by configure(). Call it after track edits.
// update() accepts simulation time, not wall time; equal times freeze emission.
// Vertices use straight alpha, white texture, depth test ON and depth writes OFF.
// Cars/moving props occlude rain through depth; only static geometry makes impacts.
class RainEffects {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 static constexpr size_t max_drops=1536,max_impacts=256;
 RainEffects();~RainEffects();
 RainEffects(RainEffects&&) noexcept;RainEffects&operator=(RainEffects&&) noexcept;
 RainEffects(const RainEffects&)=delete;RainEffects&operator=(const RainEffects&)=delete;
 void configure(const Geometry&);
 void clear();
 void update(Vec eye,double time,bool enabled,float strength,int detail,uint64_t generation);
 void vertices(Vec right,Vec up,RainEffectVertices&)const;
 size_t drop_count()const;size_t splash_count()const;size_t ripple_count()const;
 RainEffectStats stats()const;
};
}
