#include "track_projects.hpp"
#include "file_io.hpp"

namespace ofo {
namespace {
constexpr size_t max_project_bytes=8*1024*1024;
bool project_file(const fs::path& path){return lower(path.extension().u8string())==".oftrack";}
void validate_project_path(const fs::path& path){
 if(!project_file(path)||path.stem().empty())throw std::runtime_error("Track project must have a name and .oftrack extension");
}
}

TrackDocument load_track_project(const fs::path& path){
 validate_project_path(path);
 const auto data=read_file(path,max_project_bytes);
 auto document=parse_track_document(Json::parse(data.begin(),data.end()));
 if(clean_path(document.stage)!=document.stage)throw std::runtime_error("Track project has an invalid base stage");
 return document;
}

void save_track_project(const fs::path& path,const TrackDocument& document){
 validate_project_path(path);
 if(clean_path(document.stage)!=document.stage)throw std::runtime_error("Track project has an invalid base stage");
 const auto data=track_document_json(document).dump(2);
 if(data.size()>max_project_bytes)throw std::runtime_error("Track project exceeds file limit");
 if(!path.parent_path().empty())fs::create_directories(path.parent_path());
 write_file_atomic(path,data);
}

std::vector<TrackProject> list_track_projects(const fs::path& directory,const std::vector<std::string>& stages){
 std::vector<TrackProject> projects;
 if(directory.empty())return projects;
 std::error_code error;
 for(fs::directory_iterator i(directory,error),end;!error&&i!=end;i.increment(error)){
  std::error_code status_error;
  if(!i->is_regular_file(status_error)||!project_file(i->path()))continue;
  try{
   const auto document=load_track_project(i->path());
   if(std::find(stages.begin(),stages.end(),document.stage)==stages.end())continue;
   projects.push_back({i->path(),i->path().stem().u8string(),document.stage,track_project_has_circuit(document)});
  }catch(const std::exception&){ /* One malformed project cannot hide the rest of the catalog. */ }
 }
 std::sort(projects.begin(),projects.end(),[](const TrackProject& a,const TrackProject& b){return a.path<b.path;});
 return projects;
}
}
