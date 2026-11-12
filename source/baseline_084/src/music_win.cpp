#define NOMINMAX
#include "music_win.hpp"
#include "content.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <random>
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.cpp"
namespace ofo {
struct Music::Impl {
 const Resources&r;std::vector<MusicEntry> menu,playlist;std::thread thread;std::atomic<bool>stop{false},active{false};mutable std::mutex mutex;bool race=false,enabled=false;float volume=.45f;unsigned skip=0;std::string playing,error,playing_title,playing_artist;ULONGLONG track_began=0;std::mt19937 random{unsigned(GetTickCount())};
 explicit Impl(const Resources&res):r(res){try{menu=music_catalog(r,true);playlist=music_catalog(r,false);}catch(const std::exception&e){error=e.what();}thread=std::thread([this]{run();});}
 ~Impl(){stop=true;if(thread.joinable())thread.join();}
 void run(){HWAVEOUT output=nullptr;stb_vorbis*decoder=nullptr;Bytes encoded;struct Buffer{short pcm[8192]{};WAVEHDR hdr{};};std::array<Buffer,4>buffers;bool context=false,known=false,eof=false,failed=false;unsigned generation=0;size_t previous=size_t(-1);auto close=[&]{active=false;if(output){waveOutReset(output);for(auto&b:buffers)if(b.hdr.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(output,&b.hdr,sizeof b.hdr);waveOutClose(output);output=nullptr;}if(decoder){stb_vorbis_close(decoder);decoder=nullptr;}encoded.clear();for(auto&b:buffers)b.hdr={};};
 while(!stop){bool desired,on;float gain;unsigned next;{std::lock_guard<std::mutex>lock(mutex);desired=race;on=enabled;gain=volume;next=skip;}
 if(!on){if(output)waveOutPause(output);Sleep(10);continue;}
 bool pending=false;for(auto&b:buffers)pending=pending||bool(b.hdr.dwFlags&WHDR_INQUEUE);
 if(!known||desired!=context||next!=generation||(eof&&!pending)){close();bool changed=!known||desired!=context;context=desired;generation=next;known=true;eof=false;failed=false;auto&tracks=context?playlist:menu;try{if(tracks.empty())throw std::runtime_error("Music playlist has no available tracks");size_t index=0;if(context){index=random()%tracks.size();if(!changed&&tracks.size()>1&&index==previous)index=(index+1)%tracks.size();}previous=index;encoded=r.read(tracks[index].path);if(encoded.size()>size_t(INT_MAX))throw std::runtime_error("OGG exceeds decoder limit");int code=0;decoder=stb_vorbis_open_memory(encoded.data(),int(encoded.size()),&code,nullptr);if(!decoder)throw std::runtime_error("Vorbis open failed: "+std::to_string(code));auto info=stb_vorbis_get_info(decoder);if(info.sample_rate<8000||info.sample_rate>192000||info.channels<1||info.channels>2)throw std::runtime_error("Unsupported music channel/rate");WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=info.sample_rate;fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=fmt.nSamplesPerSec*4;if(waveOutOpen(&output,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)throw std::runtime_error("Music waveOutOpen failed");for(auto&b:buffers){b.hdr.lpData=reinterpret_cast<LPSTR>(b.pcm);b.hdr.dwBufferLength=sizeof b.pcm;if(waveOutPrepareHeader(output,&b.hdr,sizeof b.hdr)!=MMSYSERR_NOERROR)throw std::runtime_error("Music prepare failed");}{std::lock_guard<std::mutex>lock(mutex);playing=tracks[index].artist+" - "+tracks[index].title;playing_title=tracks[index].title;playing_artist=tracks[index].artist;track_began=GetTickCount64();error.clear();}active=true;}catch(const std::exception&e){close();failed=true;std::lock_guard<std::mutex>lock(mutex);error=e.what();}}
 if(failed||!decoder||!output){Sleep(20);continue;}waveOutRestart(output);if(!eof)for(auto&b:buffers){if(b.hdr.dwFlags&WHDR_INQUEUE)continue;int n=stb_vorbis_get_samples_short_interleaved(decoder,2,b.pcm,8192);if(n<=0){eof=true;break;}for(int i=0;i<n*2;i++)b.pcm[i]=short(b.pcm[i]*gain);b.hdr.dwBufferLength=DWORD(n*4);if(waveOutWrite(output,&b.hdr,sizeof b.hdr)!=MMSYSERR_NOERROR){failed=true;active=false;std::lock_guard<std::mutex>lock(mutex);error="Music output write failed";break;}}Sleep(2);
 }close();}
};
Music::Music(const Resources&r):p_(std::make_unique<Impl>(r)){}Music::~Music()=default;void Music::update(bool race,float volume,bool enabled,bool next){std::lock_guard<std::mutex>lock(p_->mutex);p_->race=race;p_->volume=std::clamp(volume,0.f,1.f);p_->enabled=enabled;if(next)++p_->skip;}Json Music::diagnostics()const{std::lock_guard<std::mutex>lock(p_->mutex);return{{"active",bool(p_->active)},{"track",p_->playing},{"title",p_->playing_title},{"artist",p_->playing_artist},{"track_age",p_->track_began?double(GetTickCount64()-p_->track_began)*.001:-1.},{"error",p_->error},{"menu_tracks",p_->menu.size()},{"race_tracks",p_->playlist.size()},{"backend","bounded OGG decoder -> waveOut stereo buffers"}};}
}
