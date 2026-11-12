#include "profile_store.hpp"
#include "file_io.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <regex>
#include <stdexcept>
namespace ofo {
namespace {
Json object_file(const fs::path&p){auto b=read_file(p,4*1024*1024);auto j=Json::parse(b.begin(),b.end());if(!j.is_object())throw std::runtime_error("Savegame must be a JSON object");return j;}
void preserve_corrupt(const fs::path&p){auto out=p.parent_path()/("savegame.corrupt_"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".json");fs::copy_file(p,out,fs::copy_options::none);}
}
std::string ProfileStore::valid_name(std::string s){while(!s.empty()&&s.front()==' ')s.erase(s.begin());while(!s.empty()&&s.back()==' ')s.pop_back();if(s.empty()||s.size()>128)throw std::runtime_error("Enter a profile name (1-32 characters)");size_t count=0;for(size_t i=0;i<s.size();){unsigned char c=s[i];if(c<32||c==127)throw std::runtime_error("Profile name contains a control character");size_t n=c<128?1:(c>=0xc2&&c<=0xdf)?2:(c>=0xe0&&c<=0xef)?3:(c>=0xf0&&c<=0xf4)?4:0;if(!n||i+n>s.size())throw std::runtime_error("Invalid UTF-8 profile name");for(size_t k=1;k<n;k++)if((static_cast<unsigned char>(s[i+k])&0xc0)!=0x80)throw std::runtime_error("Invalid UTF-8 profile name");i+=n;if(++count>32)throw std::runtime_error("Profile name is limited to 32 characters");}return s;}
fs::path ProfileStore::folder(const std::string&id)const{if(!std::regex_match(id,std::regex("player_[0-9]{4,8}")))throw std::runtime_error("Invalid profile identifier");return root_/id;}
Json ProfileStore::load(const std::string&id)const{auto dir=folder(id);try{return object_file(dir/"savegame.json");}catch(const std::exception&){return object_file(dir/"savegame.bak.json");}}
std::vector<PlayerProfile> ProfileStore::list()const{std::vector<PlayerProfile> out;if(!fs::exists(root_))return out;for(const auto&f:fs::directory_iterator(root_)){if(!f.is_directory())continue;auto id=f.path().filename().u8string();try{auto j=load(id);auto p=j.at("profile");out.push_back({id,valid_name(p.at("name"))});}catch(const std::exception&){}}std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.id<b.id;});return out;}
std::string ProfileStore::active()const{try{auto id=object_file(root_/"index.json").at("active").get<std::string>();folder(id);load(id);return id;}catch(const std::exception&){return {};}}
void ProfileStore::activate(const std::string&id){load(id);fs::create_directories(root_);write_file_atomic(root_/"index.json",Json{{"version",1},{"active",id}}.dump(2));}
std::string ProfileStore::remove(const std::string&id){
 auto dir=folder(id);
 auto profiles=list();
 auto selected=std::find_if(profiles.begin(),profiles.end(),[&](const auto&p){return p.id==id;});
 if(selected==profiles.end()||fs::is_symlink(dir))throw std::runtime_error("Profile unavailable");
 std::string next=active();
 if(next.empty()||next==id){
  next.clear();
  for(const auto&p:profiles)if(p.id!=id){next=p.id;break;}
 }
 // Move first so a failed index write can restore the whole profile.
 auto trash=root_/(".deleted_"+id+"_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::rename(dir,trash);
 try{write_file_atomic(root_/"index.json",Json{{"version",1},{"active",next}}.dump(2));}
 catch(...){fs::rename(trash,dir);throw;}
 std::error_code ec;
 fs::remove_all(trash,ec);
 return next;
}
PlayerProfile ProfileStore::create(std::string name,const Json&initial){name=valid_name(name);for(auto&p:list())if(p.name==name)throw std::runtime_error("A profile with that name already exists");fs::create_directories(root_);std::string id;for(unsigned n=1;n<100000000;++n){char b[40];std::snprintf(b,sizeof b,"player_%04u",n);if(!fs::exists(root_/b)){id=b;break;}}if(id.empty())throw std::runtime_error("Profile storage is full");auto dir=folder(id);fs::create_directory(dir);Json j=initial;if(!j.is_object())j=Json::object();j["version"]=3;j["profile"]={{"id",id},{"name",name}};try{write_file_atomic(dir/"savegame.json",j.dump(2));}catch(...){std::error_code ec;fs::remove(dir,ec);throw;}return{id,name};}
void ProfileStore::save(const std::string&id,const Json&values)const{if(!values.is_object())throw std::runtime_error("Invalid savegame");auto dir=folder(id),path=dir/"savegame.json";auto current=load(id);Json next=values;next["version"]=3;next["profile"]=current.at("profile");Json good;try{good=object_file(path);}catch(const std::exception&){}if(good.is_object())write_file_atomic(dir/"savegame.bak.json",good.dump(2));else if(fs::exists(path))preserve_corrupt(path);write_file_atomic(path,next.dump(2));}
}
