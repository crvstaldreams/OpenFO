 static constexpr size_t surface_budget=16*1024*1024,surface_entry_cap=96;
 bool use_track_surface(const Geometry&g,const GPU&m)const{
  if(m.car||m.water||m.glass||m.cutout||m.shader==tree_billboard_shader||g.source.find("tracks/menu/")!=std::string::npos)return false;
  auto mode=native_material_profile(m.shader).mode;
  return mode==1||mode==2||mode==8||mode==10||mode==11;
 }
 TrackSurfaceImage decode_track_surface(const std::string&path)const{
  try{
   auto bytes=r.read(path);
   // Standard block DDS can be decoded directly at its first <=256px mip;
   // this avoids allocating a large CPU image just to resample detail.
   if(bytes.size()>=128&&u32(bytes,0)==0x20534444){
    auto fourcc=u32(bytes,84);int w=int(u32(bytes,16)),h=int(u32(bytes,12));
    if(w<1||h<1||w>8192||h>8192)return {};
    if(fourcc==D3DFMT_DXT1||fourcc==D3DFMT_DXT3||fourcc==D3DFMT_DXT5){
     size_t offset=128;
     auto span=[&]{return size_t(std::max(1,(w+3)/4))*std::max(1,(h+3)/4)*(fourcc==D3DFMT_DXT1?8:16);};
     for(unsigned level=1;(w>256||h>256)&&level<std::min(16u,u32(bytes,28));++level){
      offset+=span();w=std::max(1,w/2);h=std::max(1,h/2);
     }
     if(offset>bytes.size()||span()>bytes.size()-offset)return {};
     if(offset>128){
      auto put=[&](int at,unsigned value){for(int k=0;k<4;++k)bytes[at+k]=uint8_t(value>>(k*8));};
      put(16,w);put(12,h);put(28,1);std::memmove(bytes.data()+128,bytes.data()+offset,span());bytes.resize(128+span());
     }
    }
   }
   auto image=decode_minimap_image(bytes);return {image.width,image.height,std::move(image.pixels)};
  }catch(const std::exception&){return {};}
 }
 Texture*track_surface_texture(const std::string&path,const std::string&material_name){
  if(path.empty()||!r.exists(path))return nullptr;
  auto profile=track_surface_profile(path,material_name);
  std::string key=clean_path(path)+"|"+std::to_string(int(profile.specular*100+.5f))+"|"+
      std::to_string(int(profile.roughness*100+.5f))+"|"+std::to_string(int(profile.bump*100+.5f));
  auto found=normal_maps.find(key);if(found!=normal_maps.end())return found->second.get();
  if(normal_maps.size()>=surface_entry_cap||normal_map_bytes>=surface_budget||
     surface_attempted.size()>=surface_entry_cap*4||!surface_attempted.insert(key).second)return nullptr;
  try{
   auto decode=[&](const std::string&file){return decode_track_surface(file);};
   auto base=decode(path);
   auto image=build_track_surface_map(r,path,material_name,base,decode);
   if(image.width<1||image.height<1||image.width>256||image.height>256||image.argb.size()!=size_t(image.width)*image.height)return nullptr;
   int levels=1;size_t bytes=image.argb.size()*4;
   for(int w=image.width,h=image.height;w>1||h>1;){
    w=std::max(1,w/2);h=std::max(1,h/2);bytes+=size_t(w)*h*4;++levels;
   }
   if(bytes>surface_budget-normal_map_bytes)return nullptr;
   const int width=image.width,height=image.height;
   auto stage=std::make_unique<Texture>(),gpu=std::make_unique<Texture>();
   hr(d->CreateTexture(image.width,image.height,levels,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&stage->p,nullptr),"Track surface staging");
   for(int level=0;level<levels;++level){
    int w=image.width,h=image.height;D3DLOCKED_RECT lock{};
    hr(stage->p->LockRect(level,&lock,nullptr,0),"Track surface lock");
    for(int y=0;y<h;++y)std::memcpy(static_cast<uint8_t*>(lock.pBits)+size_t(y)*lock.Pitch,image.argb.data()+size_t(y)*w,size_t(w)*4);
    hr(stage->p->UnlockRect(level),"Track surface unlock");
    if(level+1<levels){
     int nw=std::max(1,w/2),nh=std::max(1,h/2);std::vector<uint32_t>mip(size_t(nw)*nh);
     for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){
      unsigned sums[4]={};unsigned count=0;
      for(int j=y*h/nh;j<(y+1)*h/nh;++j)for(int i=x*w/nw;i<(x+1)*w/nw;++i){
       auto p=image.argb[size_t(j)*w+i];for(int c=0;c<4;++c)sums[c]+=(p>>(c*8))&255;++count;
      }
      for(int c=0;c<4;++c)mip[size_t(y)*nw+x]|=((sums[c]+count/2)/count)<<(c*8);
     }
     image.width=nw;image.height=nh;image.argb=std::move(mip);
    }
   }
   hr(d->CreateTexture(width,height,levels,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&gpu->p,nullptr),"Track surface texture");
   hr(d->UpdateTexture(stage->p,gpu->p),"Track surface upload");
   gpu->w=width;gpu->h=height;
   auto*ptr=gpu.get();normal_maps.emplace(std::move(key),std::move(gpu));normal_map_bytes+=bytes;return ptr;
  }catch(const std::exception&e){trace("track_surface_unavailable",path+" : "+e.what());return nullptr;}
 }
