#pragma once
#include <cstddef>
namespace ofo {
// Enumerate all live texture pointers and unbind device textures before calling.
// Keep UI/font textures and missing-resource entries cached.
template<class Cache,class Pinned>std::size_t prune_scene_cache(Cache&cache,Pinned pinned){
 std::size_t removed=0;for(auto it=cache.begin();it!=cache.end();){const auto&key=it->first;
 bool scene=key.rfind("tracks/",0)==0||key.rfind("cars/",0)==0||key.rfind("track_edits/",0)==0;
 if(scene&&it->second&&!pinned(it->second.get())){it=cache.erase(it);++removed;}else ++it;}return removed;
}
}
