#include "editor_gizmo.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace ofo;
namespace {
bool near(float a,float b){return std::abs(a-b)<.0002f;}
bool near(Vec a,Vec b){return length(a-b)<.0004f;}
void placement_and_pivots(){
 PropDefinition p;p.center={1,3,-2};p.half={2,4,1};p.transform=multiply(translate({80,90,100}),multiply(rotate_x(.4f),rotate_y(.7f)));p.transform[0]*=2;p.transform[1]*=2;p.transform[2]*=2;
 TrackPlacement a{0,{7,8,9},.3f};assert(near(point(placement_matrix(p,a),{}),a.position));
 assert(near(pivot_local(p,a,EditorPivot::Origin),{}));assert(near(pivot_local(p,a,EditorPivot::BoundsCenter),p.center));assert(near(pivot_local(p,a,EditorPivot::BoundsBottom),Vec{1,-1,-2}));
 for(auto mode:{EditorPivot::Origin,EditorPivot::BoundsCenter,EditorPivot::BoundsBottom}){auto b=a;Vec before=pivot_world(p,b,mode);assert(rotate_placement_about_pivot(p,b,mode,1.2f));assert(near(b.yaw,a.yaw));assert(near(pivot_world(p,b,mode),before));}
 assert(near(editor_axis(p,a,0,EditorAxisSpace::World),{1,0,0}));assert(near(editor_axis(p,a,0,EditorAxisSpace::Local),unit(direction(placement_matrix(p,a),{1,0,0}))));
 auto old=a;assert(!rotate_placement_about_pivot(p,a,EditorPivot::Origin,std::numeric_limits<float>::quiet_NaN()));assert(a.yaw==old.yaw&&near(a.position,old.position));
}
void compound_mesh_placement(){
 PropDefinition p;p.transform=multiply(translate({10,20,30}),rotate_y(.8f));p.transform[0]*=-2;p.transform[1]*=-2;p.transform[2]*=-2;p.transform[4]+=.4f;
 TrackPlacement a{0,{-20,4,8},1.1f};Mat relative=multiply(translate({3,-4,5}),rotate_x(.6f));relative[10]*=.7f;
 auto source=multiply(p.transform,relative),expected=multiply(placement_matrix(p,a),relative),actual=placement_mesh_matrix(p,a,source);
 for(int i=0;i<16;i++)assert(near(actual[i],expected[i]));
}
void scale_preserves_pivot_and_local_axes(){
 PropDefinition p;
 p.center={1,2,3};
 p.transform=rotate_y(1.57079632679f);
 TrackPlacement a{0,{5,7,9}};
 const auto pivot=pivot_world(p,a,EditorPivot::BoundsBottom);
 const auto axes=placement_rigid_matrix(p,a);
 assert(scale_placement_about_pivot(p,a,EditorPivot::BoundsBottom,{2,1,.5f},pivot,axes));
 assert(near(pivot_world(p,a,EditorPivot::BoundsBottom),pivot));
 assert(near(a.scale,{2,1,.5f}));
 auto matrix=placement_matrix(p,a);
 assert(near(direction(matrix,{1,0,0}),direction(axes,{2,0,0})));
 assert(near(direction(matrix,{0,0,1}),direction(axes,{0,0,.5f})));
 auto unchanged=a;
 assert(!scale_placement_about_pivot(p,a,EditorPivot::BoundsBottom,{0,1,1},pivot,axes));
 assert(near(a.position,unchanged.position)&&near(a.scale,unchanged.scale));
}
void scale_group_spacing(){
 PropDefinition p;
 TrackPlacement a{0,{0,1,-3}};
 auto axes=rotate_y(1.57079632679f);
 assert(scale_placement_about_pivot(p,a,EditorPivot::Origin,{2,1,1},{},axes));
 assert(near(a.position,{0,1,-6}));
}
void projection_roundtrip(){
 Vec eye{2,3,-8},forward=unit(Vec{.3f,-.2f,1});auto vp=multiply(perspective(1.05f,800.f/600,.1f,1000),look_at(eye,eye+forward));
 for(Vec world:{eye+forward*15,eye+forward*25+Vec{3,2,1}}){auto screen=project_editor_point(world,vp,800,600);assert(screen);auto ray=editor_screen_ray(screen->x,screen->y,800,600,eye,forward);assert(ray&&near(ray->origin,eye));assert(near(ray->direction,unit(world-eye)));}
 assert(!project_editor_point(eye-forward,vp,800,600));assert(!editor_screen_ray(0,0,0,600,eye,forward));assert(!editor_screen_ray(0,0,800,600,eye,{}));
}
void affine_obb(){
 PropDefinition p;p.center={1,2,3};p.half={1,2,3};p.transform=identity();p.transform[0]=-2;p.transform[4]=.5f;p.transform[5]=3;p.transform[10]=.5f;TrackPlacement a{0,{5,7,9},.6f};auto m=placement_matrix(p,a);
 Vec start=point(m,p.center+Vec{0,0,-10}),delta=direction(m,{0,0,1});auto t=ray_prop_obb({start,unit(delta)},p,a);assert(t&&near(*t,3.5f));
 assert(!ray_prop_obb({start,unit(delta)*-1},p,a));assert(!ray_prop_obb({point(m,p.center+Vec{5,0,-10}),unit(delta)},p,a));
 auto inside=ray_prop_obb({point(m,p.center),unit(delta)},p,a);assert(inside&&near(*inside,0));p.transform[0]=0;assert(!ray_prop_obb({start,unit(delta)},p,a));
}
void interaction_math(){
 auto plane=ray_plane({{0,2,0},{0,-1,1}},{},{0,1,0});assert(plane&&near(*plane,{0,0,2}));assert(!ray_plane({{0,2,0},{1,0,0}},{},{0,1,0}));assert(!ray_plane({{0,2,0},{0,1,0}},{},{0,1,0}));
 auto axis=ray_axis_parameter({{3,2,-5},{0,0,1}},{},{1,0,0});assert(axis&&near(*axis,3));assert(!ray_axis_parameter({{0,2,0},{1,0,0}},{},{1,0,0}));
 assert(hit_axis_segment({5,8,0},{0,0,0},{10,0,0}));assert(!hit_axis_segment({20,0,0},{0,0,0},{10,0,0}));assert(hit_axis_segment({1,1,0},{0,0,0},{0,0,0}));
 assert(near(snap_editor_value(1.26f,.5f),1.5f));assert(near(snap_editor_value(-1.26f,.5f),-1.5f));assert(near(snap_editor_value(1.26f,0),1.26f));
}
}
int main(){placement_and_pivots();compound_mesh_placement();scale_preserves_pivot_and_local_axes();scale_group_spacing();projection_roundtrip();affine_obb();interaction_math();std::cout<<"editor gizmo pivot, scale, projection, affine OBB and interaction tests passed\n";}
