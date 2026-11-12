#include "race_participants.hpp"
#include <cassert>
#include <iostream>
using namespace ofo;
int main(){
 State s;s.car=translate({0,0,-2});
 auto bots=std::make_shared<std::vector<OpponentState>>();
 OpponentState a;a.id=1;a.name="Native One";a.car_id=8;a.skin=-1;a.car=translate({0,0,2});a.speed=10;bots->push_back(a);
 s.opponents=bots;auto poses=participant_poses(s);assert(poses.size()==2&&poses[1].speed_kmh==36);
 Json raw=Json::array({Json{{"car",1},{"rank",1},{"lap",2},{"laps",4},{"best",32.5},{"finished",true},{"time",140.}},Json{{"car",0},{"rank",2},{"lap",2},{"laps",4},{"finished",true},{"time",145.}}});
 auto named=name_standings(raw,s);assert(named[0]["name"]=="Native One"&&named[0]["car"]==1&&named[0]["car_id"]==8);assert(named[1]["player"]==true);
 auto rows=race_overlay_rows(named);assert(rows.size()==2&&!rows[0].player&&rows[1].player&&rows[0].best==32.5);
 assert(race_player_rank(named)==2);assert(race_player_rank(Json::array())==1);
 std::cout<<"Shared opponent poses, named standings, player rank and HUD rows passed\n";
}
