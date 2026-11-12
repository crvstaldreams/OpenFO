#pragma once
#include <algorithm>
#include <cstdint>
namespace ofo {
// Native results show a whole-dollar amount per category and for the combined
// cents total (e.g. $1.60 -> $2, $7.05 -> $7, combined $8.65 -> $9).
inline int smash_dollars(int64_t cents){
 return int((std::clamp<int64_t>(cents,0,10000000000LL)+50)/100);
}
}
