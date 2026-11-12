#include "cpu_parallel.hpp"
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <chrono>
#include <set>
#include <mutex>
#include <stdexcept>
using namespace ofo;
int main(){
 assert(cpu_lanes_for_cores(0)==1);
 assert(cpu_lanes_for_cores(1)==1);
 assert(cpu_lanes_for_cores(2)==2);
 assert(cpu_lanes_for_cores(3)==2);
 assert(cpu_lanes_for_cores(4)==4);
 assert(cpu_lanes_for_cores(64)==4);
 auto detected=detect_cpu_parallel_policy();
 assert(detected.lanes==1||detected.lanes==2||detected.lanes==4);
 for(unsigned lanes:{1u,2u,4u}){
  CpuTaskLanes tasks(lanes);
  for(int repeat=0;repeat<400;++repeat){
   std::array<std::atomic<int>,33> hits{};
   tasks.for_each(hits.size(),[&](size_t i){hits[i].fetch_add(1);});
   for(auto& h:hits)assert(h.load()==1);
  }
  if(lanes>1){std::mutex m;std::set<std::thread::id> threads;tasks.for_each(16,[&](size_t){std::this_thread::sleep_for(std::chrono::milliseconds(1));std::lock_guard<std::mutex> lock(m);threads.insert(std::this_thread::get_id());});assert(threads.size()>1);}
  bool caught=false;
  try{tasks.for_each(20,[](size_t n){if(n==10)throw std::runtime_error("test exception");});}catch(const std::runtime_error&){caught=true;}
  assert(caught);
  tasks.for_each(10,[](size_t){});
  tasks.configure(1);assert(tasks.lanes()==1);
 }
 std::cout<<"OK: 1/2/4 lanes, deterministic coverage, exception recovery; hardware: logical="<<detected.eligible_logical<<" physical="<<detected.eligible_physical<<" selected="<<detected.lanes<<"\n";
}
