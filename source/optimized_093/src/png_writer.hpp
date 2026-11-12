#pragma once
#include <cstdint>
#include <vector>
namespace ofo {
// Top-down packed A8R8G8B8 input. Lossless RGB8 output, independent of input alpha.
// Throws invalid_argument for empty/oversize dimensions or a mismatched pixel count.
std::vector<uint8_t> encode_png(uint32_t width,uint32_t height,const std::vector<uint32_t>& argb);
}
