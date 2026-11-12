#include "minimap_composite.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ofo {namespace {
uint32_t read(const Bytes&b,size_t p,int n=4){if(p>b.size()||size_t(n)>b.size()-p)throw std::runtime_error("Truncated minimap image");uint32_t v=0;for(int i=0;i<n;i++)v|=uint32_t(b[p+i])<<(8*i);return v;}
uint32_t rgb(int r,int g,int b,int a=255){return uint32_t(a)<<24|uint32_t(r)<<16|uint32_t(g)<<8|uint32_t(b);}
int ch(uint32_t c,int shift){return int((c>>shift)&255);}
uint32_t blend(uint32_t a,uint32_t b,float f){f=std::clamp(f,0.f,1.f);return rgb(int(ch(a,16)*(1-f)+ch(b,16)*f+.5f),int(ch(a,8)*(1-f)+ch(b,8)*f+.5f),int(ch(a,0)*(1-f)+ch(b,0)*f+.5f),int(ch(a,24)*(1-f)+ch(b,24)*f+.5f));}
uint32_t color565(uint32_t c){int r=(c>>11)&31,g=(c>>5)&63,b=c&31;return rgb((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2));}
MinimapImage image(int w,int h){if(w<=0||h<=0||w>8192||h>8192||uint64_t(w)*h>16777216)throw std::runtime_error("Invalid minimap image dimensions");return {w,h,std::vector<uint32_t>(size_t(w)*h)};}
uint32_t sample(const MinimapImage&i,float u,float v){u=std::clamp(u,0.f,1.f)*i.width-.5f;v=std::clamp(v,0.f,1.f)*i.height-.5f;int x=int(std::floor(u)),y=int(std::floor(v));auto at=[&](int a,int b){return i.pixels[size_t(std::clamp(b,0,i.height-1))*i.width+std::clamp(a,0,i.width-1)];};return blend(blend(at(x,y),at(x+1,y),u-x),blend(at(x,y+1),at(x+1,y+1),u-x),v-y);}
int unpack(uint32_t v,uint32_t mask,int fallback=0){if(!mask)return fallback;int shift=0;while(!(mask&1)){mask>>=1;++shift;}return int((uint64_t((v>>shift)&mask)*255+mask/2)/mask);}
MinimapImage dds(const Bytes&b){if(read(b,4)!=124||(read(b,76)!=32&&read(b,76)!=24))throw std::runtime_error("Invalid DDS header");auto out=image(int(read(b,16)),int(read(b,12)));auto code=read(b,84),flags=read(b,80);size_t p=128;
 if(flags&4){bool d1=code==0x31545844,d3=code==0x33545844,d5=code==0x35545844;if(!d1&&!d3&&!d5)throw std::runtime_error("Unsupported minimap DDS compression");int stride=d1?8:16;for(int by=0;by<out.height;by+=4)for(int bx=0;bx<out.width;bx+=4){read(b,p+stride-1,1);size_t cp=p+(d1?0:8);auto c0=read(b,cp,2),c1=read(b,cp+2,2);uint32_t colors[4]={color565(c0),color565(c1),0,0};colors[2]=blend(colors[0],colors[1],d1&&c0<=c1?.5f:1.f/3);colors[3]=d1&&c0<=c1?0:blend(colors[0],colors[1],2.f/3);uint32_t indices=read(b,cp+4);int alphas[8]={255,255,255,255,255,255,255,255};uint64_t ai=0;if(d5){alphas[0]=read(b,p,1);alphas[1]=read(b,p+1,1);if(alphas[0]>alphas[1])for(int k=2;k<8;k++)alphas[k]=((8-k)*alphas[0]+(k-1)*alphas[1])/7;else{for(int k=2;k<6;k++)alphas[k]=((6-k)*alphas[0]+(k-1)*alphas[1])/5;alphas[6]=0;alphas[7]=255;}for(int k=0;k<6;k++)ai|=uint64_t(read(b,p+2+k,1))<<(8*k);}
 for(int k=0;k<16;k++){int x=bx+k%4,y=by+k/4;if(x>=out.width||y>=out.height)continue;auto c=colors[(indices>>(2*k))&3];int a=d3?int((read(b,p+k/2,1)>>(4*(k%2)))&15)*17:d5?alphas[(ai>>(3*k))&7]:ch(c,24);out.pixels[size_t(y)*out.width+x]=(c&0xffffff)|(uint32_t(a)<<24);}p+=stride;}
 }else{auto bits=read(b,88);if(!(flags&0x40)||(bits!=16&&bits!=24&&bits!=32))throw std::runtime_error("Unsupported minimap DDS pixels");int bytes=int(bits/8);size_t row=size_t(out.width)*bytes;if(read(b,8)&8)row=std::max(row,size_t(read(b,20)));if(row>size_t(8192*4))throw std::runtime_error("Invalid DDS pitch");for(int y=0;y<out.height;y++)for(int x=0;x<out.width;x++){auto c=read(b,p+size_t(y)*row+size_t(x)*bytes,bytes);out.pixels[size_t(y)*out.width+x]=rgb(unpack(c,read(b,92)),unpack(c,read(b,96)),unpack(c,read(b,100)),unpack(c,read(b,104),255));}}
 return out;}
