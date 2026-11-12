#include "native_material_profile.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace ofo;
 assert(native_material_profile(0).mode==1);
 assert(native_material_profile(1).mode==2);
 assert(native_material_profile(2).mode==11);
 assert(native_material_profile(5).mode==3&&native_material_profile(30).mode==3);
 assert(native_material_profile(6).mode==4&&native_material_profile(32).mode==15);
 assert(native_material_profile(10).mode==16);
 assert(native_material_profile(26).mode==8);
 assert(native_material_profile(0xfffffffeu).scene&&native_material_profile(0xfffffffeu).mode==8);
 assert(!native_material_profile(13).scene);
 assert(!native_material_profile(17).scene);
 assert(!native_material_profile(34).scene);
 assert(!native_material_profile(400).scene);
 for(unsigned i=0;i<40;++i)assert(native_material_profile(i).role[0]);
 std::cout<<"40 original shader IDs classified; auxiliary passes separate from mesh materials\n";
}
