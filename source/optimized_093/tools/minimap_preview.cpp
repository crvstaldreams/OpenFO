// Portable inspection utility: OpenFOMinimapPreview DATA STAGE OUTPUT.bmp [SIZE]
#include "minimap_composite.hpp"
#include <fstream>
#include <iostream>
using namespace ofo;
int main(int argc,char**argv){try{if(argc<4){std::cerr<<"usage: minimap_preview DATA STAGE OUTPUT.bmp [SIZE]\n";return 2;}Resources r(argv[1]);std::string stage=argv[2];auto geometry=load_geometry(r,stage_geometry(stage),stage);auto map=build_minimap(r,geometry,stage,bed_file(r,stage+"/data/map.bed"),argc>4?std::stoi(argv[4]):512);std::ofstream f(argv[3],std::ios::binary);auto word=[&](uint32_t v,int bytes=4){for(int k=0;k<bytes;k++)f.put(char(v>>(8*k)));};word(0x4d42,2);word(54+uint32_t(map.pixels.size()*4));word(0);word(54);word(40);word(map.width);word(uint32_t(-map.height));word(1,2);word(32,2);word(0);word(uint32_t(map.pixels.size()*4));word(2835);word(2835);word(0);word(0);for(auto c:map.pixels)word(c);if(!f)throw std::runtime_error("Cannot write minimap preview");std::cout<<map.diagnostics.dump(2)<<'\n';return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
