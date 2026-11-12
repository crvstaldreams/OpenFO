#include "rain_effects.hpp"
#include "editor_scene_query.hpp"
#include "water.hpp"
#include <array>
#include <cmath>
#include <algorithm>
namespace ofo {
namespace {
constexpr double fixed_step=1./60;
constexpr float step=float(fixed_step),segment_time=.175f,radius=35;
constexpr int segments=8;
bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
uint32_t color(float alpha){return(uint32_t(std::clamp(alpha,0.f,1.f)*255)<<24)|0x00e8d8c8;}
void tri(std::vector<Vertex>&out,Vec a,Vec b,Vec c,float aa,float ab,float ac){for(auto p:{std::pair<Vec,float>{a,aa},{b,ab},{c,ac}}){Vertex v;v.position=p.first;v.normal={0,1,0};v.color=color(p.second);out.push_back(v);}}
void quad(std::vector<Vertex>&out,Vec a,Vec b,Vec c,Vec d,float alpha){tri(out,a,b,c,alpha,alpha,alpha);tri(out,a,c,d,alpha,alpha,alpha);}
}
struct RainEffects::Impl {
 struct Drop {std::array<Vec,segments+1> path{};float age=0,life=segments*segment_time;int last=segments;bool hit=false,water=false;};
 struct Impact {Vec position;float age=0;bool water=false;};
 std::unique_ptr<EditorSceneQuery> solid,water;
 std::vector<Drop> drops;std::vector<Impact> impacts;
 RainEffectStats stats;double time=0,pending=0;float credit=0,sky=20;bool started=false;uint64_t generation=0;uint32_t random=0x7272696e;
 Impl(){drops.reserve(max_drops);impacts.reserve(max_impacts);}
 void reset(){drops.clear();impacts.clear();stats={};credit=0;pending=0;started=false;random=0x7272696e;}
 float rand(){random^=random<<13;random^=random>>17;random^=random<<5;return float(random>>8)*(1.f/16777216);}
 std::optional<Vec> ray(const EditorSceneQuery*q,Vec o,Vec d,float distance){if(!q||q->triangle_count()==0)return {}; ++stats.rays;return q->ray(o,d,distance);}
 std::optional<Vec> first(Vec a,Vec b,bool&wet){Vec diff=b-a;float distance=length(diff);Vec dir=unit(diff);auto s=ray(solid.get(),a-dir*.12f,dir,distance+.12f),w=ray(water.get(),a-dir*.12f,dir,distance+.12f);wet=w&&(!s||length(*w-a)<length(*s-a));return wet?w:s;}
 Vec position(const Drop&d,float age)const {float f=std::clamp(age,0.f,d.life)/segment_time;int i=std::min(d.last-1,int(f));float start=i*segment_time,end=std::min(d.life,(i+1)*segment_time);float t=end>start?std::clamp((age-start)/(end-start),0.f,1.f):1;return d.path[i]+(d.path[i+1]-d.path[i])*t;}
 bool spawn(Vec eye){
  // Reserve the complete query budget before constructing a trajectory; live
  // particles never need rays, and their bounded paths are their collision cache.
  if(stats.rays+2+segments*2>stats.ray_budget)return false;
  Drop d;float a=rand()*6.2831853f,r=std::sqrt(rand())*31;
  d.path[0]=eye+Vec{std::cos(a)*r,18,std::sin(a)*r};
  const float skyY=std::max(sky,d.path[0].y+2);
  bool wet=false;auto roof=first({d.path[0].x,skyY,d.path[0].z},d.path[0],wet);
  // A sky-occluded spawn is discarded: there is never indoor emission below a roof.
  if(roof)return true;
  Vec velocity={2.2f+rand()*.9f,-23-rand()*5,.8f};
  for(int i=1;i<=segments;++i){
   Vec end=d.path[i-1]+velocity*segment_time+Vec{0,-4.9f*segment_time*segment_time,0};velocity.y-=9.8f*segment_time;
   auto hit=first(d.path[i-1],end,wet);d.path[i]=hit?*hit:end;
   if(hit){d.hit=true;d.water=wet;d.last=i;const float distance=length(end-d.path[i-1]);float fraction=std::clamp(length(*hit-d.path[i-1])/distance,0.f,1.f);d.life=(i-1+fraction)*segment_time;break;}
  }
  if(d.life>.001f)drops.push_back(d);
  return true;
 }
 void tick(Vec eye,float strength,int detail){
  for(auto&i:impacts)i.age+=step;
  impacts.erase(std::remove_if(impacts.begin(),impacts.end(),[&](const Impact&i){return i.age>=(i.water?.85f:.22f)||length(i.position-eye)>radius;}),impacts.end());
  for(auto&d:drops){d.age+=step;if(d.hit&&d.age>=d.life&&length(d.path[d.last]-eye)<=radius){if(d.water)++stats.water_impacts;else ++stats.ground_impacts;if(impacts.size()<max_impacts)impacts.push_back({d.path[d.last],0,d.water});}}
  drops.erase(std::remove_if(drops.begin(),drops.end(),[&](const Drop&d){return d.age>=d.life||length(position(d,d.age)-eye)>radius;}),drops.end());
  const size_t limits[]={256,512,1024,max_drops};const float rate[]={190,380,760,1140};
  if(drops.size()>limits[detail])drops.resize(limits[detail]);
  credit=std::min(credit+rate[detail]*strength*step,32.f);
  while(credit>=1&&drops.size()<limits[detail]){if(!spawn(eye))break;credit-=1;}
 }
};
RainEffects::RainEffects():impl_(std::make_unique<Impl>()){}RainEffects::~RainEffects()=default;
RainEffects::RainEffects(RainEffects&&)noexcept=default;RainEffects&RainEffects::operator=(RainEffects&&)noexcept=default;
void RainEffects::configure(const Geometry&g){
 if(!impl_)impl_=std::make_unique<Impl>();
 impl_->reset();impl_->solid=std::make_unique<EditorSceneQuery>(g);
 // Editor picking deliberately excludes water. Reuse exactly the same BVH and
 // intersection implementation for a second, water-only collision snapshot.
 Geometry water;Material material;material.name="rain_collision";water.materials.push_back(material);float top=-1000000;
 for(const auto&m:g.meshes){if(m.prop_index>=0||m.captured||m.billboard||m.material>=g.materials.size())continue;
  if(m.solid||water_material(g.materials[m.material]))for(const auto&v:m.vertices){Vec p=point(m.transform,v.position);if(finite(p))top=std::max(top,p.y);}
  if(water_material(g.materials[m.material])){auto copy=m;copy.material=0;copy.solid=true;water.meshes.push_back(std::move(copy));}
 }
 impl_->sky=std::isfinite(top)?std::min(top+2,1000000.f):20;impl_->water=std::make_unique<EditorSceneQuery>(water);
}
void RainEffects::clear(){if(impl_)impl_->reset();}
void RainEffects::update(Vec eye,double time,bool enabled,float strength,int detail,uint64_t generation){
 if(!impl_)impl_=std::make_unique<Impl>();
 auto&p=*impl_;detail=std::clamp(detail,0,3);const size_t budget[]={96,160,288,416};p.stats.rays=0;p.stats.ray_budget=budget[detail];
 if(!enabled||!finite(eye)||!std::isfinite(time)||!std::isfinite(strength)||strength<=0){p.reset();return;}
 if(!p.started||p.generation!=generation||time<p.time||time-p.time>.5){p.reset();p.time=time;p.generation=generation;p.started=true;p.stats.ray_budget=budget[detail];return;}
 p.pending+=time-p.time;p.time=time;
 // Fixed simulation steps give stable trajectories and emission at normal frame
 // rates. Catch-up is bounded; recovery/large time jumps discard stale weather.
 int ticks=0;while(p.pending+1e-10>=fixed_step&&ticks++<30){p.pending-=fixed_step;p.tick(eye,std::clamp(strength,0.f,1.f),detail);}
}
void RainEffects::vertices(Vec right,Vec up,RainEffectVertices&out)const {
 out.streaks.clear();out.splashes.clear();out.ripples.clear();if(!impl_)return;
 right=finite(right)?unit(right,{1,0,0}):Vec{1,0,0};up=finite(up)?unit(up):Vec{0,1,0};
 out.streaks.reserve(impl_->drops.size()*12);out.splashes.reserve(impl_->impacts.size()*18);out.ripples.reserve(impl_->impacts.size()*96);
 for(const auto&d:impl_->drops){Vec tip=impl_->position(d,d.age),tail=impl_->position(d,std::max(0.f,d.age-.024f));Vec side=unit(cross(tip-tail,cross(right,up)),right)*.012f,mid=(tip+tail)*.5f;
  // A soft cross section is encoded in vertex alpha; no particle texture needed.
  tri(out.streaks,tail-side,tip-side,mid,0,0,.48f);tri(out.streaks,tip-side,tip+side,mid,0,0,.48f);tri(out.streaks,tip+side,tail+side,mid,0,0,.48f);tri(out.streaks,tail+side,tail-side,mid,0,0,.48f);
 }
 for(const auto&i:impl_->impacts){float life=i.water?.85f:.22f,t=i.age/life,alpha=(1-t)*(i.water?.38f:.45f);Vec center=i.position+Vec{0,.025f,0};
  if(i.water){float r=.045f+i.age*.65f,w=.013f*(1-t)+.003f;constexpr int n=16;for(int k=0;k<n;++k){float a=k*6.2831853f/n,b=(k+1)*6.2831853f/n;Vec va{std::cos(a),0,std::sin(a)},vb{std::cos(b),0,std::sin(b)};quad(out.ripples,center+va*(r-w),center+va*(r+w),center+vb*(r+w),center+vb*(r-w),alpha);}}
  // Three brief upward droplets on both road and water, always above the hit.
  if(i.age<.22f)for(int k=0;k<3;++k){float a=k*2.0943951f;Vec pos=center+Vec{std::cos(a)*i.age*.8f,std::max(0.f,i.age*1.4f-4.9f*i.age*i.age),std::sin(a)*i.age*.8f};float s=.022f;quad(out.splashes,pos-right*s,pos+right*s,pos+right*s+Vec{0,.055f,0},pos-right*s+Vec{0,.055f,0},(.22f-i.age)*2);}
 }
}
size_t RainEffects::drop_count()const{return impl_?impl_->drops.size():0;}
size_t RainEffects::splash_count()const {if(!impl_)return 0;return std::count_if(impl_->impacts.begin(),impl_->impacts.end(),[](const Impl::Impact&i){return i.age<.22f;});}
size_t RainEffects::ripple_count()const {if(!impl_)return 0;return std::count_if(impl_->impacts.begin(),impl_->impacts.end(),[](const Impl::Impact&i){return i.water;});}
RainEffectStats RainEffects::stats()const{return impl_?impl_->stats:RainEffectStats{};}
}
