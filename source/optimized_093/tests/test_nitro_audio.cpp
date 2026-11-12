#include "nitro_audio.hpp"
#include <stdexcept>
#include <iostream>
using namespace ofo;
void need(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 NitroAudioConfig cfg;cfg.shot_gain=.5f;cfg.loop_gain=.25f;cfg.delay=2.5;
 PcmWave shot{44100,{1,1,1}},loop{44100,{1,1}};
 NitroAudioMixer mix(cfg,shot,loop);
 mix.update(false,true,0,0);need(mix.frame()==0,"inactive is silent");
 mix.update(true,true,0,0);need(mix.frame()==.75f,"native shot and loop gains mixed");
 for(int i=0;i<10;++i)mix.update(true,true,.01*i,0);
 need(mix.diagnostics()["shots"]==1,"held activation never restarts onset");
 for(int i=0;i<10;++i)mix.frame();need(mix.frame()==.25f,"one shot ends while loop wraps");
 mix.update(true,false,.2,0);need(mix.frame()==0,"pause immediately releases all nitro voices");
 mix.update(false,true,.3,0);need(mix.frame()==0,"empty tank or invalid mode stays silent");
 mix.update(true,true,.4,0);need(mix.frame()==.25f&&mix.diagnostics()["shots"]==1,"native onset cooldown retained");
 mix.update(false,true,3,0);mix.update(true,true,3,0);need(mix.frame()==.75f&&mix.diagnostics()["shots"]==2,"fresh native onset after cooldown");
 mix.update(false,true,0,1);need(mix.frame()==0,"recovery resets voices");
 if(argc>1){Resources r(argv[1]);auto native=load_nitro_audio_config(r);need(native.shot=="sound/gearshift/nitro_on.wav"&&native.loop=="sound/gearshift/nitro_loop.wav","native script sample selection");need(native.delay==2.5&&native.shot_gain==1&&native.loop_gain==1,"native script mix parameters");NitroAudioMixer real(r,native);real.update(true,true,0,0);float energy=0;for(int i=0;i<44100*3;++i){auto v=real.frame();need(std::isfinite(v),"finite native resampling");energy+=v*v;}need(energy>1,"native loop remains audible");real.update(false,true,3,0);need(real.frame()==0,"native release silent");}
 std::cout<<"nitro audio tests passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
