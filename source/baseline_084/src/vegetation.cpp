#include "vegetation.hpp"

#include <cstring>
#include <stdexcept>
namespace ofo {
namespace {
struct Reader {const Bytes&b;size_t p=0;uint32_t u(){if(p>b.size()||b.size()-p<4)throw std::runtime_error("Truncated psdb vegetation");uint32_t v=0;for(int k=0;k<4;++k)v|=uint32_t(b[p++])<<(8*k);return v;}float f(){auto v=u();float x;std::memcpy(&x,&v,4);if(!std::isfinite(x)||std::abs(x)>1e7)throw std::runtime_error("Invalid plant metric");return x;}};
}
PlantWind plant_wind(double seconds){
 PlantWind out{};if(!std::isfinite(seconds))seconds=0;
 // Native clock is integral milliseconds masked with 4095.
 double ms=std::fmod(std::max(0.,seconds)*1000.,4096.);
 float t=float(std::floor(ms))*.0015339808305725455f;
 for(int k=0;k<16;++k){float phase=t+k*.39269909262657166f,tilt=std::sin(phase)*.27925267815589905f;
 out[k]={std::sin(tilt),std::cos(tilt),std::cos(phase)*.1f};}return out;
}
PlantQuad plant_quad(const PlantSprite&s,const PlantWind&wind){
 const auto&w=wind[s.wind_phase&15];const float dx=s.direction[0]*s.width,dz=s.direction[1]*s.width;
 const float shiftX=s.direction[1]*s.height*w[0]+dx*w[2],shiftZ=dz*w[2]-s.direction[0]*s.height*w[0],rise=s.height*w[1];
 PlantQuad q;auto p=s.position;
 q.position={{{p[0]+dx,p[1],p[2]+dz},{p[0]-dx,p[1],p[2]-dz},{p[0]-dx+shiftX,p[1]+rise,p[2]-dz+shiftZ},{p[0]+dx+shiftX,p[1]+rise,p[2]+dz+shiftZ}}};
 float u=(s.tile%4)*.25f,v=(s.tile/4)*.25f;
 q.uv={{{u+.25f,v+.25f},{u,v+.25f},{u,v},{u+.25f,v}}};return q;
}
float plant_opacity(float distance,float start,float end){if(!std::isfinite(distance)||end<=start)return 0;return std::clamp((end-distance)/(end-start),0.f,1.f);}
Vegetation parse_vegetation(const Bytes&b,const Bytes&colors){
 Reader r{b};if(r.u()!=0x62647370||r.u()!=1)throw std::runtime_error("Unsupported psdb vegetation header");
 std::array<float,16> sizes;for(auto&v:sizes){v=r.f();if(v<=0||v>100)throw std::runtime_error("Invalid plant dimensions");}
 float limits[3][2];for(auto&axis:limits){axis[0]=r.f();axis[1]=r.f();if(axis[1]<axis[0])throw std::runtime_error("Inverted plant bounds");}
 const auto n=r.u();if(n>2000000||n>(b.size()-r.p)/8)throw std::runtime_error("Invalid plant count");if(!colors.empty()&&colors.size()!=size_t(n)*4)throw std::runtime_error("Plant lighting count mismatch");
 Vegetation out;out.sprites.reserve(n);
 for(uint32_t i=0;i<n;++i){auto a=r.u(),c=r.u();PlantSprite s;s.position={limits[0][0]+float(a&4095)/4095*(limits[0][1]-limits[0][0]),limits[1][0]+float(c&65535)/65535*(limits[1][1]-limits[1][0]),limits[2][0]+float((a>>12)&4095)/4095*(limits[2][1]-limits[2][0])};s.tile=uint8_t((a>>24)&15);s.variation=uint16_t(c>>16);
 // Four rows of min/max dimension pairs; byte 7 interpolates each pair.
 const int type=s.tile/4;const float size=float(c>>24)*.003921568859368563f;
 s.width=sizes[type*2]+(sizes[8+type*2]-sizes[type*2])*size;
 s.height=sizes[type*2+1]+(sizes[9+type*2]-sizes[type*2+1])*size;
 s.wind_phase=uint8_t(a>>28);const float angle=float((c>>16)&255)*.02454369328916073f;s.direction={std::cos(angle),std::sin(angle)};
 if(!colors.empty())for(int k=0;k<3;++k)s.color[k]=colors[size_t(i)*4+2-k]/255.f;
 out.sprites.push_back(s);}
 const auto count=r.u();if(count>n||size_t(count)*8!=b.size()-r.p)throw std::runtime_error("Invalid plant cell table");std::vector<bool>seen(n,false);
 for(uint32_t i=0;i<count;++i){PlantCell cell;cell.count=r.u();cell.first=r.u();if(!cell.count||cell.first>n||cell.count>n-cell.first)throw std::runtime_error("Plant cell range outside source records");cell.bounds.lo.fill(INFINITY);cell.bounds.hi.fill(-INFINITY);
  for(size_t j=cell.first;j<cell.first+cell.count;++j){if(seen[j])throw std::runtime_error("Overlapping plant cells");seen[j]=true;const auto&s=out.sprites[j];for(int k=0;k<3;++k){cell.bounds.lo[k]=std::min(cell.bounds.lo[k],s.position[k]-(k==1?0:s.width*1.1f+s.height*.28f));cell.bounds.hi[k]=std::max(cell.bounds.hi[k],s.position[k]+(k==1?s.height:s.width*1.1f+s.height*.28f));}}out.cells.push_back(cell);}
 if(std::find(seen.begin(),seen.end(),false)!=seen.end())throw std::runtime_error("Unassigned plant records");return out;
}
Vegetation load_vegetation(const Resources&r,const std::string&track,const std::string&stage,int weather){
 Vegetation out;out.source=fs::path(track).parent_path().generic_string()+"/plant_geom.w32";if(!r.exists(out.source))return out;
 const auto source=out.source;auto lighting=stage+"/lighting/plantcolors_w"+std::to_string(weather)+".w32";
 try{Bytes colors;if(r.exists(lighting))colors=r.read(lighting);out=parse_vegetation(r.read(source),colors);out.source=source;out.color_source=colors.empty()?"":lighting;out.texture=r.texture(track,"plants.dds");
  try{auto settings=bed_file(r,"settings/plants.ini");out.far_clip=std::clamp(settings.value("FarClip",150.f),1.f,1000.f);out.fade_start=std::clamp(settings.value("FadeStart",100.f),0.f,out.far_clip-1.f);}catch(const std::exception&){}
 }catch(const std::exception&ex){out=Vegetation{};out.source=source;out.error=ex.what();}return out;
}
}
