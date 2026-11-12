#include "engine_effects.hpp"
#include "wheel_effects.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
namespace {
void need(bool value,const char*message){if(!value)throw std::runtime_error(message);}
bool same(Vec a,Vec b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same(const Vertex&a,const Vertex&b){return same(a.position,b.position)&&same(a.normal,b.normal)&&a.color==b.color&&a.u==b.u&&a.v==b.v;}
void exact(const std::vector<Vertex>&a,const std::vector<Vertex>&b){need(a.size()==b.size(),"matching output size");for(size_t i=0;i<a.size();++i)need(same(a[i],b[i]),"matching output vertices");}
void finite(const std::vector<Vertex>&a){for(const auto&v:a)need(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z)&&std::isfinite(v.u)&&std::isfinite(v.v)&&v.u>=0&&v.u<=1&&v.v>=0&&v.v<=1,"finite native atlas vertices");}
void subset(const std::vector<Vertex>&full,const std::vector<Vertex>&reduced,int stride){need(reduced.size()==((full.size()/6+stride-1)/stride)*6,"reduced particle count");for(size_t i=0;i<reduced.size();++i)need(same(reduced[i],full[(i/6)*stride*6+i%6]),"stable particle subset preserves lifetime and atlas");}
}
int main(){try{
 EffectParameters cfg;EngineEffectVertices engine;
 auto emit=[&](EngineEffectVertices&out,int stride=1){engine_effect_vertices(cfg,1,{1,2,3},{3,0,4},{1,0,0},{0,1,0},2.125,out,false,stride);};
 emit(engine);auto old=engine_effect_vertices(cfg,1,{1,2,3},{3,0,4},{1,0,0},{0,1,0},2.125);
 exact(engine.smoke,old.smoke);exact(engine.fire,old.fire);need(engine.smoke.size()==60&&engine.fire.size()==288,"full engine detail uses reduced smoke and retains fire count");
 auto*smoke=engine.smoke.data();auto*fire=engine.fire.data();auto smoke_capacity=engine.smoke.capacity(),fire_capacity=engine.fire.capacity();
 for(int stride:{4,2,1,4,1}){emit(engine,stride);subset(old.smoke,engine.smoke,stride);subset(old.fire,engine.fire,stride);finite(engine.smoke);finite(engine.fire);need(engine.smoke.data()==smoke&&engine.fire.data()==fire&&engine.smoke.capacity()==smoke_capacity&&engine.fire.capacity()==fire_capacity,"engine capacity reused across presets");}
 engine_effect_vertices(cfg,0,{},{},{1,0,0},{0,1,0},3,engine);need(engine.smoke.empty()&&engine.fire.empty(),"healthy frame clears reused output");
 emit(engine);engine_effect_vertices(cfg,1,{},{},{1,0,0},{0,1,0},NAN,engine);need(engine.smoke.empty()&&engine.fire.empty(),"invalid clock clears reused output");
 WheelEffectConfig wc;WheelSurface surface;surface.skid_type=3;surface.dirt_type=1;wc.materials.push_back(surface);WheelEffects wheels(wc);
 std::array<WheelContact,4> contacts{};for(auto&c:contacts){c.grounded=true;c.load=1000;c.material=0;c.slip=8;c.speed=30;c.velocity={0,0,30};}
 wheels.update(contacts,0);for(int i=1;i<60;++i){for(auto&c:contacts)c.point.z+=.2f;wheels.update(contacts,i*.02);}
 WheelEffectVertices result;wheels.vertices({1,0,0},{0,1,0},result);auto full=wheels.vertices({1,0,0},{0,1,0});exact(result.skids,full.skids);exact(result.particles,full.particles);
 auto particle_count=wheels.particle_count(),skid_count=wheels.skid_count();auto*skids=result.skids.data();auto*particles=result.particles.data();auto skid_capacity=result.skids.capacity(),particle_capacity=result.particles.capacity();
 for(int stride:{4,2,1,4,1}){wheels.vertices({1,0,0},{0,1,0},result,stride);subset(full.particles,result.particles,stride);need(result.skids.size()==((skid_count+stride-1)/stride)*6,"reduced skid history count");size_t offset=full.skids.size()-result.skids.size();for(size_t i=0;i<result.skids.size();++i)need(same(result.skids[i],full.skids[offset+i]),"skids retain contiguous newest history without gaps");finite(result.skids);finite(result.particles);need(result.skids.data()==skids&&result.particles.data()==particles&&result.skids.capacity()==skid_capacity&&result.particles.capacity()==particle_capacity,"wheel capacity reused across presets");need(wheels.particle_count()==particle_count&&wheels.skid_count()==skid_count,"detail does not mutate simulation");}
 wheels.clear();wheels.vertices({1,0,0},{0,1,0},result);need(result.skids.empty()&&result.particles.empty(),"wheel reset clears reused output");
 WheelEffects growing(wc);WheelEffectVertices growth;size_t allocations=0,capacity=0;growing.update(contacts,0);
 for(int i=1;i<500;++i){for(auto&c:contacts)c.point.z+=.2f;growing.update(contacts,i*.02);growing.vertices({1,0,0},{0,1,0},growth);if(growth.skids.capacity()!=capacity){++allocations;capacity=growth.skids.capacity();}}
 need(allocations<20,"growing skid history reallocates logarithmically instead of each frame");

 std::cout<<"effect output reuse, stable density, native atlas, contiguous skid history passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
