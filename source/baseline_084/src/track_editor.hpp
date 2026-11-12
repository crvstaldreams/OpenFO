#pragma once
#include "assets.hpp"
#include <optional>
namespace ofo {
inline constexpr float track_min_scale=.05f,track_max_scale=20.f;
// source_prop is a session-local index. Foreign references persist by
// geometry path and origin ID, independently of the destination's native IDs.
struct TrackTemplate { size_t source_prop=0; uint64_t native_id=0; std::string name; std::string source_geometry; };
struct TrackPlacement {
 size_t source_prop=0; Vec position{}; float yaw=0; uint64_t source_id=0;
 Mat rotation=identity(); bool replace_native=false; std::string source_geometry; Vec scale{1,1,1};
};
struct TrackDocument {
 int version=4; std::string stage;
 std::vector<TrackPlacement> placements;
 std::vector<Vec> route; bool closed=true; size_t start_index=0;
 std::string name,course;
};
inline bool track_template_only(const Geometry& scene,size_t prop){return scene.editor_template_props.count(prop)!=0;}
std::vector<TrackTemplate> track_templates(const Geometry&);
class TrackAssetLibrary {
 const Resources& resources_;
 bool catalog_ready_=false;
 std::vector<TrackTemplate> catalog_;
 std::optional<Geometry> cached_source_;
 const Geometry& load_source(const std::string&);
public:
 explicit TrackAssetLibrary(const Resources& resources):resources_(resources){}
 std::vector<TrackTemplate> templates(const Geometry&);
 size_t materialize(Geometry&,const TrackTemplate&);
};
void validate_track_document(const TrackDocument&);
// Resolve stable local or foreign identity, including v1 Forest capture-order indices.
void bind_track_document(TrackDocument&,Geometry&,const Resources&);
Json track_document_json(const TrackDocument&);
TrackDocument parse_track_document(const Json&);
// Append once to a fresh source geometry; never removes authored native props.
size_t apply_track_document(Geometry&,const TrackDocument&);
fs::path track_edit_path(const fs::path& root,const std::string& stage);
std::optional<TrackDocument> load_track_document(const fs::path& root,const std::string& stage);
void save_track_document(const fs::path& root,const TrackDocument&);
Json track_route_json(const TrackDocument&);
struct TrackMinimap {
 int size=0; Bytes bmp,tga; Vec world_top_left{},world_bottom_right{}; Json metadata;
};
TrackMinimap make_track_minimap(const TrackDocument&,int size=512);
// Writes <sidecar stem>_minimap.bmp, native-compatible .tga and .json beside sidecar.
void save_track_minimap(const fs::path& root,const TrackDocument&,int size=512);
}
