#pragma once
#include "editor_gizmo.hpp"
#include <string>
namespace ofo {
class TransformNumber{std::string text_;public:void input(char);void backspace();void clear(){text_.clear();}bool empty()const{return text_.empty();}const std::string&text()const{return text_;}std::optional<float> value()const;};
}
