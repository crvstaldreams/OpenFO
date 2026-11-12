#include "sky_environment.hpp"
#include "content.hpp"
#include <iostream>
#include <stdexcept>
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{using namespace ofo;
 auto c=sky_face_vertices(0);need(c[0].v==2&&c[2].v==0,"native side is an upper hemisphere: horizon at v1, not center of source");
 auto night=sky_face_vertices(0,500,true);need(night[0].v==1&&night[2].v==0,"generated full cube has horizon at v0.5");
 for(Vec eye:{Vec{0,0,0},Vec{900,40,-300},Vec{-120,80,700}}){auto view=look_at(eye,eye+Vec{.2f,.3f,1});auto centered=multiply(view,translate(eye));auto origin=look_at({},Vec{.2f,.3f,1});for(unsigned face=0;face<6;face++)for(auto&v:sky_face_vertices(face)){need(length(point(centered,v.position)-point(origin,v.position))<.01f,"sky follows camera translation without shifting horizon");}}
 for(unsigned f=0;f<6;f++){auto v=sky_face_vertices(f);for(auto&q:v)need(length(q.position)<900,"sky within 1800 far clip");}
 need(sky_side_v(0)==1&&sky_side_v(1)==0,"horizon and zenith edge mapping");
 if(argc>1){Resources r(argv[1]);if(argc>2)r.overlay_textures(fs::path(argv[2])/"data");size_t stages=0;for(auto&stage:stage_catalog(r)){auto s=load_sky_environment(r,stage);need(!s.day[0].empty()&&r.exists(s.day[0]),"native per-stage sky face resolved including dds.tga");if(argc>2)for(auto&p:s.night)need(r.exists(p),"every generated night face resolves through production overlay");need(s.stage==stage,"full race variant retained");need(s.night[0].find(sky_night_family(stage))!=std::string::npos,"night family resolved for complete stage");++stages;}auto menu=load_sky_environment(r,"tracks/menu/menu1/a");need(r.exists(menu.day[4]),"menu upper native TGA found");std::cout<<stages<<" stages + menu native sky profiles passed\n";}
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
