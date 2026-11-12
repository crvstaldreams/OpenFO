#include "captured_scene.hpp"
#include "file_io.hpp"
#include <cassert>
#include <iostream>
using namespace ofo;
int main(){
 auto root=fs::temp_directory_path()/"openfo-native-props-test";fs::remove_all(root);fs::create_directories(root/"capture_assets");
 Json object={{"export_status","linked"},{"source_track","data/tracks/forest/forest1/geometry/track_geom.w32"},{"type_name","wood_box"},{"instance_name","box"},{"surface_indices",{7}},{"world_matrix",translate({10,0,0})},{"mass_native",30},{"restitution_native",.2},{"id",92},{"collision_center_local",{0,0,0}},{"collision_half_extents",{1,1,1}},{"inertia_diagonal_native",{20,20,20}}};
 write_file_atomic(root/"capture_assets/manifest.json",Json{{"complete",true},{"objects",Json::array({object})}}.dump());fs::create_directories(root/"global/dynamics");write_file_atomic(root/"global/dynamics/dynamic_objects.bed","gate_test = { Mass=40, Restitution=0.1, Hinge={AngleLimits={-28,44}, MaxForce=1000, BreakForce=10000}, Inertia=INERTIA_SOLID_BOX }\n pole_test = {Mass=30, Restitution=0.2, Joint={XAxisLimits={-9,9},ZAxisLimits={-4,4},BreakForce=2500}, Inertia=INERTIA_SOLID_BOX}\n");Resources r(root);
 Geometry g;g.track=true;g.source="tracks/desert/desert1/geometry/track_geom.w32";
 NativePropPlacement p;p.name="desert_box";p.type="wood_box";p.model_name="desert_native_model";p.model_flags=4;p.native_index=4;p.surfaces={7};p.half={2,1,1};p.transform=translate({400,3,50});g.native_props.push_back(p);
 Mesh m;m.name=p.name;m.source_surface=7;m.transform=p.transform;m.vertices.resize(3);m.indices={0,1,2};g.meshes.push_back(m);
 Mesh tree=m;tree.name="tree_0_0";g.meshes.push_back(tree);Mesh ground=m;ground.name="static_0";g.meshes.push_back(ground);
 auto report=append_captured_scene(g,r);assert(g.props.size()==1);assert(g.meshes.size()==3);assert(g.meshes[0].prop_index==0);assert(g.meshes[1].prop_index==-1&&g.meshes[2].prop_index==-1);assert(g.props[0].transform==p.transform);assert(g.props[0].half.x==2);assert(g.props[0].inertia.y==50);assert(report["native_type_instances"]==1);assert(report["exact_captured_instances"]==0);
 // Calling twice cannot create duplicate bodies or duplicate visuals.
 append_captured_scene(g,r);assert(g.props.size()==1&&g.meshes.size()==3);
 Geometry empty;empty.source=g.source;append_captured_scene(empty,r);assert(empty.props.empty()&&empty.meshes.empty());
 Geometry unknown;unknown.source=g.source;p.type="unknown_type";unknown.native_props.push_back(p);unknown.meshes.push_back(m);assert(append_captured_scene(unknown,r)["unmatched_dynamic_instances"]==1);assert(unknown.props.empty());
 Geometry gates;gates.source=g.source;for(int i=0;i<2;i++){auto q=p;q.type=i?"pole_test":"gate_test";q.name=i?"pole":"gate";q.native_index=i;gates.native_props.push_back(q);auto mesh=m;mesh.name=q.name;gates.meshes.push_back(mesh);}append_captured_scene(gates,r);assert(gates.props.size()==2);assert(gates.props[0].joint.kind==PropJointDefinition::Hinge);assert(std::abs(gates.props[0].joint.x_limits[0]+28*.0174532925f)<1e-5f);assert(gates.props[0].joint.break_force==10000);assert(gates.props[1].joint.kind==PropJointDefinition::Pole);assert(g.props[0].joint.kind==PropJointDefinition::None);
 fs::remove_all(root);std::cout<<"native prop track scoping passed\n";
}
