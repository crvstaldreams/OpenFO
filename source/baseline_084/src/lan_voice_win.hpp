#pragma once
#include "lan_session.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

namespace ofo {
class LanVoice {
 struct Buffer {
  WAVEHDR header{};
  std::array<int16_t,lan_voice_samples> pcm{};
 };
 HWAVEIN input_=nullptr;
 HWAVEOUT output_=nullptr;
 std::array<Buffer,5> capture_{};
 std::array<Buffer,6> playback_{};
 bool attempted_=false;
 static WAVEFORMATEX format();
 bool start_capture();
 void stop_capture();
 void play(const std::vector<LanVoicePacket>&);
public:
 LanVoice()=default;
 ~LanVoice();
 LanVoice(const LanVoice&)=delete;
 LanVoice& operator=(const LanVoice&)=delete;
 void update(LanSession*,bool transmit);
 bool transmitting()const{return input_!=nullptr;}
};
}
