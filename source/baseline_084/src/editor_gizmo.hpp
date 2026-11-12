#pragma once
#include "track_editor.hpp"
namespace ofo {
enum class EditorPivot { Origin, BoundsCenter, BoundsBottom };
enum class EditorAxisSpace { World, Local };
struct EditorRay { Vec origin, direction; };
// TrackDocument v1 adds yaw to the source native basis; position is its origin.
Mat editor_axis_rotation(Vec axis,float radians);
bool rotate_placement_axis(const PropDefinition&,TrackPlacement&,Vec axis,float radians,Vec world_pivot);
Mat placement_rigid_matrix(const PropDefinition&, const TrackPlacement&);
Mat placement_matrix(const PropDefinition&, const TrackPlacement&);
// Preserves each compound mesh's transform relative to its source prop.
// Throws std::runtime_error for a singular source basis.
Mat placement_mesh_matrix(const PropDefinition&, const TrackPlacement&, const Mat& mesh_source_transform);
Vec pivot_local(const PropDefinition&, const TrackPlacement&, EditorPivot);
Vec pivot_world(const PropDefinition&, const TrackPlacement&, EditorPivot);
bool rotate_placement_about_pivot(const PropDefinition&, TrackPlacement&, EditorPivot, float delta);
// Applies local size factors and moves the object pivot within the active
// selection basis. The runtime body frame remains separate from mesh scaling.
bool scale_placement_about_pivot(const PropDefinition&, TrackPlacement&, EditorPivot,
                                 Vec factors, Vec group_pivot, const Mat& selection_axes);
Vec editor_axis(const PropDefinition&, const TrackPlacement&, int axis, EditorAxisSpace);
// Screen coordinates use a top-left origin. Result z is D3D normalized depth.
std::optional<Vec> project_editor_point(Vec, const Mat& view_projection, float width, float height);
std::optional<EditorRay> editor_screen_ray(float x, float y, float width, float height, Vec eye, Vec forward, float fov=1.05f);
// Hit parameters retain the supplied direction's units; OBB rays starting inside return 0.
std::optional<float> ray_prop_obb(const EditorRay&, const PropDefinition&, const TrackPlacement&);
std::optional<Vec> ray_plane(const EditorRay&, Vec origin, Vec normal);
std::optional<float> ray_axis_parameter(const EditorRay&, Vec origin, Vec axis);
bool hit_axis_segment(Vec mouse, Vec a, Vec b, float pixels=9);
float snap_editor_value(float value, float step);
}
