#include "profile_store.hpp"
#include "file_io.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 auto root=fs::temp_directory_path()/"openfo_profile_store054_test";fs::remove_all(root);ProfileStore store(root);
 auto a=store.create("  Аргон  ",{{"money",4000},{"career",{{"highest_class",1}}}});need(a.name=="Аргон","trimmed Unicode profile name");store.activate(a.id);need(store.active()==a.id,"active player persisted");
 auto b=store.create("Другий");store.save(b.id,{{"money",120},{"career",{{"highest_class",3}}}});need(store.load(a.id)["money"]==4000&&store.load(b.id)["money"]==120,"career files isolated per player");need(store.list().size()==2,"two profiles discoverable");
 bool invalid=false;try{store.folder("../escape");}catch(...){invalid=true;}need(invalid,"profile id cannot escape storage");invalid=false;try{store.create("Аргон");}catch(...){invalid=true;}need(invalid,"duplicate player names rejected");
 store.save(a.id,{{"money",4500}});write_file_atomic(store.folder(a.id)/"savegame.json","broken");auto recovered=store.load(a.id);need(recovered["money"]==4000,"damaged primary uses last good backup");store.save(a.id,{{"money",4600}});need(store.load(a.id)["money"]==4600,"recovered profile can continue saving");bool preserved=false;for(auto&f:fs::directory_iterator(store.folder(a.id)))if(f.path().filename().string().find("savegame.corrupt")!=std::string::npos)preserved=true;need(preserved,"corrupt bytes preserved for recovery");
 auto backup=store.folder(a.id)/"savegame.bak.json";fs::remove(backup);fs::create_directory(backup);bool blocked=false;try{store.save(a.id,{{"money",5000}});}catch(...){blocked=true;}need(blocked&&store.load(a.id)["money"]==4600,"backup write failure must not replace the good primary save");
 need(store.remove(a.id)==b.id&&store.active()==b.id&&!fs::exists(store.folder(a.id)),"delete active and choose surviving profile");
 need(store.remove(b.id).empty()&&store.active().empty()&&store.list().empty()&&fs::exists(root/"index.json"),"last deletion persists an empty roster");
 fs::remove_all(root);std::cout<<"profile creation, isolated savegames, active player and backup recovery passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
