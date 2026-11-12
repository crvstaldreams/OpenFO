#include "editor_transform.hpp"
#include <cstdlib>
namespace ofo {
void TransformNumber::input(char c){if(c=='-'){if(!text_.empty()&&text_[0]=='-')text_.erase(0,1);else text_.insert(text_.begin(),'-');}else if((c=='.'||c==',')&&text_.find('.')==std::string::npos&&text_.size()<24)text_+='.';else if(c>='0'&&c<='9'&&text_.size()<24)text_+=c;}
void TransformNumber::backspace(){if(!text_.empty())text_.pop_back();}
std::optional<float> TransformNumber::value()const{if(text_.empty())return {};char*end;float v=std::strtof(text_.c_str(),&end);if(end==text_.c_str()||*end||!std::isfinite(v)||std::abs(v)>1000000)return {};return v;}
}
