#include "assets.hpp"
#include <iostream>
#include <stdexcept>
// Native geometry must retain its authored physical normals. Raster front/back
// classification changes with the LH camera projection and is not normal data.
int main(int argc,char**argv){try{using namespace ofo;if(argc<2)throw std::runtime_error("native data directory required");Resources resources(argv[1]);auto g=load_geometry(resources,"tracks/forest/forest1/geometry/track_geom.w32");size_t aligned=0,opposed=0,up=0,clockwiseUp=0;for(const auto&m:g.meshes)for(size_t i=0;i+2<m.indices.size();i+=3){const auto&a=m.vertices[m.indices[i]],&b=m.vertices[m.indices[i+1]],&c=m.vertices[m.indices[i+2]];auto geometric=cross(b.position-a.position,c.position-a.position),authored=a.normal+b.normal+c.normal;float agreement=dot(geometric,authored);aligned+=agreement>1e-6f;opposed+=agreement<-1e-6f;if(authored.y>1&&std::abs(geometric.y)>length(geometric)*.8f){++up;clockwiseUp+=geometric.y<0;}}
 if(aligned<100000||aligned<opposed*4||up<10000||clockwiseUp*4>up)throw std::runtime_error("native receiver normal/winding convention changed");
 std::cout<<"Native Forest receiver normals: aligned="<<aligned<<" opposed="<<opposed<<" up="<<up<<" clockwise-up="<<clockwiseUp<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
