#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

namespace ofo {
// Portable byte-exact cache. The callback returns success, so failed device
// writes never become cached state. Pointer values are identities, not owners.
class StateCache {
public:
 using Key=std::array<std::uint32_t,3>;
 bool enabled=true;
 std::uint64_t calls=0,skipped=0; // Actual backend attempts / avoided attempts.
 void begin_frame(){calls=skipped=0;}
 void invalidate(){values.clear();constant_valid.fill(false);}
 void erase(Key key){values.erase(key);}
 bool read(Key key,void* value,std::size_t size)const {
  if(!enabled)return false;
  auto it=values.find(key);if(it==values.end()||it->second.size()!=size)return false;
  if(size)std::memcpy(value,it->second.data(),size);
  return true;
 }
 bool matches(Key key,const void* value,std::size_t size)const {
  auto it=values.find(key);return it!=values.end()&&it->second.size()==size&&(!size||std::memcmp(it->second.data(),value,size)==0);
 }
 void remember(Key key,const void* value,std::size_t size){
  auto& bytes=values[key];bytes.resize(size);if(size)std::memcpy(bytes.data(),value,size);
 }
 template<class Submit>bool set(Key key,const void* value,std::size_t size,Submit submit){
  if(!enabled){++calls;return submit();}if(matches(key,value,size)){++skipped;return true;}
  ++calls;if(!submit())return false;remember(key,value,size);return true;
 }
 // Constants are cached per register, including overlapping upload ranges.
 template<class Submit>bool constants(std::uint32_t first,const float* data,std::uint32_t count,Submit submit){
  // D3D9 exposes at most 256 float registers. Preserve backend validation for
  // invalid ranges instead of touching the fixed arrays.
  if(!enabled){++calls;return submit();}
  if(!data||first>256||count>256-first){++calls;return submit();}
  bool same=true;
  for(std::uint32_t i=first;i<first+count;++i)if(!constant_valid[i]){same=false;break;}
  if(same&&(!count||std::memcmp(constant_values.data()+first*4,data,count*4*sizeof(float))==0)){++skipped;return true;}
  ++calls;if(!submit())return false;
  if(count)std::memcpy(constant_values.data()+first*4,data,count*4*sizeof(float));
  for(std::uint32_t i=first;i<first+count;++i)constant_valid[i]=true;
  return true;
 }
 // Preserve one backend call per upload, trimming only unchanged prefix/suffix.
 // Interior gaps are sent too: splitting them adds CPU/driver calls on D3D9.
 template<class Submit>bool constants_trimmed(std::uint32_t first,const float*data,std::uint32_t count,Submit submit){
  if(!enabled||!data||first>256||count>256-first){++calls;return submit(first,data,count);}
  auto equal=[&](std::uint32_t i){return constant_valid[first+i]&&std::memcmp(constant_values.data()+(first+i)*4,data+i*4,4*sizeof(float))==0;};
  std::uint32_t lo=0;while(lo<count&&equal(lo))++lo;if(lo==count){++skipped;return true;}
  std::uint32_t hi=count;while(hi>lo&&equal(hi-1))--hi;
  ++calls;if(!submit(first+lo,data+lo*4,hi-lo))return false;
  std::memcpy(constant_values.data()+(first+lo)*4,data+lo*4,(hi-lo)*4*sizeof(float));
  for(auto i=first+lo;i<first+hi;++i)constant_valid[i]=true;return true;
 }

private:
 std::map<Key,std::vector<unsigned char>> values;
 std::array<float,256*4> constant_values{};
 std::array<bool,256> constant_valid{};
};
}
