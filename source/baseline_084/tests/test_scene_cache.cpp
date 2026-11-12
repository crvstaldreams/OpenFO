#include "scene_cache.hpp"
#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <string>
int main(){std::map<std::string,std::unique_ptr<int>>c;for(auto k:{"tracks/old.dds","tracks/current.dds","cars/old.dds","cars/current.dds","menu/ui.dds","global/font.dds","track_edits/map.tga"})c[k]=std::make_unique<int>(1);c["tracks/missing.dds"]=nullptr;std::set<const int*>p{c["tracks/current.dds"].get(),c["cars/current.dds"].get(),c["track_edits/map.tga"].get()};auto pin=[&](const int*v){return p.count(v)!=0;};assert(ofo::prune_scene_cache(c,pin)==2&&c.size()==6);assert(ofo::prune_scene_cache(c,pin)==0);p.clear();assert(ofo::prune_scene_cache(c,pin)==3&&c.size()==3);}
