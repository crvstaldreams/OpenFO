#pragma once
#include "compat.hpp"
namespace ofo {
enum class FontStyle { Regular18,Regular20,Regular10,Condensed12,Italic14,Italic24,Italic36,Navigation };
struct FontGlyph {
 float u0=0,v0=0,u1=0,v1=0;
 int bearing=0,width=0,advance=0,reserved=0;
};
struct BitmapFont {
 float line_height=0,spacing_x=0,spacing_y=0,baseline=0;
 std::vector<uint16_t> character_map; // Indexed by codepoint-1; glyph IDs are zero-based; zero is missing except the space in Regular20.
 std::vector<FontGlyph> glyphs;
 std::string texture;
 const FontGlyph* glyph(uint32_t codepoint)const;
 bool ascii_atlas_valid()const;
};
BitmapFont parse_bitmap_font(const Bytes&);
BitmapFont load_bitmap_font(const Resources&,FontStyle);
std::vector<uint32_t> font_codepoints(const std::string&utf8);
float bitmap_text_width(const BitmapFont&,const std::string&utf8,float scale=1);
}
