#pragma once
#include "render_dx9.hpp"
#include <chrono>

namespace ofo {
struct DiagnosticPixels {
 unsigned width=0,height=0;
 std::vector<uint32_t> argb;
 std::string error;
};

struct RendererFrameMetrics {
 uint64_t draw_calls=0,triangles=0,gpu_samples=0;
 int width=0,height=0;
 double gpu_ms=0,scene_cpu_ms=0,hud_cpu_ms=0;
};

struct DiagnosticFrame {
 uint64_t number=0;
 std::chrono::steady_clock::time_point end;
 double simulation_ms=0,draw_ms=0,present_ms=0,capture_ms=0;
 bool presented=false,eligible=false;
};

class PerformanceSession {
 struct Impl;
 std::unique_ptr<Impl> p_;
public:
 PerformanceSession(const fs::path& exe_dir,const char* backend,bool uncapped,bool optimized);
 ~PerformanceSession();
 const fs::path& directory()const;
 void renderer_ready(const Renderer&);
 double capture_pending(Renderer&,const UI&,bool eligible,uint64_t frame)noexcept;
 void record(const DiagnosticFrame&,const UI&,const State&,int camera,const Renderer&)noexcept;
 void finish()noexcept;
};
}
