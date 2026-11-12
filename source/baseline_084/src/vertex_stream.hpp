#pragma once
#include <cstddef>
#include <stdexcept>
namespace ofo {
class VertexStream {
 std::size_t capacity_,cursor_=0;
public:
 struct Range{std::size_t offset;bool discard;};
 explicit VertexStream(std::size_t capacity):capacity_(capacity){}
 void begin_frame(){cursor_=0;}
 Range allocate(std::size_t bytes,std::size_t stride){
  if(!stride||stride%4||!bytes||bytes%stride)throw std::invalid_argument("Invalid vertex stream range");
  if(bytes>capacity_)throw std::length_error("Vertex stream capacity exceeded");
  auto padding=(stride-cursor_%stride)%stride;
  bool discard=cursor_==0||padding>capacity_-cursor_||bytes>capacity_-cursor_-padding;
  std::size_t offset=discard?0:cursor_+padding;cursor_=offset+bytes;return{offset,discard};
 }
};
}
