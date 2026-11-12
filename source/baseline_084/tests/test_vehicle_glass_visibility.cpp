#include "vehicle_glass_visibility.hpp"
#include <stdexcept>
static void need(bool b){if(!b)throw std::runtime_error("cockpit glass submission regression");}
int main(){
 using ofo::draw_player_glass;
 need(!draw_player_glass(2,false,false,false));
 need(draw_player_glass(0,false,false,false));
 need(draw_player_glass(1,false,false,false));
 need(draw_player_glass(2,true,false,false));
 need(draw_player_glass(2,false,true,false));
 need(draw_player_glass(2,false,false,true));
}
