#pragma once
#include "math.hpp"
#include <cstddef>
#include <memory>
#include <optional>
namespace ofo {
struct Geometry;
// Owned world-space snapshot of the static ground eligible for editor picking.
// Queries update diagnostic counters and are intended for the editor UI thread.
class EditorSceneQuery {
 struct Impl;
 std::unique_ptr<Impl> impl_;
public:
 explicit EditorSceneQuery(const Geometry&);
 ~EditorSceneQuery();
 EditorSceneQuery(EditorSceneQuery&&) noexcept;
 EditorSceneQuery& operator=(EditorSceneQuery&&) noexcept;
 EditorSceneQuery(const EditorSceneQuery&)=delete;
 EditorSceneQuery& operator=(const EditorSceneQuery&)=delete;
 std::optional<Vec> ray(Vec origin,Vec direction,float maxDistance=100000)const;
 size_t triangle_count()const;
 size_t last_triangle_tests()const;
};
}
