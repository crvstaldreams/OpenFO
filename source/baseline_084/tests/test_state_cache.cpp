#include "state_cache.hpp"
#include <cassert>
#include <iostream>
int main(){
 ofo::StateCache cache;int attempts=0;bool success=true;
 auto submit=[&]{++attempts;return success;};
 unsigned value=42;auto key=ofo::StateCache::Key{0,7,0};
 assert(cache.set(key,&value,sizeof(value),submit));
 assert(cache.set(key,&value,sizeof(value),submit));
 assert(attempts==1&&cache.calls==1&&cache.skipped==1);
 // A failed change preserves the prior known value and retries on next call.
 success=false;++value;assert(!cache.set(key,&value,sizeof(value),submit));
 assert(!cache.set(key,&value,sizeof(value),submit));assert(attempts==3);
 success=true;assert(cache.set(key,&value,sizeof(value),submit));assert(attempts==4);
 unsigned read=0;assert(cache.read(key,&read,sizeof(read))&&read==value);
 // FVF/declaration callers erase the opposite layout after successful writes.
 auto fvf=ofo::StateCache::Key{5,0,0},decl=ofo::StateCache::Key{6,0,0};
 assert(cache.set(fvf,&value,sizeof(value),submit));cache.erase(decl);
 assert(cache.set(decl,&value,sizeof(value),submit));cache.erase(fvf);
 auto before=attempts;assert(cache.set(fvf,&value,sizeof(value),submit));assert(attempts==before+1);
 float constants[96*4]{};assert(cache.constants(0,constants,96,submit));
 before=attempts;assert(cache.constants(0,constants,96,submit));assert(attempts==before);
 assert(cache.constants(32,constants+32*4,16,submit));assert(attempts==before);
 constants[35*4]=1;success=false;assert(!cache.constants(32,constants+32*4,16,submit));
 success=true;assert(cache.constants(32,constants+32*4,16,submit));
 before=attempts;assert(cache.constants(0,constants,96,submit));assert(attempts==before);
 // Frame counters reset without losing persistent state; Reset loses it all.
 cache.begin_frame();assert(cache.calls==0&&cache.skipped==0);
 assert(cache.constants(0,constants,96,submit));assert(cache.skipped==1&&cache.calls==0);
 cache.invalidate();assert(cache.constants(0,constants,96,submit));assert(cache.calls==1);
 // Last legal register and invalid ranges pass through without array access.
 float last[4]={1,2,3,4};assert(cache.constants(255,last,1,submit));
 before=attempts;assert(cache.constants(255,last,1,submit));assert(attempts==before);
 success=false;assert(!cache.constants(256,last,1,submit));
 assert(!cache.constants(255,last,2,submit));assert(!cache.constants(0,nullptr,1,submit));
 assert(attempts==before+3);
 cache.enabled=false;before=attempts;cache.set(key,&value,sizeof(value),submit);cache.set(key,&value,sizeof(value),submit);cache.constants(0,constants,96,submit);cache.constants(0,constants,96,submit);assert(attempts==before+4);
 std::cout<<"state cache tests passed\n";
}
