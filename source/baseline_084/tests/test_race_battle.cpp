#include "race_battle.hpp"
#include <iostream>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){State s;auto bots=std::make_shared<std::vector<OpponentState>>(2);s.opponents=bots;RaceBattle battle(3);battle.update(.01,s);need(!battle.finished()&&battle.survivors()==3,"all race entrants alive");(*bots)[1].damage=1;battle.update(.01,s);need(!battle.finished()&&battle.survivors()==2,"real damage eliminates opponent");(*bots)[0].damage=1;battle.update(.01,s);need(battle.finished()&&battle.standings(s)[0]["car"]==0,"last surviving player wins");RaceBattle lost(3);s.damage=1;(*bots)[0].damage=(*bots)[1].damage=0;lost.update(.01,s);auto rows=lost.standings(s);need(lost.finished()&&rows.back()["car"]==0&&rows.back()["rank"]==3,"player wrecked receives real elimination place");std::cout<<"physical last-man-standing finish and standings passed\n";}
