#include "assets.hpp"
#include <fstream>
#include <stdexcept>
int main(){using namespace ofo;auto t=fs::temp_directory_path()/"openfo_sky_assets070";fs::remove_all(t);fs::create_directories(t/"original");auto dest=t/"patch/tracks/shared/skybox/openfo_night";fs::create_directories(dest);std::ofstream(dest/"forest_forest1_a_ft.dds")<<"sky";std::ofstream(dest/"scripts.bed")<<"must not inject";Resources r(t/"original");auto n=r.overlay_textures(t/"patch");if(n!=1||!r.exists("tracks/shared/skybox/openfo_night/forest_forest1_a_ft.dds")||r.exists("tracks/shared/skybox/openfo_night/scripts.bed"))throw std::runtime_error("new generated night images must load without injecting scripts");fs::remove_all(t);}
