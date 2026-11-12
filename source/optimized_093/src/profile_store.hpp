#pragma once
#include "assets.hpp"
namespace ofo {
struct PlayerProfile {std::string id,name;};
class ProfileStore {
 fs::path root_;
public:
 explicit ProfileStore(fs::path root):root_(std::move(root)){}
 const fs::path& root()const{return root_;}
 std::vector<PlayerProfile> list()const;
 std::string active()const;
 fs::path folder(const std::string&)const;
 PlayerProfile create(std::string name,const Json&initial=Json::object());
 void activate(const std::string&);
 std::string remove(const std::string&);
 Json load(const std::string&)const;
 void save(const std::string&,const Json&)const;
 static std::string valid_name(std::string);
};
}
