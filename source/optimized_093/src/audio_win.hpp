#pragma once
#include "simulation.hpp"
#include <memory>
namespace ofo {
class Audio {struct Impl;std::unique_ptr<Impl>p_;public:Audio(const Resources&,int,const Geometry&,const std::string&stage);~Audio();void update(const State&,const Input&,float volume,bool enabled,bool surfaces_enabled=true);Json diagnostics()const;};
}
