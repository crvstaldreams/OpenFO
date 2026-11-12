#include "assets.hpp"
#include <fstream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
void put(const fs::path&p,const char*s){fs::create_directories(p.parent_path());std::ofstream(p)<<s;}
int main(){auto t=fs::temp_directory_path()/"openfo_overrides067";fs::remove_all(t);
 put(t/"native/cars/shared/common.dds","original");put(t/"native/menu/car_logos.dds","logos");put(t/"native/cars/car_1/body.bgm","geometry");
 Resources r(t/"native");put(t/"patch/cars/shared/common.dds","remastered");put(t/"patch/menu/car_logos.dds","new logos");put(t/"patch/cars/car_1/body.bgm","must not override");put(t/"patch/unknown.dds","must not add");
 need(r.overlay_textures(t/"patch")==2,"two existing textures overridden");need(r.text("data/cars/shared/common.dds")=="remastered","canonical common path resolves new asset");need(r.text("menu/car_logos.dds")=="new logos","atlas resolves new asset");need(r.text("cars/car_1/body.bgm")=="geometry","geometry untouched");need(!r.exists("unknown.dds"),"unknown files not injected");need(r.overlay_textures(t/"missing")==0,"missing override directory tolerated");
 put(t/"balks/menu/balks.bed","native BED");put(t/"balks/menu/balks.dds","native atlas");put(t/"balks/scripts/main.bed","must not inject");need(r.overlay_textures(t/"balks")==2,"restored native UI atlas and metadata register together");need(r.text("menu/balks.bed")=="native BED"&&r.text("menu/balks.dds")=="native atlas","restored balk resources resolve even if omitted from external data");need(!r.exists("scripts/main.bed"),"generic scripts remain excluded");put(t/"duplicate/cars/shared/COMMON.dds","upper");put(t/"duplicate/cars/shared/common.dds","lower");bool rejected=false;try{r.overlay_textures(t/"duplicate");}catch(const std::exception&){rejected=true;}need(rejected,"ambiguous case names rejected");need(r.text("cars/shared/common.dds")=="remastered","failed overlay is atomic");fs::remove_all(t);
}
