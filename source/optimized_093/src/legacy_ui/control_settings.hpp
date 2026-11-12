#pragma once
#include "compat.hpp"
#include "../file_io.hpp"

#include <array>
#include <algorithm>
#include <fstream>
#include <cmath>
namespace ofo {
struct ControlSettings {
 enum Action {Gas,Reverse,Left,Right,Handbrake,Nitro,Camera,Recover,ShiftUp,ShiftDown,Brake,Count};
 std::array<int,Count> keys{'W','S','A','D',32,16,'C','R','E','Q','B'};
 std::array<int,Count> secondary{38,40,37,39,0,0,0,0,0,0,0};
 double deadzone=.24,sensitivity=1;
 static const char* action_name(int i){static const char*names[]={"ACCELERATE","BRAKE / REVERSE","STEER LEFT","STEER RIGHT","HANDBRAKE","NITRO","CAMERA","RECOVER","SHIFT UP","SHIFT DOWN","BRAKE PEDAL"};return names[i];}
 static std::string key_name(int k){if((k>=48&&k<=90))return std::string(1,char(k));switch(k){case 0:return "NONE";case 16:return "SHIFT";case 17:return "CTRL";case 18:return "ALT";case 32:return "SPACE";case 37:return "LEFT";case 38:return "UP";case 39:return "RIGHT";case 40:return "DOWN";}return "KEY "+std::to_string(k);}
 static bool allowed(int k){if(k<16||k>=256)return false;for(int v:{27,13,112,113,114,122,9,36,int('F'),int('M'),int('N'),int('H'),int('L'),int('P'),int('T'),91,92,93,144,145,160,161,162,163,164,165})if(k==v)return false;return true;}
 bool assign(int action,int key){if(action<0||action>=Count||!allowed(key))return false;int old=keys[action];for(int i=0;i<Count;++i){if(i!=action&&keys[i]==key)keys[i]=old;if(secondary[i]==key)secondary[i]=0;}keys[action]=key;secondary[action]=0;return true;}
 bool save(const fs::path&path)const{try{write_file_atomic(path,Json{{"version",1},{"keys",keys},{"secondary",secondary},{"xbox_deadzone",deadzone},{"xbox_sensitivity",sensitivity}}.dump(2));return true;}catch(const std::exception&){return false;}}
 bool load(const fs::path&path){try{std::ifstream file(path);Json j;file>>j;auto candidate=*this;auto k=j.at("keys").get<std::array<int,Count>>();auto aliases=j.value("secondary",candidate.secondary);for(int i=0;i<Count;++i){if(!allowed(k[i])||(aliases[i]&&!allowed(aliases[i])))return false;for(int n=0;n<i;++n)if(k[i]==k[n])return false;}candidate.keys=k;candidate.secondary=aliases;candidate.deadzone=j.value("xbox_deadzone",.24);candidate.sensitivity=j.value("xbox_sensitivity",1.);if(!std::isfinite(candidate.deadzone)||!std::isfinite(candidate.sensitivity))return false;candidate.deadzone=std::clamp(candidate.deadzone,.05,.4);candidate.sensitivity=std::clamp(candidate.sensitivity,.5,2.);*this=candidate;return true;}catch(const std::exception&){return false;}}
};
}
