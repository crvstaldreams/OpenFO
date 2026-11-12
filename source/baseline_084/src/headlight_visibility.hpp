#pragma once
#include <algorithm>
#include <cmath>
namespace ofo {
struct HeadlightEmitterVisibility {float lens=0,shaft=0;};
inline HeadlightEmitterVisibility headlight_emitter_visibility(float facing,float /*health*/){
 // Damage is deliberately cosmetic: every physical lamp stays powered.
 float visible=std::clamp((facing-.08f)*2.f,0.f,1.f);
 // Air scattering remains visible looking along the beam from behind. The
 // lens alone is front-facing; vehicle/scene depth hides the shaft base.
 float angle=std::clamp(facing,-1.f,1.f);
 float shaft=(.65f+.35f*std::sqrt(std::max(0.f,1.f-angle*angle)));
 return{visible,shaft};
}
}
