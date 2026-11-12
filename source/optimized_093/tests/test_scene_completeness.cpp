#include "assets.hpp"
#include "material_bindings.hpp"
#include "frontend.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <set>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{if(argc<2)return 0;Resources r(argv[1]);
 auto menu=load_geometry(r,"tracks/menu/menu1/geometry/track_geom.w32");
 Frontend ui(r);size_t slots=0;for(size_t i=0;i<ui.track_slots.size();i++){ui.select_track(i);auto stage=ui.selected_stage();need(r.exists(stage_geometry(stage)),"Every actual menu slot resolves geometry");need(r.exists(stage+"/data/startpoints.bed"),"Every actual menu slot resolves startpoints");slots++;}need(slots>=40,"Original menu slots fixture");
 size_t trees=0;for(auto&m:menu.meshes)trees+=m.name.rfind("tree_",0)==0;
 need(trees==761,"Menu must include 761 native tree surface batches (400 placements), not only 259 static batches");
 size_t leaf=0;for(auto&m:menu.meshes)if(m.billboard){leaf++;need(!m.solid,"Foliage cannot become a solid wall");}need(leaf==101,"Every menu leaf placement must be instanced, including reused templates");
 need(menu.meshes.size()==1020,"Menu complete first LOD batches");
 for(auto&m:menu.materials)if(lower(m.texture[0])=="colormap.tga")need(native_material_texture(r,menu.source,m,0,"tracks/menu/menu1/a/lighting/lightmap1_w2.dds")=="tracks/menu/menu1/a/lighting/lightmap1_w2.dds","Menu lightmap alias");
 size_t unresolved=0;for(auto&m:menu.meshes)for(auto&v:m.vertices)if((v.color>>24)==0xfd)unresolved++;
 need(unresolved==0,"Runtime vertexcolor references must resolve to stage lighting, not become diffuse RGB");
 auto forest=load_geometry(r,"tracks/forest/forest1/geometry/track_geom.w32");trees=0;size_t compact=0;
 for(auto&m:forest.meshes){trees+=m.name.rfind("tree_",0)==0;compact+=m.name=="dyna_warehouse_a_iv_ladders_01";}
 need(trees==6490,"Forest full native tree surfaces");size_t leaves=0;for(auto&m:forest.meshes)if(m.billboard)leaves++;need(leaves==1335,"All repeated forest leaf templates are instanced");
 auto colors=r.read("tracks/forest/forest1/a/lighting/vertexcolors_w2.w32");uint32_t expected_color=0;std::memcpy(&expected_color,colors.data()+size_t(0x1c900)*4,4);
 for(auto&m:forest.meshes)if(m.name=="tree_81_2"){need(m.billboard&&m.vertices[0].color==expected_color,"Leaf uses instance palette and stage color, not template lighting");Vec center{};for(int k=0;k<4;k++)center+=m.vertices[k].position*.25f;need(length(center)>100,"Reusable leaf template moved into world, not left at origin");}
 auto winter=load_geometry(r,"tracks/winter/winter1/geometry/track_geom.w32");need(winter.native_tree_instances>0,"Winter placeholder atlas rectangles load successfully");need(compact==1,"Native compact object placement must load before capture replacement");
 for(auto&m:forest.meshes)if(m.name=="dyna_warehouse_a_iv_ladders_01")need(std::abs(m.transform[12]-299.8382568f)<.001f,"Native compact world transform");
 std::cout<<"Menu "<<menu.meshes.size()<<" batches; Forest "<<forest.meshes.size()<<" batches; all native trees and compact placements loaded\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
