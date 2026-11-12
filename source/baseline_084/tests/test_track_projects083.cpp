#include "track_projects.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ofo;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 const auto path=fs::temp_directory_path()/("openfo-project-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{path};
 TrackDocument stock_edit;
 stock_edit.stage="tracks/forest/forest1/a";
 stock_edit.route={{1,2,3},{3,2,1},{4,2,4}};
 save_track_document(path,stock_edit);
 auto a=stock_edit,b=stock_edit;
 a.route[0].x=20;
 b.route[0].x=50;
 b.closed=false;
 auto first=path/"track_projects"/fs::u8path("Зимовий ліс.oftrack");
 auto second=path/"track_projects"/"Alternate.oftrack";
 save_track_project(first,a);
 save_track_project(second,b);
 std::ofstream(path/"track_projects"/"Broken.oftrack")<<"{";
 require(load_track_document(path,stock_edit.stage)->route[0].x==1,"Save As preserves existing stage edits");
 require(load_track_project(first).route[0].x==20&&load_track_project(second).route[0].x==50,"same-stage projects round-trip independently");
 const auto listed=list_track_projects(path/"track_projects",{stock_edit.stage});
 require(listed.size()==2,"catalog skips invalid projects");
 for(const auto& entry:listed){
  require(entry.stage==stock_edit.stage&&!entry.name.empty(),"project catalog retains base stage and display name");
  require(entry.circuit==(entry.path==first),"closed project enters circuit mode, open project enters free drive");
 }
 require(list_track_projects(path/"track_projects",{"tracks/forest/forest1/b"}).empty(),"projects require their exact stage");
 std::cout<<"Separate save and reopen, Unicode, catalog isolation, and launch mode passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
