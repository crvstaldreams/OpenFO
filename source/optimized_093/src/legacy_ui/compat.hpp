#pragma once
#include "../assets.hpp"
namespace ofo {
inline std::string ui_canonical(std::string s){return clean_path(std::move(s));}
std::string text_decode(const Bytes&);
Json parse_data(const std::string&);
}
