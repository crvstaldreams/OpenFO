#pragma once
#include "../frontend.hpp"
#include "race_ui.hpp"
#include <algorithm>
#include <cmath>
namespace ofo {
// menutemplate.bed uses a 640-wide frame for a 512-wide BED slice. Keep
// that authored mapping, with equal menu scale on every display. Extra aspect
// width repeats whole frames; each end is cropped, never horizontally stretched.
inline void append_menu_balk(std::vector<FrontendDraw>& out,const MenuArtwork& art,
                             int width,int height,float source_y) {
 if(!art||width<=0||height<=0)return;
 MenuViewport view(width,height);const float scale=float(view.width/640.);
 const float tile_width=640*scale,top=float(view.map_y(source_y));
 const float bottom=top+art.height*scale;
 const float clipped_top=std::max(0.f,top),clipped_bottom=std::min(float(height),bottom);
 if(clipped_bottom<=clipped_top)return;
 // Preserve the centered 4:3 frame's UV phase. No wrap sampler is needed and
 // no UV can enter adjacent rows of the atlas, including partial end tiles.
 float x=float(view.x)+std::floor(-float(view.x)/tile_width)*tile_width;
 for(;x<float(width);x+=tile_width){
  float left=std::max(0.f,x),right=std::min(float(width),x+tile_width);
  if(right<=left)continue;
  FrontendDraw d;d.kind=FrontendDraw::Image;d.value=art.texture;d.color=0xffffffff;
  d.x=left;d.y=clipped_top;d.w=right-left;d.h=clipped_bottom-clipped_top;
  d.u=(art.x+(left-x)/tile_width*art.width)/art.atlas_width;
  d.v=(art.y+(clipped_top-top)/scale)/art.atlas_height;
  d.uw=(right-left)/tile_width*art.width/art.atlas_width;
  d.vh=(clipped_bottom-clipped_top)/scale/art.atlas_height;
  out.push_back(d);
 }
}
}
