#include "lan_voice_win.hpp"
#include <algorithm>
#include <map>

namespace ofo {
namespace {
uint8_t encode_mulaw(int16_t sample){
 const int sign=sample<0?0x80:0;
 const int value=std::min(32635,std::abs(int(sample)))+132;
 int exponent=7;
 for(int bit=0x4000;exponent>0&&!(value&bit);bit>>=1)--exponent;
 return uint8_t(~(sign|(exponent<<4)|((value>>(exponent+3))&15)));
}
int decode_mulaw(uint8_t sample){
 const int code=uint8_t(~sample);
 const int value=(((code&15)<<3)+132)<<((code>>4)&7);
 return code&0x80?132-value:value-132;
}
}
WAVEFORMATEX LanVoice::format(){return {WAVE_FORMAT_PCM,1,8000,16000,2,16,0};}
bool LanVoice::start_capture(){
 auto f=format();
 if(waveInOpen(&input_,WAVE_MAPPER,&f,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){input_=nullptr;return false;}
 for(auto& b:capture_){
  b.header={};b.header.lpData=reinterpret_cast<char*>(b.pcm.data());
  b.header.dwBufferLength=DWORD(b.pcm.size()*sizeof(int16_t));
  if(waveInPrepareHeader(input_,&b.header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR||
     waveInAddBuffer(input_,&b.header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR){stop_capture();return false;}
 }
 if(waveInStart(input_)!=MMSYSERR_NOERROR){stop_capture();return false;}
 return true;
}
void LanVoice::stop_capture(){
 if(!input_)return;
 waveInReset(input_);
 for(auto& b:capture_)if(b.header.dwFlags&WHDR_PREPARED)waveInUnprepareHeader(input_,&b.header,sizeof(WAVEHDR));
 waveInClose(input_);input_=nullptr;
}
LanVoice::~LanVoice(){
 stop_capture();
 if(!output_)return;
 waveOutReset(output_);
 for(auto& b:playback_)if(b.header.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(output_,&b.header,sizeof(WAVEHDR));
 waveOutClose(output_);
}
void LanVoice::play(const std::vector<LanVoicePacket>& packets){
 if(packets.empty())return;
 if(!output_){
  auto f=format();
  if(waveOutOpen(&output_,WAVE_MAPPER,&f,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){output_=nullptr;return;}
 }
 std::map<int,size_t> offsets;
 std::array<std::array<int,lan_voice_samples>,5> mixed{};
 size_t count=0;
 for(const auto& packet:packets){
  const auto index=offsets[packet.sender]++;
  if(index>=mixed.size())continue;
  count=std::max(count,index+1);
  for(size_t i=0;i<lan_voice_samples;++i)mixed[index][i]+=decode_mulaw(packet.samples[i]);
 }
 for(size_t index=0;index<count;++index){
  auto it=std::find_if(playback_.begin(),playback_.end(),[](const auto& b){return !(b.header.dwFlags&WHDR_INQUEUE);});
  if(it==playback_.end())break;
  auto& b=*it;
  if(!(b.header.dwFlags&WHDR_PREPARED)){
   b.header={};b.header.lpData=reinterpret_cast<char*>(b.pcm.data());
   b.header.dwBufferLength=DWORD(b.pcm.size()*sizeof(int16_t));
   if(waveOutPrepareHeader(output_,&b.header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR)break;
  }
  for(size_t i=0;i<lan_voice_samples;++i)b.pcm[i]=int16_t(std::clamp(mixed[index][i],-32768,32767));
  waveOutWrite(output_,&b.header,sizeof(WAVEHDR));
 }
}
void LanVoice::update(LanSession* session,bool transmit){
 const bool active=session&&(session->phase()==LanSession::Phase::Lobby||session->phase()==LanSession::Phase::Playing);
 if(!active||!transmit){stop_capture();attempted_=false;}
 else if(!attempted_){attempted_=true;start_capture();}
 if(!active)return;
 if(input_){
  for(auto& b:capture_)if(b.header.dwFlags&WHDR_DONE){
   if(b.header.dwBytesRecorded==b.header.dwBufferLength){
    LanVoiceFrame samples{};
    for(size_t i=0;i<samples.size();++i)samples[i]=encode_mulaw(b.pcm[i]);
    session->send_voice(samples);
   }
   b.header.dwBytesRecorded=0;
   waveInAddBuffer(input_,&b.header,sizeof(WAVEHDR));
  }
 }
 play(session->take_voice());
}
}
