#include "editor_gizmo.hpp"
#include <limits>
#include <stdexcept>
namespace ofo {
namespace {
bool finite(Vec a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
bool valid_ray(const EditorRay&r){return finite(r.origin)&&finite(r.direction)&&dot(r.direction,r.direction)>1e-14f;}
// The cross-product adjugate preserves scale, shear and reflected native bases.
bool inverse_basis(const Mat&m, Vec v, Vec& out){
 Vec a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[8],m[9],m[10]};
 double det=double(a.x)*(double(b.y)*c.z-double(b.z)*c.y)-double(b.x)*(double(a.y)*c.z-double(a.z)*c.y)+double(c.x)*(double(a.y)*b.z-double(a.z)*b.y);
 double scale=double(length(a))*length(b)*length(c);
 if(!std::isfinite(det)||!std::isfinite(scale)||scale==0||std::abs(det)<=scale*1e-8)return false;
 out={float((double(v.x)*(double(b.y)*c.z-double(b.z)*c.y)+double(v.y)*(double(b.z)*c.x-double(b.x)*c.z)+double(v.z)*(double(b.x)*c.y-double(b.y)*c.x))/det),float((double(v.x)*(double(c.y)*a.z-double(c.z)*a.y)+double(v.y)*(double(c.z)*a.x-double(c.x)*a.z)+double(v.z)*(double(c.x)*a.y-double(c.y)*a.x))/det),float((double(v.x)*(double(a.y)*b.z-double(a.z)*b.y)+double(v.y)*(double(a.z)*b.x-double(a.x)*b.z)+double(v.z)*(double(a.x)*b.y-double(a.y)*b.x))/det)};
 return finite(out);
}
}
Mat editor_axis_rotation(Vec axis,float radians){axis=unit(axis);float c=std::cos(radians),s=std::sin(radians),t=1-c;auto a=axis;return {t*a.x*a.x+c,t*a.x*a.y+s*a.z,t*a.x*a.z-s*a.y,0,t*a.x*a.y-s*a.z,t*a.y*a.y+c,t*a.y*a.z+s*a.x,0,t*a.x*a.z+s*a.y,t*a.y*a.z-s*a.x,t*a.z*a.z+c,0,0,0,0,1};}
bool rotate_placement_axis(const PropDefinition&p,TrackPlacement&a,Vec axis,float radians,Vec pivot){if(!finite(axis)||!finite(pivot)||length(axis)<1e-7f||!std::isfinite(radians))return false;auto r=editor_axis_rotation(axis,radians);auto next=a;next.position=pivot+direction(r,a.position-pivot);next.rotation=multiply(r,a.rotation);if(!finite(next.position))return false;a=next;return true;}
Mat placement_rigid_matrix(const PropDefinition&p,const TrackPlacement&a){auto basis=p.transform;basis[12]=basis[13]=basis[14]=0;return multiply(translate(a.position),multiply(a.rotation,multiply(rotate_y(a.yaw),basis)));}
Mat placement_matrix(const PropDefinition&p,const TrackPlacement&a){
 auto matrix=placement_rigid_matrix(p,a);
 for(int col=0;col<3;++col)for(int row=0;row<3;++row)matrix[col*4+row]*=a.scale[col];
 return matrix;
}
Mat placement_mesh_matrix(const PropDefinition&p,const TrackPlacement&a,const Mat&mesh){
 Mat relative=identity();
 for(int col=0;col<4;col++){
  Vec v{mesh[col*4],mesh[col*4+1],mesh[col*4+2]},local;
  if(col==3)v=v-Vec{p.transform[12],p.transform[13],p.transform[14]};
  if(!inverse_basis(p.transform,v,local))throw std::runtime_error("Track source transform is singular");
  relative[col*4]=local.x;relative[col*4+1]=local.y;relative[col*4+2]=local.z;
 }
 return multiply(placement_matrix(p,a),relative);
}
Vec pivot_local(const PropDefinition&p,const TrackPlacement&,EditorPivot mode){if(mode==EditorPivot::Origin)return {};if(mode==EditorPivot::BoundsBottom)return p.center-Vec{0,p.half.y,0};return p.center;}
Vec pivot_world(const PropDefinition&p,const TrackPlacement&a,EditorPivot mode){return point(placement_matrix(p,a),pivot_local(p,a,mode));}
bool rotate_placement_about_pivot(const PropDefinition&p,TrackPlacement&a,EditorPivot mode,float delta){
 if(!std::isfinite(delta)||!std::isfinite(a.yaw)||!finite(a.position))return false;
 Vec pivot=pivot_world(p,a,mode);return rotate_placement_axis(p,a,{0,1,0},delta,pivot);
}
bool scale_placement_about_pivot(const PropDefinition&p,TrackPlacement&a,EditorPivot mode,
                                Vec factors,Vec group_pivot,const Mat&selection_axes){
 if(!finite(factors)||!finite(group_pivot))return false;
 auto next=a;
 for(int axis=0;axis<3;++axis){
  next.scale[axis]*=factors[axis];
  if(next.scale[axis]<track_min_scale||next.scale[axis]>track_max_scale)return false;
 }
 Vec local;
 if(!inverse_basis(selection_axes,pivot_world(p,a,mode)-group_pivot,local))return false;
 for(int axis=0;axis<3;++axis)local[axis]*=factors[axis];
 const auto pivot=group_pivot+direction(selection_axes,local);
 next.position+=pivot-pivot_world(p,next,mode);
 if(!finite(next.position)||std::abs(next.position.x)>1000000.f||
    std::abs(next.position.y)>1000000.f||std::abs(next.position.z)>1000000.f)return false;
 a=std::move(next);
 return true;
}
Vec editor_axis(const PropDefinition&p,const TrackPlacement&a,int axis,EditorAxisSpace space){if(axis<0||axis>2)return {};Vec v{};v[axis]=1;return space==EditorAxisSpace::World?v:unit(direction(placement_matrix(p,a),v),v);}
std::optional<Vec> project_editor_point(Vec v,const Mat&m,float w,float h){
 if(!finite(v)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0)return {};
 float clipw=m[3]*v.x+m[7]*v.y+m[11]*v.z+m[15];if(!std::isfinite(clipw)||clipw<=1e-7f)return {};
 Vec clip=point(m,v)/clipw;if(!finite(clip)||clip.z<0||clip.z>1)return {};return Vec{(clip.x+1)*w*.5f,(1-clip.y)*h*.5f,clip.z};
}
std::optional<EditorRay> editor_screen_ray(float x,float y,float w,float h,Vec eye,Vec forward,float fov){
 if(!finite(eye)||!finite(forward)||dot(forward,forward)<=1e-14f||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||!std::isfinite(fov)||fov<=0||fov>=3.14159265f)return {};
 Vec f=unit(forward),r=unit(cross({0,1,0},f),{1,0,0}),u=cross(f,r);float t=std::tan(fov*.5f);
 Vec d=unit(f+r*((2*x/w-1)*t*w/h)+u*((1-2*y/h)*t),f);if(!finite(d))return {};return EditorRay{eye,d};
}
std::optional<float> ray_prop_obb(const EditorRay&r,const PropDefinition&p,const TrackPlacement&a){
 if(!valid_ray(r)||!finite(p.center)||!finite(p.half)||p.half.x<0||p.half.y<0||p.half.z<0)return {};
 auto m=placement_matrix(p,a);Vec origin,dir;if(!inverse_basis(m,r.origin-point(m,{}),origin)||!inverse_basis(m,r.direction,dir))return {};origin=origin-p.center;
 double lo=0,hi=std::numeric_limits<double>::infinity();
 for(int i=0;i<3;i++){if(dir[i]==0){if(origin[i]<-p.half[i]||origin[i]>p.half[i])return {};continue;}double first=(-double(p.half[i])-origin[i])/dir[i],last=(double(p.half[i])-origin[i])/dir[i];if(first>last)std::swap(first,last);lo=std::max(lo,first);hi=std::min(hi,last);if(lo>hi)return {};}
 if(!std::isfinite(lo)||lo>std::numeric_limits<float>::max())return {};
 return float(lo);
}
std::optional<Vec> ray_plane(const EditorRay&r,Vec origin,Vec normal){
 if(!valid_ray(r)||!finite(origin)||!finite(normal))return {};
 float den=dot(r.direction,normal);float scale=length(r.direction)*length(normal);if(scale==0||std::abs(den)<=1e-6f*scale)return {};float t=dot(origin-r.origin,normal)/den;if(!std::isfinite(t)||t<0)return {};Vec result=r.origin+r.direction*t;return finite(result)?std::optional<Vec>(result):std::nullopt;
}
std::optional<float> ray_axis_parameter(const EditorRay&r,Vec origin,Vec axis){
 if(!valid_ray(r)||!finite(origin)||!finite(axis))return {};
 Vec offset=r.origin-origin;double aa=dot(axis,axis),dd=dot(r.direction,r.direction),ad=dot(axis,r.direction),den=aa*dd-ad*ad;if(aa==0||den<=aa*dd*1e-8)return {};
 double t=(dd*dot(axis,offset)-ad*dot(r.direction,offset))/den;double rayt=(ad*t-dot(r.direction,offset))/dd;if(rayt<0||!std::isfinite(t)||std::abs(t)>std::numeric_limits<float>::max())return {};return float(t);
}
bool hit_axis_segment(Vec mouse,Vec a,Vec b,float pixels){
 if(!finite(mouse)||!finite(a)||!finite(b)||!std::isfinite(pixels)||pixels<0)return false;
 float dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;float t=den>0?std::clamp(((mouse.x-a.x)*dx+(mouse.y-a.y)*dy)/den,0.f,1.f):0;float x=mouse.x-a.x-t*dx,y=mouse.y-a.y-t*dy;return x*x+y*y<=pixels*pixels;
}
float snap_editor_value(float value,float step){if(!std::isfinite(value)||!std::isfinite(step)||step<=0)return value;double snapped=std::round(double(value)/step)*step;return std::isfinite(snapped)&&std::abs(snapped)<=std::numeric_limits<float>::max()?float(snapped):value;}
}
