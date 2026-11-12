#pragma once
#include <vector>
#include <algorithm>
#include <cstddef>
#include <stdexcept>
namespace ofo {
template<class Vertex,class Key>class OrderedBatch {
 std::vector<Vertex> vertices_;Key key_{};bool valid_=false;size_t limit_;
public:
 explicit OrderedBatch(size_t limit=16380):limit_(limit-limit%3){if(limit_<3)throw std::invalid_argument("Triangle batch capacity");vertices_.reserve(limit_);}
 template<class Emit>void flush(Emit emit){if(!vertices_.empty())emit(key_,vertices_.data(),vertices_.size());vertices_.clear();valid_=false;}
 template<class Emit>void append(Key key,const Vertex* data,size_t count,Emit emit){if(count%3)throw std::invalid_argument("Triangle batch count");while(count){if(valid_&&key_!=key)flush(emit);if(vertices_.size()==limit_)flush(emit);key_=key;valid_=true;size_t n=std::min(count,limit_-vertices_.size());vertices_.insert(vertices_.end(),data,data+n);data+=n;count-=n;}}
};
}
