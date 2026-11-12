#include "content.hpp"
#include <iostream>
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.cpp"
using namespace ofo;
int main(int argc,char**argv){try{Resources r(fs::u8path(argv[1]));int count=0;for(auto&p:r.paths()){if(p.rfind("music/",0)!=0||fs::path(p).extension()!=".ogg")continue;auto b=r.read(p);int error=0;auto*decoder=stb_vorbis_open_memory(b.data(),int(b.size()),&error,nullptr);if(!decoder)throw std::runtime_error(p+" Vorbis open");short block[8192];auto info=stb_vorbis_get_info(decoder);int samples=stb_vorbis_get_samples_short_interleaved(decoder,2,block,8192);stb_vorbis_close(decoder);if(samples<=0||info.sample_rate<8000)throw std::runtime_error(p+" Vorbis decode");++count;}if(count!=28)throw std::runtime_error("music file count");std::cout<<count<<" original OGG files opened and decoded\n";return 0;}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
