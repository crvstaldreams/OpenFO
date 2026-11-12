#pragma once
#include "assets.hpp"
namespace ofo {
// Packed A8R8G8B8, i.e. BGRA bytes on the supported little-endian platforms.
struct MinimapImage {int width=0,height=0;std::vector<uint32_t> pixels;};
struct ComposedMinimap : MinimapImage {Json diagnostics;};
MinimapImage decode_minimap_image(const Bytes&);
ComposedMinimap build_minimap(const Resources&,const Geometry&,const std::string& stage,const Json& map_config,int size=512);
}
