#pragma once
#include <array>
#include <vector>
#include <cstdint>
namespace ofo {
inline bool compact_indices(const std::vector<uint32_t>&indices){for(auto i:indices)if(i>65535)return false;return true;}
inline int duplicate_index_level(const std::array<std::vector<uint32_t>,3>&levels,int level){for(int i=0;i<level;i++)if(levels[i]==levels[level])return i;return level;}
}
