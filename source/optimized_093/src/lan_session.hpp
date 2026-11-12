#pragma once
#include "math.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include "race_event.hpp"
#include "track_environment.hpp"
#include <memory>
#include <functional>
#include <optional>

namespace ofo {
inline constexpr uint16_t lan_default_port=27267;
inline constexpr int lan_max_players=8;
struct LanRules {
 RaceMode mode=RaceMode::Circuit;
 std::string stage,subevent;
 int laps=3;
 TrackEnvironment environment;
};
struct LanPlayer {
 int id=0,car_id=3,skin=1;
 std::string name;
 bool ready=false,loaded=false,connected=true;
};
inline constexpr size_t lan_voice_samples=160;
using LanVoiceFrame=std::array<uint8_t,lan_voice_samples>;
struct LanRoomInfo {
 std::string name,stage;
 uint16_t port=0;
 int players=0;
 bool open=false;
};
struct LanVoicePacket {int sender=0;LanVoiceFrame samples{};};
struct LanPose {
 Mat car=identity();
 std::array<Mat,4> wheels{{identity(),identity(),identity(),identity()}};
 Vec velocity{};
 float damage=0;
 uint64_t recovery_generation=0;
 double score=0;
 bool finished=false,ejected=false;
};

class LanSession {
 struct Impl;
 std::unique_ptr<Impl> p_;
public:
 enum class Phase { Closed, Connecting, Lobby, Loading, Playing, Failed };
 LanSession();
 ~LanSession();
 LanSession(const LanSession&)=delete;
 LanSession& operator=(const LanSession&)=delete;
 void host(uint16_t port,LanPlayer player,const LanRules&);
 void join(const std::string& address,LanPlayer player);
 void serve(uint16_t port,std::string room,const LanRules&);
 LanRoomInfo room_info()const;
 const std::string& room_name()const;
 void set_directory(std::function<std::vector<LanRoomInfo>()>);
 void set_room_creator(std::function<uint16_t(const std::string&)>);
 static uint16_t create_room(const std::string& address,const std::string& name);
 static std::vector<LanRoomInfo> list_rooms(const std::string& address);
 static std::string room_address(const std::string& address,uint16_t port);
 void close();
 void poll();
 Phase phase()const;
 bool is_host()const;
 int local_id()const;
 uint16_t port()const;
 const std::string& error()const;
 const LanRules& rules()const;
 const std::vector<LanPlayer>& players()const;
 bool can_start()const;
 bool set_rules(const LanRules&);
 void set_car(int id,int skin);
 void set_ready(bool);
 bool start();
 bool take_load_request();
 void loaded();
 void publish(const LanPose&);
 void send_voice(const LanVoiceFrame&);
 std::vector<LanVoicePacket> take_voice();
 std::optional<LanPose> peer_pose(int id,bool interpolated=true)const;
};

Mat lan_grid_spawn(Mat base,int slot,bool circuit);
}
