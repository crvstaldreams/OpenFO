#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace ofo {
namespace cpu_jobs_detail {
inline thread_local bool running = false;
struct Scope {
 bool previous = running;
 Scope() { running = true; }
 ~Scope() { running = previous; }
};
}
// Load-time CPU work only. Callbacks own disjoint output slots and never call GL.
// At most eight participants (including this thread), no persistent pool, nested
// calls run serially. A thread-creation failure leaves remaining work to callers.
// All jobs finish before returning; the lowest-index exception is rethrown.
template<class Job>
size_t bounded_cpu_for(size_t count, Job job, size_t worker_limit = 0) {
 if (!count) return 0;
 size_t hardware = std::max(1u, std::thread::hardware_concurrency());
 size_t workers = std::min({count, size_t(8), worker_limit ? worker_limit : hardware});
 if (cpu_jobs_detail::running) workers = 1;
 cpu_jobs_detail::Scope scope;
 if (workers == 1) {
  std::exception_ptr first_failure;
  for (size_t i = 0; i < count; ++i) {
   try { job(i); } catch (...) { if (!first_failure) first_failure = std::current_exception(); }
  }
  if (first_failure) std::rethrow_exception(first_failure);
  return 1;
 }
 std::atomic<size_t> next{0};
 std::mutex failure_mutex;
 size_t failed_index = count;
 std::exception_ptr failure;
 auto run = [&] {
  cpu_jobs_detail::Scope worker_scope;
  for (;;) {
   size_t i = next.fetch_add(1, std::memory_order_relaxed);
   if (i >= count) return;
   try { job(i); }
   catch (...) {
    std::lock_guard<std::mutex> lock(failure_mutex);
    if (i < failed_index) { failed_index = i; failure = std::current_exception(); }
   }
  }
 };
 std::vector<std::thread> threads;
 threads.reserve(workers - 1);
 for (size_t i = 1; i < workers; ++i) {
  try { threads.emplace_back(run); }
  catch (...) { break; }
 }
 run();
 for (auto& thread : threads) thread.join();
 if (failure) std::rethrow_exception(failure);
 return threads.size() + 1;
}
}
