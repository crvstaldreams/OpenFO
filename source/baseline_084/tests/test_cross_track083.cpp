#include "alpha_track.hpp"
#include "captured_scene.hpp"
#include "track_projects.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void require(bool condition,const char* what){if(!condition)throw std::runtime_error(what);}
int main(int argc,char** argv){
 try{
  if(argc!=4)throw std::runtime_error("Expected retail resources, alpha content and captured scene directories");
  Resources resources(argv[1]);
  resources.mount(argv[2],"alpha_content");
  resources.mount(argv[3],"capture_assets");
  const std::string stage="tracks/forest/forest1/a";
  const auto base_path=stage_geometry(stage);
  auto scene=load_geometry(resources,base_path,stage);
  append_captured_scene(scene,resources);
  TrackAssetLibrary library(resources);
  const auto templates=library.templates(scene);
  const auto donor=std::find_if(templates.begin(),templates.end(),[](const auto& entry){
   return entry.source_geometry.rfind("tracks/pit/",0)==0;
  });
  require(donor!=templates.end(),"stock cross-track template available");
  const auto initial=scene.props.size();
  const auto preview=library.materialize(scene,*donor);
  require(preview==initial&&track_template_only(scene,preview),"preview stays separate from authored bodies");
  require(library.materialize(scene,*donor)==preview,"duplicate donor shares source");
  TrackDocument document;document.stage=stage;
  TrackPlacement placement;placement.source_prop=preview;placement.position={3,5,7};placement.scale={.8f,1.25f,1.f};
  document.placements.push_back(placement);
  bind_track_document(document,scene,resources);
  require(document.placements.front().source_geometry==donor->source_geometry,"source origin is preserved");
  auto round_trip=parse_track_document(track_document_json(document));
  auto fresh=load_geometry(resources,base_path,stage);append_captured_scene(fresh,resources);
  bind_track_document(round_trip,fresh,resources);
  require(round_trip.placements.front().scale.y==1.25f,"instance scale is preserved");
  apply_track_document(fresh,round_trip);
  require(fresh.props.size()==initial+1&&fresh.editor_template_props.empty(),"race gets exactly one new collider");
  std::cout<<"Cross-track object and collider survive saving and reload\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