MinimapImage tga(const Bytes&b){read(b,17,1);int type=b[2],bits=b[16],bytes=bits/8;if(b[1]||(type!=2&&type!=3&&type!=10&&type!=11)||(bits!=8&&bits!=24&&bits!=32)||((type==2||type==10)&&bits==8))throw std::runtime_error("Unsupported minimap TGA");auto out=image(int(read(b,12,2)),int(read(b,14,2)));size_t p=18+b[0],cursor=0;auto pixel=[&](){uint32_t c=read(b,p,bytes);p+=bytes;return bits==8?rgb(c,c,c):bits==24?c|0xff000000:c;};while(cursor<out.pixels.size()){size_t count=1;bool repeated=false;if(type>=10){auto header=read(b,p++,1);count=(header&127)+1;repeated=(header&128)!=0;}if(count>out.pixels.size()-cursor)throw std::runtime_error("Invalid TGA run");uint32_t c=repeated?pixel():0;for(size_t k=0;k<count;k++){if(!repeated)c=pixel();int x=int(cursor%out.width),y=int(cursor/out.width);if(!(b[17]&32))y=out.height-1-y;if(b[17]&16)x=out.width-1-x;out.pixels[size_t(y)*out.width+x]=c;++cursor;}}return out;}
void feather(MinimapImage&out){
 for(int y=0;y<out.height;y++)for(int x=0;x<out.width;x++){
 auto&i=out.pixels[size_t(y)*out.width+x];int edge=std::min({x,y,out.width-1-x,out.height-1-y});
 float t=std::clamp(edge/(std::min(out.width,out.height)*.12f),0.f,1.f);float a=t*t*(3-2*t);
 i=(i&0xffffff)|(uint32_t(std::lround(ch(i,24)*a))<<24);
 }
}
float metric(const Json&j,const char*key,const char*axis){float v=j.at(key).at(axis).get<float>();if(!std::isfinite(v))throw std::runtime_error("Invalid minimap world bounds");return v;}
}
MinimapImage decode_minimap_image(const Bytes&b){return b.size()>=4&&read(b,0)==0x20534444?dds(b):tga(b);}
ComposedMinimap build_minimap(const Resources&r,const Geometry&g,const std::string&stage,const Json&config,int size){
 if(size<16||size>2048)throw std::runtime_error("Invalid minimap output size");
 float left=metric(config,"WorldTopLeft","1"),top=metric(config,"WorldTopLeft","2"),right=metric(config,"WorldBottomRight","1"),bottom=metric(config,"WorldBottomRight","2");if(std::abs(right-left)<1e-5f||std::abs(bottom-top)<1e-5f)throw std::runtime_error("Empty minimap world bounds");
 ComposedMinimap out;out.width=out.height=size;out.pixels.assign(size_t(size)*size,0);out.diagnostics={{"mode","schematic_fallback"},{"rasterized_triangles",0},{"covered_pixels",0},{"schematic_pixels",0},{"lightmap_uv","UV0 projected through world triangles"}};
 MinimapImage schematic,atlas;try{auto path=r.texture(g.source,config.at("MapTexture").get<std::string>());schematic=decode_minimap_image(r.read(path));out.diagnostics["schematic"]=path;}catch(const std::exception&e){out.diagnostics["schematic_error"]=e.what();}
 auto fallback=[&](){if(!schematic.pixels.empty())for(int y=0;y<size;y++)for(int x=0;x<size;x++)out.pixels[size_t(y)*size+x]=sample(schematic,(x+.5f)/size,(y+.5f)/size);};
 auto light=stage+"/lighting/lightmap1_w2.dds";out.diagnostics["lightmap"]=light;try{atlas=decode_minimap_image(r.read(light));}catch(const std::exception&e){out.diagnostics["lightmap_error"]=e.what();fallback();feather(out);return out;}
 std::vector<float>depth(out.pixels.size(),-std::numeric_limits<float>::infinity());size_t triangles=0,covered=0;
 for(const auto&m:g.meshes){if(m.captured||m.billboard||!m.solid||m.prop_index>=0||m.material>=g.materials.size())continue;const auto&material=g.materials[m.material];if(material.alpha||lower(fs::path(clean_path(material.texture[0])).filename().string())!="colormap.tga")continue;
 for(size_t k=0;k+2<m.indices.size();k+=3){uint32_t ix[3]={m.indices[k],m.indices[k+1],m.indices[k+2]};if(ix[0]>=m.vertices.size()||ix[1]>=m.vertices.size()||ix[2]>=m.vertices.size())continue;const Vertex*vs[3]={&m.vertices[ix[0]],&m.vertices[ix[1]],&m.vertices[ix[2]]};Vec w[3]={point(m.transform,vs[0]->position),point(m.transform,vs[1]->position),point(m.transform,vs[2]->position)};auto normal=cross(w[1]-w[0],w[2]-w[0]);if(std::abs(normal.y)<length(normal)*.25f)continue;
 float x[3],y[3];bool finite=true;for(int i=0;i<3;i++){x[i]=(w[i].x-left)/(right-left)*size;y[i]=(w[i].z-top)/(bottom-top)*size;finite&=std::isfinite(x[i])&&std::isfinite(y[i])&&std::isfinite(w[i].y)&&std::isfinite(vs[i]->u)&&std::isfinite(vs[i]->v);}if(!finite)continue;
 auto edge=[](float ax,float ay,float bx,float by,float px,float py){return (bx-ax)*(py-ay)-(by-ay)*(px-ax);};float area=edge(x[0],y[0],x[1],y[1],x[2],y[2]);if(std::abs(area)<1e-7f)continue;
 float minx=std::min({x[0],x[1],x[2]}),maxx=std::max({x[0],x[1],x[2]}),miny=std::min({y[0],y[1],y[2]}),maxy=std::max({y[0],y[1],y[2]});if(maxx<0||maxy<0||minx>size||miny>size)continue;int x0=int(std::clamp(std::floor(minx),0.f,float(size-1))),x1=int(std::clamp(std::ceil(maxx),0.f,float(size-1))),y0=int(std::clamp(std::floor(miny),0.f,float(size-1))),y1=int(std::clamp(std::ceil(maxy),0.f,float(size-1)));bool drawn=false;
 for(int py=y0;py<=y1;py++)for(int px=x0;px<=x1;px++){float a=edge(x[1],y[1],x[2],y[2],px+.5f,py+.5f)/area,b=edge(x[2],y[2],x[0],y[0],px+.5f,py+.5f)/area,c=1-a-b;if(std::min({a,b,c})<-.00001f)continue;float h=a*w[0].y+b*w[1].y+c*w[2].y;size_t dst=size_t(py)*size+px;if(h<depth[dst])continue;if(!std::isfinite(depth[dst]))++covered;depth[dst]=h;uint32_t color=sample(atlas,a*vs[0]->u+b*vs[1]->u+c*vs[2]->u,a*vs[0]->v+b*vs[1]->v+c*vs[2]->v)|0xff000000;
 // Slight blue-black wash lets the pale route remain readable without hiding baked roads.
 out.pixels[dst]=blend(color,0xff17242a,.16f);drawn=true;}
 if(drawn)++triangles;
 }}
 out.diagnostics["rasterized_triangles"]=triangles;out.diagnostics["covered_pixels"]=covered;if(!covered){fallback();out.diagnostics["lightmap_error"]="No lightmapped terrain intersects map bounds";feather(out);return out;}
 // Filter only the terrain layer. The schematic keeps its full route contrast.
 // The original peripheral terrain uses separate UV islands, so a map-edge fade
 // also keeps those authored atlas seams from dominating the small HUD panel.
 int step=std::max(1,size/256);auto filtered=out.pixels;
 for(int axis=0;axis<2;axis++){for(int y=0;y<size;y++)for(int x=0;x<size;x++){int xa=std::clamp(x-(axis==0?step:0),0,size-1),xb=std::clamp(x+(axis==0?step:0),0,size-1),ya=std::clamp(y-(axis==1?step:0),0,size-1),yb=std::clamp(y+(axis==1?step:0),0,size-1);auto a=out.pixels[size_t(ya)*size+xa],b=out.pixels[size_t(y)*size+x],c=out.pixels[size_t(yb)*size+xb];filtered[size_t(y)*size+x]=rgb((ch(a,16)+2*ch(b,16)+ch(c,16)+2)/4,(ch(a,8)+2*ch(b,8)+ch(c,8)+2)/4,(ch(a,0)+2*ch(b,0)+ch(c,0)+2)/4,(ch(a,24)+2*ch(b,24)+ch(c,24)+2)/4);}out.pixels.swap(filtered);}
 for(size_t i=0;i<out.pixels.size();i++)if(!std::isfinite(depth[i]))out.pixels[i]=0;
 out.diagnostics["terrain_filter"]="gentle low-pass; transparent smoothstep edge fade; no backing";
 out.diagnostics["mode"]="projected_lightmap_with_schematic";
 if(!schematic.pixels.empty()){std::vector<float>route(out.pixels.size());for(int y=0;y<size;y++)for(int x=0;x<size;x++){auto c=sample(schematic,(x+.5f)/size,(y+.5f)/size);float lum=(ch(c,16)*.2126f+ch(c,8)*.7152f+ch(c,0)*.0722f);route[size_t(y)*size+x]=std::clamp((lum-165)/65,0.f,1.f)*(ch(c,24)/255.f);}
 int radius=std::max(1,size/256);size_t route_count=0;for(int y=0;y<size;y++)for(int x=0;x<size;x++){float halo=0;for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++){if(dx*dx+dy*dy>radius*radius)continue;int xx=x+dx,yy=y+dy;if(xx>=0&&yy>=0&&xx<size&&yy<size)halo=std::max(halo,route[size_t(yy)*size+xx]);}size_t i=size_t(y)*size+x;out.pixels[i]=blend(out.pixels[i],0xff101b22,halo*.8f);out.pixels[i]=blend(out.pixels[i],0xfff3f0d8,route[i]*.93f);route_count+=route[i]>.2f;}out.diagnostics["schematic_pixels"]=route_count;}
 feather(out);out.diagnostics["edge_alpha"]="smoothstep 12 percent; zero at image boundary";
 return out;
}
}
