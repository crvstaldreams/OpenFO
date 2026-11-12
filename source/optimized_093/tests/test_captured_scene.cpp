#include "captured_scene.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace ofo;void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{need(argc==3,"data + capture directory required");Resources r(argv[1]);r.mount(argv[2],"capture_assets");auto g=load_geometry(r,"tracks/forest/forest1/geometry/track_geom.w32");auto original=g;auto report=append_captured_scene(g,r);need(report["exact_captured_instances"]==4129,"all exact Forest capture bodies retained");need(report["instances"]==4481,"previously frozen Forest native props now receive bodies");need(g.meshes.size()==original.meshes.size(),"attaching physics cannot duplicate/remove visuals");need(std::count_if(g.props.begin(),g.props.end(),[](const auto&p){return p.collision_sound>0;})>1000,"native CollisionSound categories populated");need(report.at("native_bonus_instances").get<int>()>1000,"native BonusType definitions attach to captured instances");bool has_fence=false,has_rock=false;for(const auto&p:g.props){if(p.bonus.type==2){need(p.bonus.cents==47&&std::abs(p.bonus.nitro-.006f)<1e-6f,"native fence price and mayhem");has_fence=true;}if(p.bonus.type==9){need(p.bonus.cents==0&&std::abs(p.bonus.nitro-.18f)<1e-6f,"native rock only awards mayhem");has_rock=true;}}need(has_fence&&has_rock,"Forest fixture contains paid and nitro-only objects");std::set<uint64_t>ids;for(auto&p:g.props)need(ids.insert(p.id).second,"track-local body identity unique");for(size_t i=0;i<g.meshes.size();i++){const auto&a=g.meshes[i];const auto&b=original.meshes[i];need(a.name==b.name&&a.transform==b.transform&&a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"all authored instances and topology preserved");if(a.captured){need(a.prop_index>=0&&size_t(a.prop_index)<g.props.size(),"dynamic visual attached to body");auto ref=g.materials[a.material].texture[0];need(ref.empty()||r.exists(r.texture(g.source,ref)),"native prop texture resolves");}}

 // A live native capture of Forest1/a is an independent placement oracle.
 // The full location has 4481 placements; only 4129 belong to variant a.
 std::array<size_t,3> expected={4129,4002,4000};
 for(int variant=0;variant<3;variant++){
  auto stage=std::string("tracks/forest/forest1/")+char('a'+variant);
  auto selected=load_geometry(r,g.source,stage);
  need(selected.native_props.size()==expected[variant],"same-location native stage placement counts");
  need(selected.native_compact_instances==4481,"complete source count retained for audit");
  need(selected.filtered_compact_instances==4481-expected[variant],"filtered stage count");
  for(auto&p:selected.native_props)need((p.placement_flags&(0x2000u<<variant))!=0,"placement inclusion matches chosen stage");
  auto stage_report=append_captured_scene(selected,r);
  need(stage_report["instances"]==expected[variant],"physics only attaches to selected stage props");
  if(variant==0){need(stage_report["exact_captured_instances"]==4129,"every live-captured Forest1/a instance included, with no extra placement");need(stage_report["native_type_instances"]==0,"no unrecorded alternate-stage body leaks into native capture stage");}
 }
 auto other=load_geometry(r,"tracks/forest/forest2/geometry/track_geom.w32");auto second=append_captured_scene(other,r);need(second["instances"].get<size_t>()>4000,"other track has its own native dynamic bodies");need(second["exact_captured_instances"]==0,"Forest1 capture transforms cannot leak to Forest2");for(auto&p:other.props){bool found=false;for(auto&n:other.native_props)if(p.transform==n.transform){found=true;break;}need(found,"each body retains placement from its own track");}std::cout<<report.dump()<<'\n'<<second.dump()<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
