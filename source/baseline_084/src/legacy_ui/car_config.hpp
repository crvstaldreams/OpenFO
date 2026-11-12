#pragma once
#include "compat.hpp"
namespace ofo {
// Read a stock equipment profile as literal data only; never executes game scripts.
Json car_configuration(const Resources &resources, const std::string &folder,const std::vector<std::string>& upgrades={});
}
