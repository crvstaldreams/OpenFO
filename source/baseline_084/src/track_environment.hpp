#pragma once
#include "assets.hpp"
#include <algorithm>
#include <cmath>
namespace ofo {
// Runtime overrides. Native atmosphere/lighting assets remain the source of
// colors and directions; these values never rewrite the original resources.
struct TrackEnvironment {
 bool night=false,rain=false;
 float sun_intensity=1.f,fog_density=0.f,rain_strength=.65f;
 void validate(){
  sun_intensity=std::isfinite(sun_intensity)?std::clamp(sun_intensity,0.f,2.f):1.f;
  fog_density=std::isfinite(fog_density)?std::clamp(fog_density,0.f,1.f):0.f;
  rain_strength=std::isfinite(rain_strength)?std::clamp(rain_strength,0.f,1.f):.65f;
 }
 Json json()const{auto e=*this;e.validate();return {{"time_of_day",e.night?"night":"day"},{"weather",e.rain?"rain":"dry"},{"sun_intensity",e.sun_intensity},{"fog_density",e.fog_density},{"rain_strength",e.rain_strength}};}
 static TrackEnvironment from_json(const Json&j){TrackEnvironment e;if(!j.is_object())return e;
  auto text=[&](const char*k,const char*v){return j.contains(k)&&j[k].is_string()&&j[k].get<std::string>()==v;};
  e.night=text("time_of_day","night");e.rain=text("weather","rain");
  auto number=[&](const char*k,float fallback){try{return j.contains(k)&&j[k].is_number()?j[k].get<float>():fallback;}catch(const std::exception&){return fallback;}};
  e.sun_intensity=number("sun_intensity",1);e.fog_density=number("fog_density",0);e.rain_strength=number("rain_strength",.65f);e.validate();return e;
 }
};
// Settings are indexed by complete stage path, including a/b variants. This
// document is stored beside profile settings, independently of career progress.
struct TrackRaceSettings {
 TrackEnvironment environment;int ai_handicap_level=1;bool ai_override=false;
 Json json()const{return {{"environment",environment.json()},{"ai_handicap_level",std::clamp(ai_handicap_level,1,7)},{"ai_override",ai_override}};}
 static TrackRaceSettings from_json(const Json&j){TrackRaceSettings s;if(!j.is_object())return s;if(j.contains("environment"))s.environment=TrackEnvironment::from_json(j["environment"]);
  if(j.contains("ai_handicap_level")&&j["ai_handicap_level"].is_number_integer()){try{s.ai_handicap_level=std::clamp(j["ai_handicap_level"].get<int>(),1,7);}catch(const std::exception&){}}
  s.ai_override=j.contains("ai_override")&&j["ai_override"].is_boolean()&&j["ai_override"].get<bool>();return s;}
};
}
