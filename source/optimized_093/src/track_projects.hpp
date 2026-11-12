#pragma once
#include "track_editor.hpp"

namespace ofo {
struct TrackProject {
 fs::path path;
 std::string name, stage;
 bool circuit=false;
};

TrackDocument load_track_project(const fs::path& path);
void save_track_project(const fs::path& path,const TrackDocument& document);
std::vector<TrackProject> list_track_projects(const fs::path& directory,const std::vector<std::string>& stages);
inline bool track_project_has_circuit(const TrackDocument& document){
 return document.closed&&document.route.size()>=3;
}
}
