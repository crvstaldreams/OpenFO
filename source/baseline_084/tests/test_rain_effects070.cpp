#include "rain_effects.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace ofo;
namespace {
Mesh plane(float y,size_t material=0){Mesh m;m.material=material;for(auto p:{Vec{-120,y,-120},Vec{120,y,-120},Vec{120,y,120},Vec{-120,y,120}}){Vertex v;v.position=p;m.vertices.push_back(v);}m.indices={0,1,2,0,2,3};return m;}
Geometry scene(bool roof,bool water){Geometry g;g.materials.resize(2);g.materials[0].name="road";g.materials[1].name="water_surface";g.meshes.push_back(plane(-3));if(water)g.meshes.push_back(plane(0,1));if(roof)g.meshes.push_back(plane(8));return g;}
void run(RainEffects&r,Vec eye,int quality=3){for(int i=0;i<=360;++i){r.update(eye,i/60.,true,1,quality,1);assert(r.stats().rays<=r.stats().ray_budget);assert(r.drop_count()<=RainEffects::max_drops);assert(r.splash_count()<=RainEffects::max_impacts);assert(r.ripple_count()<=RainEffects::max_impacts);}}
void finite_output(const RainEffectVertices&v){for(auto list:{&v.streaks,&v.splashes,&v.ripples}){assert(list->size()%3==0);for(auto&p:*list)assert(std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z));}}
}
int main(){
 RainEffects rain;rain.configure(scene(false,false));run(rain,{0,2,0});assert(rain.drop_count()>0&&rain.stats().ground_impacts>0&&rain.stats().water_impacts==0);RainEffectVertices out;rain.vertices({1,0,0},{0,1,0},out);finite_output(out);assert(!out.streaks.empty()&&!out.splashes.empty()&&out.ripples.empty());
 RainEffects roof;roof.configure(scene(true,false));run(roof,{0,2,0});roof.vertices({1,0,0},{0,1,0},out);assert(roof.stats().ground_impacts>0);for(auto&v:out.streaks)assert(v.position.y>=8);for(auto&v:out.splashes)assert(v.position.y>=8);
 RainEffects coveredWater;coveredWater.configure(scene(true,true));run(coveredWater,{0,2,0});assert(coveredWater.stats().ground_impacts>0&&coveredWater.stats().water_impacts==0&&coveredWater.ripple_count()==0);
 RainEffects enclosed;enclosed.configure(scene(true,false));run(enclosed,{0,-20,0});assert(enclosed.drop_count()==0&&enclosed.splash_count()==0);
 RainEffects water;water.configure(scene(false,true));run(water,{0,2,0});assert(water.stats().water_impacts>0&&water.stats().ground_impacts==0&&water.ripple_count()>0);water.vertices({1,0,0},{0,1,0},out);assert(!out.ripples.empty());for(auto&v:out.ripples)assert(v.position.y>0&&v.position.y<.1f);finite_output(out);
 const auto count=water.drop_count(),impacts=water.stats().water_impacts;water.update({0,2,0},6,true,1,3,1);assert(water.drop_count()==count&&water.stats().water_impacts==impacts&&water.stats().rays==0);water.update({0,2,0},6,false,1,3,1);assert(water.drop_count()==0&&water.ripple_count()==0);water.vertices({1,0,0},{0,1,0},out);assert(out.streaks.empty()&&out.ripples.empty());
 rain.update({0,2,0},6.1,true,1,3,2);assert(rain.drop_count()==0&&rain.stats().ground_impacts==0);rain.update({0,2,0},1,true,1,3,2);assert(rain.drop_count()==0);rain.update({NAN,2,0},2,true,1,3,2);assert(rain.drop_count()==0);rain.update({0,2,0},INFINITY,true,1,3,2);assert(rain.drop_count()==0);
 RainEffects low,high;low.configure(scene(false,false));high.configure(scene(false,false));run(low,{0,2,0},0);run(high,{0,2,0},3);assert(low.drop_count()<high.drop_count());
 RainEffects deterministic;deterministic.configure(scene(false,false));run(deterministic,{0,2,0});high.vertices({1,0,0},{0,1,0},out);RainEffectVertices same;deterministic.vertices({1,0,0},{0,1,0},same);assert(out.streaks.size()==same.streaks.size());for(size_t i=0;i<same.streaks.size();++i)assert(length(out.streaks[i].position-same.streaks[i].position)==0);
 for(int i=361;i<2000;++i){Vec moving{float(i%150),2,float(i%100)};high.update(moving,i/60.,true,1,i%4,1);assert(high.stats().rays<=high.stats().ray_budget&&high.drop_count()<=RainEffects::max_drops&&high.ripple_count()<=RainEffects::max_impacts);}
 high.clear();assert(high.drop_count()==0&&high.ripple_count()==0);high.configure(Geometry{});run(high,{0,2,0});high.vertices({NAN,0,0},{0,INFINITY,0},out);finite_output(out);assert(high.stats().ground_impacts==0);
 std::cout<<"rain070: roof exclusion, first-hit water, ripples, pause, reset, quality, finite output and bounded rays/particles passed\n";
}
