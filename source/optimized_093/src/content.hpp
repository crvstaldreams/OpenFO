#pragma once
#include "assets.hpp"
namespace ofo {
struct CarEntry{int id=0;std::string name;std::vector<int> skins;};
std::vector<CarEntry> car_catalog(const Resources&);
std::vector<std::string> stage_catalog(const Resources&);
struct MusicEntry{std::string path,artist,title;};
std::vector<MusicEntry> music_catalog(const Resources&,bool menu);
}
namespace ofo {std::map<std::string,std::string> language_strings(const Resources&,int index=0);}
