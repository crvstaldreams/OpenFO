#pragma once
#include "assets.hpp"
#include <memory>
namespace ofo {class Music {struct Impl;std::unique_ptr<Impl> p_;public:explicit Music(const Resources&);~Music();void update(bool race,float volume,bool enabled,bool next);Json diagnostics()const;};}
