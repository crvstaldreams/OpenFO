#pragma once
#include "assets.hpp"
namespace ofo {
struct MeshLods {std::array<std::vector<uint32_t>,3>indices;std::array<float,3>error{};};
// Index-only quadric simplification: original attributes stay intact; seam/open-edge vertices pinned.
MeshLods build_mesh_lods(const std::vector<Vertex>&,const std::vector<uint32_t>&);
// Ordered load-time preprocessing; worker_limit=1 is the serial reference.
// Inputs must remain alive and immutable until this call returns.
std::vector<MeshLods> build_mesh_lods_many(const std::vector<const Mesh*>&,size_t worker_limit=0);
// distance is nearest bounding-sphere view depth, projection_y = cot(FOV/2).
int choose_mesh_lod(const std::array<float,3>&error,float world_scale,float distance,int height,float projection_y,int previous);
}
