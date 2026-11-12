#include "recovery_flow.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using ofo::RecoveryFlow;
static void check(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}
int main(){
 try{
  // All 6 stunt types use the same recovery policy: no automatic return after ejection.
  for(int stunt=0;stunt<6;++stunt){
   RecoveryFlow flow;
   for(int i=0;i<2500;++i){
    check(!flow.step(.01,true,false,false),"stunt ejection caused automatic teleport");
    check(!flow.active()&&flow.remaining()==0,"stunt ejection started recovery countdown");
   }
   flow.reset();
   check(!flow.step(.01,true,false,false),"new stunt attempt returned immediately");
   check(!flow.active(),"new stunt attempt started countdown");
  }
  // Ordinary races retain the original 3-second return behaviour.
  RecoveryFlow race;
  check(!race.step(.01,true,false,true),"race returned before countdown");
  check(race.active(),"race ejection must start recovery countdown");
  int fired=0;
  for(int i=0;i<310;++i)fired+=race.step(.01,true,false,true)?1:0;
  check(fired==1&&!race.active(),"race ejection must return exactly once");
  // Automatic recoveries are disabled in stunts, but explicit requests remain functional.
  RecoveryFlow explicit_flow;
  explicit_flow.request();
  fired=0;
  for(int i=0;i<310;++i)fired+=explicit_flow.step(.01,false,false,false)?1:0;
  check(fired==1,"explicit manual recovery must remain functional");
  // A completed race cancels its countdown cleanly.
  RecoveryFlow finished;
  finished.step(.01,true,false,true);
  check(!finished.step(.01,true,true,true)&&!finished.active(),"finished race cancels recovery");
  std::cout<<"PASS: six stunt ejections remain on scene; race and explicit recovery still work\n";
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
