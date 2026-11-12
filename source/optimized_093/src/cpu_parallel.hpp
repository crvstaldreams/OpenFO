#pragma once
// OpenFO CPU execution policy and persistent task lanes (C++17).
// CPU-only AI planning: NEVER issue OpenGL, D3D9Ex or Bullet world calls here.
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ofo {
struct CpuParallelPolicy {
 unsigned eligible_logical=1, eligible_physical=0, lanes=1;
 bool physical_detected=false;
};
inline unsigned cpu_lanes_for_cores(unsigned count){return count>=4?4u:count>=2?2u:1u;}
inline CpuParallelPolicy detect_cpu_parallel_policy(){
 CpuParallelPolicy p;
 p.eligible_logical=std::max(1u,std::thread::hardware_concurrency());
#ifdef _WIN32
 DWORD_PTR process_mask=0, system_mask=0;
 if(GetProcessAffinityMask(GetCurrentProcess(),&process_mask,&system_mask) && process_mask){
  unsigned n=0;for(DWORD_PTR m=process_mask;m;m&=(m-1))++n;
  p.eligible_logical=std::max(1u,n);
  DWORD needed=0;
  GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&needed);
  if(needed){PROCESSOR_NUMBER current{};GetCurrentProcessorNumberEx(&current);std::vector<unsigned char> bytes(needed);
   auto* first=reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(bytes.data());
   if(GetLogicalProcessorInformationEx(RelationProcessorCore,first,&needed)){
    unsigned cores=0;
    size_t offset=0;
    while(offset+sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)<=needed){
     const auto* info=reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(bytes.data()+offset);
     if(!info->Size||offset+info->Size>needed)break;
     if(info->Relationship==RelationProcessorCore){
      for(WORD i=0;i<info->Processor.GroupCount;++i){
       const auto& entry=info->Processor.GroupMask[i];
       // The process mask is for its primary processor group; unknown
       // groups are ignored instead of oversubscribing the worker pool.
       if(entry.Group==current.Group && (entry.Mask&process_mask)){++cores;break;}
      }
     }
     offset+=info->Size;
    }
    if(cores){p.eligible_physical=cores;p.physical_detected=true;}
   }
  }
 }
#endif
 p.lanes=cpu_lanes_for_cores(p.physical_detected?p.eligible_physical:p.eligible_logical);
 return p;
}
// Number of lanes includes the calling thread. The pool owns at most 3 persistent
// OS threads and is only scheduled for sufficiently large, independent jobs.
class CpuTaskLanes {
 unsigned lanes_=1;
 std::vector<std::thread> workers_;
 std::mutex mutex_;
 std::condition_variable start_,done_;
 std::function<void(size_t)> work_;
 std::atomic<size_t> cursor_{0};
 size_t size_=0;
 unsigned pending_=0;
 uint64_t epoch_=0;
 bool stop_=false;
 std::exception_ptr error_;
 void loop(){uint64_t previous=0;for(;;){
  std::function<void(size_t)> work;size_t n=0;
  {std::unique_lock<std::mutex> lock(mutex_);start_.wait(lock,[&]{return stop_||epoch_!=previous;});if(stop_)return;
   previous=epoch_;work=work_;n=size_;}
  for(size_t i=cursor_.fetch_add(1,std::memory_order_relaxed);i<n;i=cursor_.fetch_add(1,std::memory_order_relaxed)){
   try{work(i);}catch(...){std::lock_guard<std::mutex> lock(mutex_);if(!error_)error_=std::current_exception();}
  }
  {std::lock_guard<std::mutex> lock(mutex_);if(--pending_==0)done_.notify_one();}
 }}
public:
 CpuTaskLanes()=default;
 explicit CpuTaskLanes(unsigned lanes){configure(lanes);}
 CpuTaskLanes(const CpuTaskLanes&)=delete;CpuTaskLanes& operator=(const CpuTaskLanes&)=delete;
 ~CpuTaskLanes(){shutdown();}
 void shutdown(){
  {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}
  start_.notify_all();for(auto& w:workers_)if(w.joinable())w.join();workers_.clear();lanes_=1;
 }
 void configure(unsigned lanes){
  lanes=lanes>=4?4:lanes>=2?2:1;
  if(lanes==lanes_)return;
  shutdown();
  {std::lock_guard<std::mutex> lock(mutex_);stop_=false;epoch_=0;pending_=0;}
  for(unsigned i=1;i<lanes;++i)workers_.emplace_back([this]{loop();});
  lanes_=lanes;
 }
 unsigned lanes()const{return lanes_;}
 template<class F>void for_each(size_t n,F&& callback){
  if(lanes_==1||n<4){for(size_t i=0;i<n;++i)callback(i);return;}
  {std::lock_guard<std::mutex> lock(mutex_);error_=nullptr;work_=std::forward<F>(callback);size_=n;
   cursor_.store(0,std::memory_order_relaxed);pending_=unsigned(workers_.size());++epoch_;}
  start_.notify_all();
  for(size_t i=cursor_.fetch_add(1,std::memory_order_relaxed);i<n;i=cursor_.fetch_add(1,std::memory_order_relaxed)){
   try{callback(i);}catch(...){std::lock_guard<std::mutex> lock(mutex_);if(!error_)error_=std::current_exception();}
  }
  std::exception_ptr error;{std::unique_lock<std::mutex> lock(mutex_);done_.wait(lock,[&]{return pending_==0;});work_={};error=error_;}
  if(error)std::rethrow_exception(error);
 }
};
}
