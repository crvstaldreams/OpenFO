#include "race_loading.hpp"
#include "startup_flow.hpp"
#include <cassert>
#include <cmath>
using namespace ofo;

int main(){
 auto cropped=loading_cover_uv(1920,1080,640,480);
 assert(cropped.u==0&&cropped.w==1);
 assert(std::abs(cropped.h-.75f)<1e-6f&&std::abs(cropped.v-.125f)<1e-6f);
 auto wide=loading_cover_uv(640,480,1920,1080);
 assert(wide.v==0&&wide.h==1&&std::abs(wide.w-.75f)<1e-6f);
 assert(loading_cover_uv(0,0,0,0).w==1);
 assert(loading_progress(150)==1&&loading_progress(42)==.42f);
 assert(!loading_race_has_laps(race_mode("tracks/derby/derby1/a","DERBY")));
 assert(loading_race_has_laps(race_mode("tracks/arena/arena1/a","RACE")));
 assert(!loading_race_has_laps(RaceMode::Stunt)&&!loading_race_has_laps(RaceMode::FreeRoam));

 PresentedAudioScene audio;
 assert(!audio.ready(AudioScene::Menu));
 audio.presented(AudioScene::Menu);
 assert(audio.ready(AudioScene::Menu)&&!audio.ready(AudioScene::Race));
 audio.loading();
 assert(!audio.ready(AudioScene::Menu)&&!audio.ready(AudioScene::Race));
 audio.presented(AudioScene::Race);
 assert(audio.ready(AudioScene::Race)&&!audio.ready(AudioScene::Silent));
}
