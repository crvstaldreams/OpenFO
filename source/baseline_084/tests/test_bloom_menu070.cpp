#include "frontend.hpp"
#include "legacy_ui/race_ui.hpp"
#include "menu_key_repeat.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{using namespace ofo;need(argc>1,"data");Resources r(argv[1]);auto tmp=fs::temp_directory_path()/"openfo_bloom_menu070";fs::remove_all(tmp);Frontend m(r,tmp);need(m.create_profile("BLOOM TEST"),"profile");m.page=MenuPage::GameOptions;m.row=5;
 need(m.bloom_intensity==1,"default bloom");m.key(39);need(std::abs(m.bloom_intensity-1.1f)<.001,"keyboard increases bloom");m.key(37);need(std::abs(m.bloom_intensity-1)<.001,"keyboard decreases bloom");
 for(int i=0;i<30;i++)m.key(37);need(m.bloom_intensity==0,"zero disables bloom");for(int i=0;i<30;i++)m.key(39);need(m.bloom_intensity==2,"maximum clamps");
 MenuViewport vp(1920,1080);m.click(int(vp.x+468*vp.width/640),int(vp.map_y(294)),1920,1080);need(std::abs(m.bloom_intensity-1)<.01,"bloom slider mouse coordinate matches drawn row");
 Frontend saved(r,tmp);need(std::abs(saved.bloom_intensity-1)<.01,"per-profile intensity persists");
 m.page=MenuPage::GameOptions;m.key(34);need(m.page==MenuPage::Options,"next graphics->audio");m.key(34);need(m.page==MenuPage::Controls,"next audio->controls");m.key(34);need(m.page==MenuPage::GameOptions,"next controls->graphics");m.key(33);need(m.page==MenuPage::Controls,"previous tab");m.key(9);need(m.page==MenuPage::GameOptions,"Tab cycles consistently");
 m.options_return=MenuPage::Pause;m.key(27);need(m.page==MenuPage::Pause,"back returns parent");
 MenuKeyRepeat repeat;need(repeat.update(true,0),"initial stick/dpad pulse");need(!repeat.update(true,.2),"repeat delay");need(repeat.update(true,.45),"held repeat");need(!repeat.update(true,.46),"bounded repeat");repeat.update(false,.47);need(repeat.update(true,.48),"release rearms");
 fs::remove_all(tmp);std::cout<<"Bloom slider/key/clamp/persistence and settings tabs/repeat passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
