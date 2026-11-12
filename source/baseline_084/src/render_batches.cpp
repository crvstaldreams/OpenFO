#include "render_batches.hpp"
#include "bounded_cpu_jobs.hpp"
#include "water.hpp"
#include <tuple>
#include <limits>
#include <set>
#include <stdexcept>
namespace ofo {
namespace {
bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool rigid(const Mat&m){for(float n:m)if(!std::isfinite(n))return false;if(m[3]!=0||m[7]!=0||m[11]!=0||m[15]!=1)return false;Vec a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[8],m[9],m[10]};constexpr float eps=2e-4f;return std::abs(dot(a,a)-1)<eps&&std::abs(dot(b,b)-1)<eps&&std::abs(dot(c,c)-1)<eps&&std::abs(dot(a,b))<eps&&std::abs(dot(a,c))<eps&&std::abs(dot(b,c))<eps;}
// Explicit allowlist: prelit/lightmapped/tree and EMU diffuse programs. The
// object-space specular offset (shader 2), animated water, and unsupported
// effects retain their original world matrix and draw order.
bool safe_shader(uint32_t id){switch(id){case 0:case 1:case 3:case 4:case 7:case 8:case 9:case 15:case 16:case 18:case 19:case 20:case 21:return true;default:return false;}}
struct Bounds {Vec lo{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},hi{-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};void add(Vec p,float radius=0){for(int i=0;i<3;i++){lo[i]=std::min(lo[i],p[i]-radius);hi[i]=std::max(hi[i],p[i]+radius);}}Vec center()const{return lo*.5f+hi*.5f;}};
// Validate and locate a source without creating a temporary geometry copy.
// Cofactors are constant for every vertex in one mesh.
struct BakeTransform {
 const Mesh& mesh;Vec a,b,c;float det;
 explicit BakeTransform(const Mesh&m):mesh(m){Vec x{m.transform[0],m.transform[1],m.transform[2]},y{m.transform[4],m.transform[5],m.transform[6]},z{m.transform[8],m.transform[9],m.transform[10]};a=cross(y,z);b=cross(z,x);c=cross(x,y);det=dot(x,a);}
 Vertex vertex(Vertex v)const{if(!mesh.billboard){v.position=point(mesh.transform,v.position);v.normal=(a*v.normal.x+b*v.normal.y+c*v.normal.z)/det;}return v;}
};
bool bake_bounds(const Mesh&m,Bounds&bounds){if(m.billboard&&m.transform!=identity())return false;BakeTransform transform(m);for(const auto&source:m.vertices){if(!finite(source.position)||!finite(source.normal)||!std::isfinite(source.u)||!std::isfinite(source.v)||!std::isfinite(source.lu)||!std::isfinite(source.lv))return false;auto v=transform.vertex(source);if(!finite(v.position)||!finite(v.normal))return false;if(m.billboard){float r=std::hypot(v.normal.x,v.normal.y);auto c=v.position-v.normal;if(!finite(c)||!std::isfinite(r))return false;bounds.add(c,r);}else bounds.add(v.position);}return finite(bounds.lo)&&finite(bounds.hi)&&finite(bounds.center());}
struct Candidate {std::vector<size_t>members;size_t vertices=0,indices=0;};
using Key=std::tuple<int64_t,int64_t,int64_t,uint32_t,uint32_t,std::string,std::string,std::string,bool>;
}
RenderBatchPlan build_render_batches(const Geometry&g,float cell_size,size_t max_vertices,size_t worker_limit){
 if(!std::isfinite(cell_size)||cell_size<=0||max_vertices<3||max_vertices>std::numeric_limits<uint32_t>::max())throw std::invalid_argument("Invalid render batch cell size/vertex budget");
 RenderBatchPlan plan;std::map<Key,size_t> active;std::vector<Candidate> candidates;std::map<std::string,size_t> skipped;size_t original_vertices=0,original_indices=0;std::set<std::tuple<int64_t,int64_t,int64_t>> cells;
 for(size_t index=0;index<g.meshes.size();++index){const auto&m=g.meshes[index];original_vertices+=m.vertices.size();original_indices+=m.indices.size();auto skip=[&](const char*why){plan.unbatched.push_back(index);++skipped[why];};
  if(m.material>=g.materials.size()){skip("material");continue;}const auto&mat=g.materials[m.material];if(water_material(mat)){skip("water");continue;}if(!safe_shader(mat.shader)){skip("shader_object_space_or_unsupported");continue;}if(m.vertices.empty()||m.indices.empty()||m.indices.size()%3){skip("empty_or_nontriangle");continue;}if(m.vertices.size()>max_vertices){skip("oversized");continue;}if(!rigid(m.transform)){skip("nonrigid_or_nonfinite_transform");continue;}if(!finite(m.center)||!std::isfinite(m.radius)||m.radius<0){skip("invalid_bounds");continue;}bool valid=true;for(auto i:m.indices)if(i>=m.vertices.size()){valid=false;break;}if(!valid){skip("index");continue;}
  Bounds bounds;if(!bake_bounds(m,bounds)){skip("unsafe_attributes_or_billboard_transform");continue;}auto center=bounds.center();std::array<int64_t,3> cell{};for(int axis=0;axis<3;axis++){double coord=std::floor(double(center[axis])/cell_size);if(coord<-9e15||coord>9e15){valid=false;break;}cell[axis]=int64_t(coord);}if(!valid){skip("cell_range");continue;}
  Key key{cell[0],cell[1],cell[2],mat.shader,mat.alpha,lower(clean_path(mat.texture[0])),lower(clean_path(mat.texture[1])),lower(clean_path(mat.texture[2])),m.billboard};cells.emplace(cell[0],cell[1],cell[2]);auto at=active.find(key);if(at==active.end()||candidates[at->second].vertices>max_vertices-m.vertices.size()){candidates.emplace_back();active[key]=candidates.size()-1;at=active.find(key);}auto&b=candidates[at->second];b.members.push_back(index);b.vertices+=m.vertices.size();b.indices+=m.indices.size();
 }
 size_t merged_members=0,batch_vertices=0,batch_indices=0;
 // Singletons keep their original buffers. Only merged groups allocate geometry,
 // after their final sizes are known, so no growing buffers or bake copies coexist.
 std::vector<size_t> merged;
 for(size_t candidate=0;candidate<candidates.size();++candidate){if(candidates[candidate].members.size()<2){plan.unbatched.push_back(candidates[candidate].members.front());++skipped["singleton"];}else merged.push_back(candidate);}
 plan.batches.resize(merged.size());
 bounded_cpu_for(merged.size(),[&](size_t slot){size_t candidate=merged[slot];auto&pending=candidates[candidate];RenderBatch b;b.members=std::move(pending.members);const auto&first=g.meshes[b.members.front()];b.mesh.material=first.material;b.mesh.name="render_batch_"+std::to_string(candidate);b.mesh.billboard=first.billboard;b.mesh.solid=false;b.mesh.vertices.reserve(pending.vertices);b.mesh.indices.reserve(pending.indices);
  for(auto index:b.members){const auto&m=g.meshes[index];BakeTransform transform(m);auto offset=uint32_t(b.mesh.vertices.size());for(const auto&v:m.vertices)b.mesh.vertices.push_back(transform.vertex(v));for(auto i:m.indices)b.mesh.indices.push_back(offset+i);if(m.prop_index>=0&&std::find(b.props.begin(),b.props.end(),m.prop_index)==b.props.end())b.props.push_back(m.prop_index);b.mesh.captured=b.mesh.captured||m.captured;}
  Bounds bounds;for(auto i:b.members)bounds.add(g.meshes[i].center,g.meshes[i].radius);for(auto&v:b.mesh.vertices){if(b.mesh.billboard)bounds.add(v.position-v.normal,std::hypot(v.normal.x,v.normal.y));else bounds.add(v.position);}b.mesh.center=bounds.center();double radius=0;for(auto i:b.members)radius=std::max(radius,double(length(g.meshes[i].center-b.mesh.center))+g.meshes[i].radius);for(auto&v:b.mesh.vertices){Vec p=b.mesh.billboard?v.position-v.normal:v.position;double dx=double(p.x)-b.mesh.center.x,dy=double(p.y)-b.mesh.center.y,dz=double(p.z)-b.mesh.center.z;double r=std::sqrt(dx*dx+dy*dy+dz*dz)+(b.mesh.billboard?std::hypot(v.normal.x,v.normal.y):0);radius=std::max(radius,r);}b.mesh.radius=std::nextafter(float(radius),std::numeric_limits<float>::infinity());plan.batches[slot]=std::move(b);},original_vertices<32768?1:worker_limit);
 for(const auto&b:plan.batches){merged_members+=b.members.size();batch_vertices+=b.mesh.vertices.size();batch_indices+=b.mesh.indices.size();}

 std::sort(plan.unbatched.begin(),plan.unbatched.end());size_t after_vertices=batch_vertices,after_indices=batch_indices;for(auto i:plan.unbatched){after_vertices+=g.meshes[i].vertices.size();after_indices+=g.meshes[i].indices.size();}
 plan.report={{"source_meshes",g.meshes.size()},{"batches",plan.batches.size()},{"batched_members",merged_members},{"unbatched",plan.unbatched.size()},{"planned_draws",plan.batches.size()+plan.unbatched.size()},{"draws_saved",merged_members-plan.batches.size()},{"source_vertices",original_vertices},{"planned_vertices",after_vertices},{"source_triangles",original_indices/3},{"planned_triangles",after_indices/3},{"spatial_cells",cells.size()},{"cell_size",cell_size},{"max_vertices",max_vertices},{"unbatched_reasons",skipped},{"lossless",original_vertices==after_vertices&&original_indices==after_indices},{"physics_modified",false}};return plan;
}
RenderFrustum::RenderFrustum(const Mat&vp,Vec eye,float far):eye_(eye),far_(far){for(int i=0;i<4;i++){planes_[0][i]=vp[i*4+3]+vp[i*4];planes_[1][i]=vp[i*4+3]-vp[i*4];planes_[2][i]=vp[i*4+3]+vp[i*4+1];planes_[3][i]=vp[i*4+3]-vp[i*4+1];planes_[4][i]=vp[i*4+2];planes_[5][i]=vp[i*4+3]-vp[i*4+2];}for(auto&p:planes_){float len=std::hypot(p[0],std::hypot(p[1],p[2]));if(!std::isfinite(len)||len<1e-12f){p={0,0,0,0};continue;}for(auto&v:p)v/=len;}}
bool RenderFrustum::visible(Vec center,float radius)const{if(!finite(center)||!std::isfinite(radius)||radius<0)return true;if(finite(eye_)&&std::isfinite(far_)&&far_>0){double dx=double(center.x)-eye_.x,dy=double(center.y)-eye_.y,dz=double(center.z)-eye_.z;double range=double(far_)+radius;if(dx*dx+dy*dy+dz*dz>range*range)return false;}for(auto&p:planes_)if(p[0]*center.x+p[1]*center.y+p[2]*center.z+p[3]<-radius)return false;return true;}
void RenderSpatialIndex::build(std::vector<RenderSphere> spheres){
 spheres_=std::move(spheres);
 order_.clear();unbounded_.clear();nodes_.clear();
 for(size_t i=0;i<spheres_.size();++i){
  const auto s=spheres_[i];
  auto& list=finite(s.center)&&std::isfinite(s.radius)&&s.radius>=0?order_:unbounded_;
  list.push_back(i);
 }
 if(!order_.empty())build_node(0,order_.size());
}
size_t RenderSpatialIndex::build_node(size_t begin,size_t end){
 const auto index=nodes_.size();
 nodes_.push_back({{},begin,end});
 if(end-begin<=8)return index;
 Bounds bounds;
 for(size_t i=begin;i<end;++i)bounds.add(spheres_[order_[i]].center);
 const auto center=bounds.center();
 double radius=0;
 for(size_t i=begin;i<end;++i){
  const auto s=spheres_[order_[i]];
  const auto d=std::hypot(double(s.center.x)-center.x,double(s.center.y)-center.y,double(s.center.z)-center.z);
  radius=std::max(radius,d+s.radius);
 }
 // Absorb normalization/rounding at a parent plane boundary. Extreme bounds
 // overflow to infinity, causing traversal to fail open like a leaf.
 nodes_[index].sphere={center,float(radius*1.00001+.001)};
 const auto extent=bounds.hi-bounds.lo;
 int axis=extent.x>=extent.y?0:1;
 if(extent.z>extent[axis])axis=2;
 const size_t middle=begin+(end-begin)/2;
 std::nth_element(order_.begin()+begin,order_.begin()+middle,order_.begin()+end,[&](size_t a,size_t b){
  const float x=spheres_[a].center[axis],y=spheres_[b].center[axis];
  return x==y?a<b:x<y;
 });
 build_node(begin,middle);
 const auto right=build_node(middle,end);
 nodes_[index].right=right;
 return index;
}
}
