#pragma once
#include "assets.hpp"

namespace ofo {
// Load-time, render-only partition of one large immutable, opaque mesh.
// Empty means "keep original". Never modifies collision geometry, props or assets.
// Each returned packet keeps the source transform/material/vertex attributes;
// individual conservative world-space bounding spheres permit frustum rejection.
struct RenderClusterOptions {
 size_t min_source_triangles=1536;
 size_t target_triangles=768;
 size_t min_leaf_triangles=192;
 size_t max_clusters=64;                 // hard draw-call budget per source
 float min_source_radius=32.f;           // avoid costly analysis of small objects
 float min_cluster_span=36.f;            // do not subdivide spatially compact geometry
 float max_vertex_expansion=1.30f;      // low-VRAM safety valve
};
struct RenderClusterResult {
 std::vector<Mesh> meshes;
 size_t source_triangles=0;
 size_t output_triangles=0;
 size_t output_vertices=0;
};
RenderClusterResult build_render_clusters(const Mesh& source, const Material& material,
                                          const RenderClusterOptions& options={});
}
