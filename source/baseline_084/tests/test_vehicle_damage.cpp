#include "vehicle_damage.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ofo;
namespace {
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void u(Bytes&b,uint32_t n){for(int i=0;i<4;i++)b.push_back(uint8_t(n>>(i*8)));}
void f(Bytes&b,float n){uint32_t bits;std::memcpy(&bits,&n,4);u(b,bits);}
void v(Bytes&b,Vec p){f(b,p.x);f(b,p.y);f(b,p.z);}
Geometry fixture(){Geometry g;g.source="cars/car_1/body.bgm";g.materials.push_back({"body"});Mesh m;m.name="body";m.source_surface=0;m.transform=translate({0,0,2});for(auto p:{Vec{0,0,0},Vec{1,0,0},Vec{0,1,0}}){Vertex q;q.position=p;q.normal={0,0,1};q.u=p.x;q.v=p.y;m.vertices.push_back(q);}m.indices={0,1,2};g.meshes.push_back(m);return g;}
Bytes crash(const Geometry&g){Bytes b;u(b,1);for(char c:std::string("body_crash"))b.push_back(c);b.push_back(0);u(b,1);u(b,3);u(b,96);for(auto&q:g.meshes[0].vertices){v(b,q.position);v(b,q.normal);f(b,q.u);f(b,q.v);}for(auto&q:g.meshes[0].vertices){v(b,q.position);v(b,q.position+Vec{0,0,-.5f});v(b,q.normal);v(b,{0,.3f,1});}return b;}
void rejects(const Bytes&b,const Geometry&g){bool failed=false;try{parse_vehicle_crash(b,g);}catch(const std::exception&){failed=true;}need(failed,"malformed crash accepted");}
}
int main(int argc,char**argv){try{
 auto g=fixture();auto bytes=crash(g);auto targets=parse_vehicle_crash(bytes,g);need(targets.records==1&&targets.vertices==3&&targets.moved==3,"authored target counts");need(targets.surfaces[0].positions[0].z==-.5f,"authored target decoded");
 for(size_t size:{size_t(0),size_t(3),bytes.size()/2,bytes.size()-1})rejects(Bytes(bytes.begin(),bytes.begin()+size),g);
 auto extra=bytes;extra.push_back(0);rejects(extra,g);auto mismatch=g;mismatch.meshes[0].vertices[0].u=.4f;rejects(bytes,mismatch);mismatch=g;mismatch.meshes[0].vertices[0].position.x=.2f;rejects(bytes,mismatch);mismatch=g;mismatch.meshes[0].indices[0]=500;rejects(bytes,mismatch);auto huge=bytes;for(int i=0;i<4;i++)huge[i]=255;rejects(huge,g);
 VehicleDamageContact c;c.position={0,0,2};c.radius=.6f;c.depth=.2f;c.impulse=24000;c.closing_speed=20;c.normal={0,0,-1};
 auto damaged=vehicle_damage_geometry(g,targets,1,{c});need(damaged.meshes[0].vertices[0].position.z==-.5f,"contact uses BGM instance transform once");need(damaged.meshes[0].vertices[1].position.z==0,"far vertex stays intact");need(damaged.meshes[0].vertices[0].u==0&&damaged.meshes[0].indices==g.meshes[0].indices,"UV/topology preserved");need(g.meshes[0].vertices[0].position.z==0,"source geometry immutable");
 auto none=vehicle_damage_geometry(g,{},1,{c});need(none.meshes[0].vertices[0].position.z==0,"no invented deformation without native targets");
 auto invalid=vehicle_damage_geometry(g,targets,std::numeric_limits<float>::quiet_NaN(),{c});need(invalid.meshes[0].vertices[0].position.z==0,"nonfinite damage safe");
 auto panelgeo=g;panelgeo.meshes[0].name="hood";panelgeo.meshes[0].transform=identity();panelgeo.markers["hood_joint"]=translate({0,0,1});panelgeo.markers["hood_attach_1"]=translate({5,0,0});auto child=panelgeo.meshes[0];child.name="hood_trim";panelgeo.meshes.push_back(child);
 Json panels={{"hood",{{"Mass",20},{"MaxAngle",65},{"UnlockMomentum",{{"1",10}}},{"ReleaseMomentum",30}}},{"hood_trim",{{"Mass",2},{"Parent","hood"}}}};
 auto defs=vehicle_panel_definitions(panelgeo,panels);need(defs.size()==1&&defs[0].meshes.size()==2&&defs[0].mass==22,"native parent panel grouping and mass");auto d=defs[0];need(d.hinge&&d.marker=="hood_joint"&&d.pivot.z==1,"joint marker beats fallback");auto opened=point(vehicle_panel_hinge(d,d.upper_limit>0?.2f:-.2f),d.center);need(opened.y>d.center.y,"native hinge opens outward");
 VehiclePanelDamage s;VehicleDamageContact hit=c;hit.position=d.center;hit.closing_speed=40;hit.impulse=30000;hit_vehicle_panel(s,d,hit,1200,true);need(s.damage>0&&s.unlocked&&!s.detached,"damaging hit unlocks authored hinge first");auto momentum=s.attachment_momentum;hit_vehicle_panel(s,d,hit,1200,true);need(s.attachment_momentum==momentum,"single contact cooldown");tick_vehicle_panel(s,d,.04f);need(!s.detached,"hinge articulates before detachment");tick_vehicle_panel(s,d,.05f);need(s.detached,"released panel preserves detached state");VehiclePanelDamage rest;hit.closing_speed=.1f;hit.impulse=100;hit_vehicle_panel(rest,d,hit,1200,true);need(rest.damage==0&&!rest.unlocked,"resting solver load immunity");
 auto glassgeo=g;glassgeo.materials[0].name="window_front";auto glass=vehicle_glass_damage(glassgeo,{c},1000);need(glass[0]>.02f&&glass[1]==0,"localized oriented windshield damage");auto hole=vehicle_damage_geometry(glassgeo,targets,1,{c},nullptr,glass,true);need(hole.meshes[0].indices.empty(),"front glass removal");need(vehicle_window_pane("window_left")==2&&vehicle_window_pane("body")==-1,"native pane mapping");
 if(argc>1){Resources r(argv[1]);size_t all=0;for(int n=1;n<=16;n++){auto path="cars/car_"+std::to_string(n)+"/body.bgm";if(!r.exists(path))continue;auto geo=load_geometry(r,path);auto native=load_vehicle_crash(r,geo);need(native.records>10&&native.moved>1000,"native crash fragments/targets missing");auto pd=load_vehicle_panels(r,geo);need(pd.size()>4,"native panel definitions missing");if(n==3){auto it=std::find_if(pd.begin(),pd.end(),[](const auto& part){return part.name=="exhaust_l";});need(it!=pd.end()&&it->has_joint_limits&&it->joint_stiffness.x==2&&it->joint_rest.x<0,"retail exhaust keeps the native spring, rest angle and attachment");}for(auto&t:native.surfaces){auto&mesh=geo.meshes[t.mesh];need(mesh.vertices.size()==t.positions.size(),"native topology pairing");}all+=native.moved;std::cout<<n<<" crash records="<<native.records<<" surfaces="<<native.surfaces.size()<<" moved="<<native.moved<<" panels="<<pd.size()<<'\n';}need(all>0,"actual data test had no cars");}
 std::cout<<"vehicle damage authored topology, local contacts, attachments/hinges, glass passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
