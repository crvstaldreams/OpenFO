#include "state_cache.hpp"
#include <array>
#include <cassert>
#include <random>
#include <cstring>
int main(){ofo::StateCache c;std::array<float,384>a{};int calls=0;uint32_t at=0,count=0;auto submit=[&](uint32_t f,const float*p,uint32_t n){++calls;at=f;count=n;assert(p==a.data()+f*4);return true;};assert(c.constants_trimmed(0,a.data(),96,submit)&&at==0&&count==96);a[4*4]=2;a[7*4+3]=5;assert(c.constants_trimmed(0,a.data(),96,submit)&&at==4&&count==4);int before=calls;assert(c.constants_trimmed(0,a.data(),96,submit)&&calls==before);a[64*4]=8;bool fail=true;auto retry=[&](uint32_t f,const float*p,uint32_t n){assert(f==64&&n==1);return !fail;};assert(!c.constants_trimmed(0,a.data(),96,retry));fail=false;assert(c.constants_trimmed(0,a.data(),96,retry));a[64*4]=0;assert(c.constants_trimmed(0,a.data(),96,submit)&&at==64&&count==1);c.invalidate();assert(c.constants_trimmed(0,a.data(),96,submit)&&count==96);c.enabled=false;assert(c.constants_trimmed(0,a.data(),96,submit)&&count==96);
 ofo::StateCache trimmed;std::array<float,1024>gpu{},reference{};std::mt19937 random(49);
 for(int i=0;i<10000;i++){uint32_t first=random()%256,n=1+random()%(256-first);std::array<float,1024>values{};for(uint32_t j=0;j<n*4;j++)values[j]=float(random()%7);bool failed=i%29==0;if(i%127==0)trimmed.invalidate();bool ok=trimmed.constants_trimmed(first,values.data(),n,[&](uint32_t start,const float*p,uint32_t length){if(failed)return false;std::memcpy(gpu.data()+start*4,p,length*4*sizeof(float));return true;});if(ok)std::memcpy(reference.data()+first*4,values.data(),n*4*sizeof(float));assert(gpu==reference);}
}
