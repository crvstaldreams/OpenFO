#include "screen_effects_policy.hpp"
#include "screen_effects_shaders.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cstring>
using namespace ofo;
namespace ref=ofo::screen_effects_reference;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 const int divisors[]={4,4,2,2},samples[]={0,4,6,8},steps[]={0,8,12,16};
 for(int l=0;l<4;++l){auto b=screen_effects_budget(l);need(b.enabled==(l>0),"budget enabled");need(b.ao_divisor==divisors[l]&&b.ao_samples==samples[l]&&b.ssr_steps==steps[l],"quality cost caps");need(std::isfinite(b.ao_radius)&&b.ao_radius>0&&b.ao_strength>0&&b.ssr_distance>0&&b.ssr_strength>0,"finite positive tuning");}
 need(!screen_effects_budget(-8).enabled&&screen_effects_budget(999).ssr_steps==16,"quality clamped");
 need(screen_effects_size(1921,1081,4)==std::make_pair(481,271),"ceil reduced targets");
 need(screen_effects_size(0,-10,0)==std::make_pair(1,1),"invalid dimensions sanitized");
 need(screen_effects_size(std::numeric_limits<int>::max(),1,2).first==1073741824,"ceil no integer overflow");
 ref::Projection p(perspective(1.1f,16.f/9,.1f,2000));
 for(float z:{.1f,.3f,1.f,10.f,100.f,900.f})for(ref::UV uv: {ref::UV{.1f,.2f},ref::UV{.5f,.5f},ref::UV{.9f,.8f}}){
  Vec v=ref::reconstruct(uv,p.a+p.b/z,p);ref::UV result=ref::project(v,p);
  need(std::abs(v.z-z)<std::max(.0001f,z*.001f),"D3D depth z reconstruction");
  need(std::abs(result.x-uv.x)<1e-5f&&std::abs(result.y-uv.y)<1e-5f,"UV round trip +Z LH and flipped screen y");
 }
 Vec center{0,0,10},normal{0,0,-1};float flat=0,corner=0;
 for(int j=0;j<8;++j){float a=(j+.5f)*6.2831853f/8;Vec xy{std::cos(a)*.5f,std::sin(a)*.5f,0};flat+=ref::ao_neighbor(center,normal,center+xy,1.4f);corner+=ref::ao_neighbor(center,normal,center+xy+Vec{0,0,-.5f},1.4f);}
 need(std::abs(ref::ao_factor(flat,8,.32f)-1)<1e-6f,"flat plane receives no self AO");
 need(ref::ao_factor(corner,8,.32f)<.95f&&ref::ao_factor(corner,8,.32f)>=.78f,"near corner darkens conservatively");
 need(ref::ao_factor(100,8,.32f)==.78f,"AO bounded against black corners");
 need(ref::ao_neighbor(center,normal,center,1.4f)==0&&ref::ao_neighbor(center,normal,{0,0,1},1.4f)==0,"AO coincident and remote samples rejected");
 Vec reflected=ref::reflect(unit({0,-1,1}),{0,1,0});need(reflected.y>0&&reflected.z>0&&std::abs(length(reflected)-1)<1e-6f,"LH water reflection points above plane");
 // A ray that reflects from horizontal water reaches an opaque wall in view.
 auto wall=[&](ref::UV){return ref::depth({0,0,14},p);};
 auto hit=ref::trace({0,0,5},{0,1,0},unit({0,.1f,1}),p,16,20,wall);
 need(hit.found&&hit.distance>8&&hit.distance<10&&ref::inside(hit.uv),"SSR wall crossing and three-step refinement");
 auto near_wall=[&](ref::UV){return ref::depth({0,0,7.2f},p);};
 auto near_hit=ref::trace({0,0,5},{0,1,0},{0,0,1},p,8,50,near_wall);
 need(near_hit.found&&near_hit.distance<50.f/8,"SSR near wall within first MEDIUM step");
 auto sky=[](ref::UV){return 1.f;};need(!ref::trace({0,0,5},{0,1,0},{0,0,1},p,16,20,sky).found,"SSR sky miss");
 need(!ref::trace({0,0,5},{0,1,0},{0,0,-1},p,16,20,wall).found,"SSR ray toward camera misses");
 need(!ref::trace({0,0,5},{0,1,0},unit({10,0,1}),p,16,20,wall).found,"SSR offscreen terminates");
 need(!ref::trace({0,0,-1},{0,1,0},{0,0,1},p,8,50,near_wall).found,"SSR invalid biased origin behind near domain");
 need(!ref::trace({100,0,5},{0,1,0},{0,0,1},p,8,50,near_wall).found,"SSR biased origin offscreen rejected");
 auto own=[&](ref::UV){return ref::depth({0,0,5},p);};need(!ref::trace({0,0,5},{0,1,0},{0,0,1},p,16,20,own).found,"SSR rejects self hit without front-to-back crossing");
 auto far=[&](ref::UV){return ref::depth({0,0,100},p);};need(!ref::trace({0,0,5},{0,1,0},{0,0,1},p,16,20,far).found,"SSR finite distance miss");
 const char* hlsl=screen_effects_hlsl();for(const char* entry:{"ao_ps(","composite_ps(","water_vs(","water_ps("})need(std::strstr(hlsl,entry)!=nullptr,"shader entrypoint contract");
 std::cout<<"screen effects: budgets, target sizes, LH depth/UV roundtrips, bounded AO and SSR hit/miss/self/offscreen passed (CPU reference; GPU untested)\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
