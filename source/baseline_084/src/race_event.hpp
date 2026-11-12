#pragma once
#include <string_view>

namespace ofo {
enum class RaceMode { Circuit, Battle, Stunt, FreeRoam };

inline RaceMode race_mode(std::string_view track,std::string_view event={},bool free_roam=false){
 if(free_roam||event=="FREEROAM")return RaceMode::FreeRoam;
 if(event=="STUNT"||track.find("/stunt/")!=track.npos)return RaceMode::Stunt;
 if(track.find("/derby/")!=track.npos||event=="DERBY")return RaceMode::Battle;
 return RaceMode::Circuit;
}

inline bool arena_circuit(std::string_view track){return track.find("/arena/")!=track.npos;}

enum class StuntKind { None, Longjump, Highjump, Darts, Bowling, Clown, Bullseye };
inline constexpr float stunt_max_launch_angle=85.f;

inline StuntKind stunt_kind(std::string_view subevent){
 if(subevent=="STUNT_LONGJUMP")return StuntKind::Longjump;
 if(subevent=="STUNT_HIGHJUMP")return StuntKind::Highjump;
 if(subevent=="STUNT_DARTS")return StuntKind::Darts;
 if(subevent=="STUNT_BOWLING")return StuntKind::Bowling;
 if(subevent=="STUNT_CLOWN")return StuntKind::Clown;
 if(subevent=="STUNT_CURLING")return StuntKind::Bullseye;
 return StuntKind::None;
}
}
