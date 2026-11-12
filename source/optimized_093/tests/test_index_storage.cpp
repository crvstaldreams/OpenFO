#include "index_storage.hpp"
#include <cassert>
int main(){using namespace ofo;std::vector<uint32_t>a{0,1,65535};assert(compact_indices(a));assert(!compact_indices({0,65536,1}));std::array<std::vector<uint32_t>,3>x{a,a,std::vector<uint32_t>{0,1,2}};assert(duplicate_index_level(x,0)==0&&duplicate_index_level(x,1)==0&&duplicate_index_level(x,2)==2);}
