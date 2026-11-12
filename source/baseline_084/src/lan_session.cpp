#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "lan_session.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <map>
#include <stdexcept>

namespace ofo {
namespace {
using Clock=std::chrono::steady_clock;
using Time=Clock::time_point;
constexpr int wire_version=3;
constexpr size_t max_packet=16384,max_queue=max_packet*8;
#ifdef _WIN32
using Socket=SOCKET;
constexpr auto invalid_socket=INVALID_SOCKET;
int socket_error(){return WSAGetLastError();}
bool would_block(int e){return e==WSAEWOULDBLOCK||e==WSAEINPROGRESS;}
void close_socket(Socket socket){closesocket(socket);}
void nonblocking(Socket socket){
 u_long enabled=1;
 if(ioctlsocket(socket,FIONBIO,&enabled)!=0)throw std::runtime_error("LAN socket setup failed");
}
#else
using Socket=int;
constexpr Socket invalid_socket=-1;
int socket_error(){return errno;}
bool would_block(int e){return e==EAGAIN||e==EWOULDBLOCK||e==EINPROGRESS||e==EINTR;}
void close_socket(Socket socket){::close(socket);}
void nonblocking(Socket socket){
 const auto flags=fcntl(socket,F_GETFL,0);
 if(flags<0||fcntl(socket,F_SETFL,flags|O_NONBLOCK)<0)throw std::runtime_error("LAN socket setup failed");
}
#endif

struct SocketHandle {
 Socket fd=invalid_socket;
 SocketHandle()=default;
 explicit SocketHandle(Socket value):fd(value){}
 SocketHandle(SocketHandle&& other)noexcept:fd(other.fd){other.fd=invalid_socket;}
 SocketHandle& operator=(SocketHandle&& other)noexcept{
  if(this!=&other){reset();fd=other.fd;other.fd=invalid_socket;}
  return *this;
 }
 ~SocketHandle(){reset();}
 void reset(){if(fd!=invalid_socket)close_socket(fd);fd=invalid_socket;}
 explicit operator bool()const{return fd!=invalid_socket;}
};

struct SocketRuntime {
#ifdef _WIN32
 SocketRuntime(){WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Windows LAN startup failed");}
 ~SocketRuntime(){WSACleanup();}
#endif
};
SocketHandle stream_socket(){
 SocketHandle s(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));
 if(!s)throw std::runtime_error("Could not create LAN socket");
 nonblocking(s.fd);
 return s;
}
void stream_low_latency(Socket s){
 const int enabled=1;
 setsockopt(s,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&enabled),sizeof(enabled));
}
sockaddr_in endpoint(const std::string& address){
 const auto colon=address.find(':');
 const auto host=address.substr(0,colon);
 int port=lan_default_port;
 if(colon!=std::string::npos){
  const auto first=address.data()+colon+1,last=address.data()+address.size();
  const auto parsed=std::from_chars(first,last,port);
  if(parsed.ec!=std::errc{}||parsed.ptr!=last||port<1||port>65535)throw std::runtime_error("Enter an IPv4 address and valid port");
 }
 sockaddr_in out{};
 out.sin_family=AF_INET;out.sin_port=htons(uint16_t(port));
 if(host.empty()||host.size()>253)throw std::runtime_error("Enter the server hostname");
 if(inet_pton(AF_INET,host.c_str(),&out.sin_addr)!=1){
  addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
  addrinfo* result=nullptr;
  if(getaddrinfo(host.c_str(),nullptr,&hints,&result)!=0)throw std::runtime_error("Could not resolve server hostname");
  out.sin_addr=reinterpret_cast<const sockaddr_in*>(result->ai_addr)->sin_addr;
  freeaddrinfo(result);
 }
 return out;
}

struct Connection {
 SocketHandle socket;
 std::string input,output;
 int id=0;
 bool directory_done=false;
 Time heard=Clock::now();
 explicit Connection(SocketHandle handle):socket(std::move(handle)){}
 void queue(const Json& message){
  const auto bytes=message.dump(-1,' ',false,Json::error_handler_t::replace);
  if(bytes.size()>max_packet||output.size()+4+bytes.size()>max_queue)throw std::runtime_error("LAN peer cannot receive updates");
  const auto size=htonl(uint32_t(bytes.size()));
  output.append(reinterpret_cast<const char*>(&size),sizeof(size));
  output+=bytes;
 }
 void flush(){
  if(output.empty())return;
#ifdef _WIN32
  const auto sent=send(socket.fd,output.data(),int(output.size()),0);
#else
  const auto sent=send(socket.fd,output.data(),output.size(),MSG_NOSIGNAL);
#endif
  if(sent>0)output.erase(0,size_t(sent));
  else if(sent==0||!would_block(socket_error()))throw std::runtime_error("LAN connection closed");
 }
 template<class Handle>void receive(Handle handle){
  char buffer[8192];
  for(int read=0;read<16;++read){
   const auto n=recv(socket.fd,buffer,sizeof(buffer),0);
   if(n==0)throw std::runtime_error("LAN connection closed");
   if(n<0){if(would_block(socket_error()))break;throw std::runtime_error("LAN connection closed");}
   input.append(buffer,size_t(n));
   size_t used=0;
   while(input.size()-used>=4){
    uint32_t size=0;
    std::memcpy(&size,input.data()+used,4);
    size=ntohl(size);
    if(!size||size>max_packet)throw std::runtime_error("Invalid LAN message size");
    if(input.size()-used-4<size)break;
    const auto* payload=input.data()+used+4;
    auto json=Json::parse(payload,payload+size,nullptr,false);
    if(!json.is_object())throw std::runtime_error("Invalid LAN message");
    handle(json);
    heard=Clock::now();
    used+=4+size;
   }
   if(used)input.erase(0,used);
  }
 }
};
bool valid_car(int car,int skin){return car>=1&&car<=20&&((skin>=1&&skin<=16)||(skin>=101&&skin<=116));}
bool valid_rules(const LanRules& r){
 const auto mode=int(r.mode);
 if(mode<int(RaceMode::Circuit)||mode>int(RaceMode::FreeRoam)||
    r.stage.empty()||r.stage.size()>180||r.subevent.size()>50||r.laps<1||r.laps>9)return false;
 if(r.stage.rfind("tracks/",0)!=0&&r.stage.rfind("alpha_content/tracks/",0)!=0)return false;
 const auto valid=[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='/'||c=='_'||c=='-'||c=='.';};
 if(!std::all_of(r.stage.begin(),r.stage.end(),valid))return false;
 std::string_view stage=r.stage;
 while(!stage.empty()){
  const auto slash=stage.find('/');
  const auto component=stage.substr(0,slash);
  if(component.empty()||component=="."||component=="..")return false;
  if(slash==stage.npos)return true;
  stage.remove_prefix(slash+1);
 }
 return false;
}
Json wire_rules(const LanRules& r){
 return {{"mode",int(r.mode)},{"stage",r.stage},{"subevent",r.subevent},{"laps",r.laps},
         {"night",r.environment.night},{"rain",r.environment.rain},{"rain_strength",r.environment.rain_strength},
         {"sun_intensity",r.environment.sun_intensity}};
}
LanRules read_rules(const Json& j){
 LanRules r;
 r.mode=RaceMode(j.at("mode").get<int>());r.stage=j.at("stage").get<std::string>();
 r.subevent=j.at("subevent").get<std::string>();r.laps=j.at("laps").get<int>();
 r.environment.night=j.at("night").get<bool>();r.environment.rain=j.at("rain").get<bool>();
 r.environment.rain_strength=j.value("rain_strength",1.f);r.environment.sun_intensity=j.value("sun_intensity",1.f);
 r.environment.validate();
 if(!valid_rules(r))throw std::runtime_error("Invalid LAN race selection");
 return r;
}
Json wire_player(const LanPlayer& p){
 return {{"id",p.id},{"name",p.name},{"car",p.car_id},{"skin",p.skin},{"ready",p.ready},
         {"loaded",p.loaded},{"connected",p.connected}};
}
LanPlayer read_player(const Json& j){
 LanPlayer p;
 p.id=j.value("id",0);p.name=j.at("name").get<std::string>();p.car_id=j.at("car").get<int>();
 p.skin=j.at("skin").get<int>();p.ready=j.value("ready",false);p.loaded=j.value("loaded",false);p.connected=j.value("connected",true);
 if(p.name.empty()||p.name.size()>128||!valid_car(p.car_id,p.skin))throw std::runtime_error("Invalid LAN player");
 return p;
}
std::array<float,12> compact(const Mat& m){
 return {m[0],m[1],m[2],m[4],m[5],m[6],m[8],m[9],m[10],m[12],m[13],m[14]};
}
Mat expand(const Json& j){
 if(!j.is_array()||j.size()!=12)throw std::runtime_error("Invalid LAN transform");
 const auto x=j.get<std::array<float,12>>();
 for(size_t i=0;i<x.size();++i)
  if(!std::isfinite(x[i])||std::abs(x[i])>(i<9?1.05f:20000.f))throw std::runtime_error("Invalid LAN transform");
 Vec f{x[6],x[7],x[8]},u{x[3],x[4],x[5]};
 if(length(f)<.9f||length(u)<.9f||std::abs(dot(f,u))>.1f)throw std::runtime_error("Invalid LAN basis");
 return {x[0],x[1],x[2],0,x[3],x[4],x[5],0,x[6],x[7],x[8],0,x[9],x[10],x[11],1};
}
Json wire_pose(int id,const LanPose& p){
 Json wheels=Json::array();
 for(const auto& wheel:p.wheels)wheels.push_back(compact(wheel));
 return {{"type","pose"},{"id",id},{"car",compact(p.car)},{"wheels",std::move(wheels)},
         {"velocity",{p.velocity.x,p.velocity.y,p.velocity.z}},{"damage",p.damage},
         {"recovery",p.recovery_generation},{"score",p.score},{"finished",p.finished},{"ejected",p.ejected}};
}
LanPose read_pose(const Json& j){
 LanPose p;
 p.car=expand(j.at("car"));
 const auto& wheels=j.at("wheels");
 if(!wheels.is_array()||wheels.size()!=p.wheels.size())throw std::runtime_error("Invalid LAN wheels");
 for(size_t i=0;i<p.wheels.size();++i)p.wheels[i]=expand(wheels[i]);
 const auto v=j.at("velocity").get<std::array<float,3>>();
 p.velocity={v[0],v[1],v[2]};
 p.damage=j.at("damage").get<float>();p.recovery_generation=j.at("recovery").get<uint64_t>();
 p.score=j.at("score").get<double>();p.finished=j.at("finished").get<bool>();p.ejected=j.at("ejected").get<bool>();
 if(!std::isfinite(length(p.velocity))||length(p.velocity)>300||!std::isfinite(p.damage)||
    p.damage<0||p.damage>1||!std::isfinite(p.score)||p.score<0||p.score>1000000)
  throw std::runtime_error("Invalid LAN pose");
 return p;
}
Mat interpolate_pose(const Mat& a,const Mat& b,float t){
 const auto f=unit(direction(a,{0,0,1})*(1-t)+direction(b,{0,0,1})*t,direction(b,{0,0,1}));
 const auto u=unit(direction(a,{0,1,0})*(1-t)+direction(b,{0,1,0})*t,direction(b,{0,1,0}));
 const auto r=unit(cross(u,f),direction(b,{1,0,0}));
 const auto up=unit(cross(f,r),u);
 const auto p=point(a,{})*(1-t)+point(b,{})*t;
 return {r.x,r.y,r.z,0,up.x,up.y,up.z,0,f.x,f.y,f.z,0,p.x,p.y,p.z,1};
}
struct PoseHistory {
 LanPose previous,current;
 Time before{},after{};
 void update(const LanPose& p){
  const auto now=Clock::now();
  if(after==Time{}||current.recovery_generation!=p.recovery_generation){previous=p;before=now;}
  else{previous=current;before=after;}
  current=p;after=now;
 }
 LanPose sample()const{
  if(after==before)return current;
  const auto desired=Clock::now()-std::chrono::milliseconds(55);
  const auto fraction=std::chrono::duration<float>(desired-before).count()/std::chrono::duration<float>(after-before).count();
  const auto t=std::clamp(fraction,0.f,1.f);
  LanPose out=current;
  if(desired>after){
   const float seconds=std::min(.10f,std::chrono::duration<float>(desired-after).count());
   const Vec offset=out.velocity*seconds;
   for(auto* pose:{&out.car,&out.wheels[0],&out.wheels[1],&out.wheels[2],&out.wheels[3]}){
    (*pose)[12]+=offset.x;(*pose)[13]+=offset.y;(*pose)[14]+=offset.z;
   }
   return out;
  }
  out.car=interpolate_pose(previous.car,current.car,t);
  for(size_t i=0;i<out.wheels.size();++i)out.wheels[i]=interpolate_pose(previous.wheels[i],current.wheels[i],t);
  return out;
 }
};
}

struct LanSession::Impl {
 std::optional<SocketRuntime> runtime;
 SocketHandle listener;
 std::vector<Connection> clients;
 std::optional<Connection> server;
 std::vector<LanPlayer> roster;
 std::map<int,PoseHistory> poses;
 std::vector<LanVoicePacket> voice;
 bool voice_allowed()const{return phase==Phase::Lobby||phase==Phase::Playing;}
 void receive_voice(int id,const Json& j){
  auto samples=j.at("samples").get<LanVoiceFrame>();
  if(voice.size()>=32)voice.erase(voice.begin());
  voice.push_back({id,samples});
  if(host)broadcast_voice({{"type","voice"},{"id",id},{"samples",samples}},id);
 }
 void broadcast_voice(const Json& j,int except){
  for(auto& c:clients)if(c.socket&&c.id&&c.id!=except&&c.output.size()<4096)c.queue(j);
 }
 LanRules rules,initial_rules;
 LanPlayer local;
 std::string room_name;
 std::function<std::vector<LanRoomInfo>()> directory;
 bool dedicated=false,remote_owner=false;
 int owner()const{return roster.empty()?0:roster.front().id;}
 bool all_ready()const{
  return phase==Phase::Lobby&&roster.size()>=2&&
   std::all_of(roster.begin(),roster.end(),[](const auto& p){return p.connected&&p.ready;});
 }
 void prepare(){
  for(auto& player:roster)player.loaded=false;
  broadcast(lobby());broadcast({{"type","prepare"}});
  set_phase(Phase::Loading);request_load=!dedicated;
 }
 void update_rules(LanRules value){
  rules=std::move(value);rules.environment.validate();
  for(auto& player:roster)player.ready=false;
  lobby_changed=true;
 }
 bool host=false,request_load=false,hello_sent=false,lobby_changed=false;
 Phase phase=Phase::Closed;
 uint16_t port=lan_default_port;
 std::string error;
 Time entered=Clock::now(),last_ping{},last_pose{};

 void set_phase(Phase value){phase=value;entered=Clock::now();}
 void fail(const std::string& reason){listener.reset();clients.clear();server.reset();error=reason;set_phase(Phase::Failed);}
 LanPlayer* player(int id){
  const auto i=std::find_if(roster.begin(),roster.end(),[&](const LanPlayer& p){return p.id==id;});
  return i==roster.end()?nullptr:&*i;
 }
 void drop(size_t index){
  const auto id=clients[index].id;
  if(id){
   if(phase==Phase::Lobby)roster.erase(std::remove_if(roster.begin(),roster.end(),[&](const auto& p){return p.id==id;}),roster.end());
   else if(auto* p=player(id))p->connected=false;
   lobby_changed=true;
  }
  clients.erase(clients.begin()+index);
 }
 void broadcast(const Json& message,int except=0){
  // A slow peer must never block the other players. Close its socket and
  // defer removal until receive/flush has unwound.
  for(auto& peer:clients)if(peer.socket&&peer.id&&peer.id!=except)
   try{peer.queue(message);}catch(const std::exception&){peer.socket.reset();}
 }
 Json lobby()const{
  Json players=Json::array();
  for(const auto& p:roster)players.push_back(wire_player(p));
  return {{"type","lobby"},{"rules",wire_rules(rules)},{"players",std::move(players)},{"owner",owner()},{"room",room_name}};
 }
 void client_queue(const Json& j){
  if(!server)return;
  try{server->queue(j);}catch(const std::exception& e){fail(e.what());}
 }
 void process_host(Connection& c,const Json& j){
  const auto type=j.at("type").get<std::string>();
  if(type=="ping")return;
  if(!c.id){
   if(type=="rooms"&&directory){
    Json rooms=Json::array();
    for(const auto& room:directory())rooms.push_back({{"name",room.name},{"port",room.port},{"stage",room.stage},{"players",room.players},{"open",room.open}});
    c.queue({{"type","rooms"},{"version",wire_version},{"rooms",std::move(rooms)}});
    c.directory_done=true;
    return;
   }
   if(type!="hello"||j.at("version").get<int>()!=wire_version||phase!=Phase::Lobby)throw std::runtime_error("LAN protocol or lobby mismatch");
   auto p=read_player(j.at("player"));
   for(int i=dedicated?1:2;i<=lan_max_players;++i)if(!player(i)){c.id=i;break;}
   if(!c.id)throw std::runtime_error("LAN lobby is full");
   p.id=c.id;p.ready=p.loaded=false;p.connected=true;roster.push_back(p);
   std::sort(roster.begin(),roster.end(),[](const auto& a,const auto& b){return a.id<b.id;});
   c.queue({{"type","welcome"},{"version",wire_version},{"id",c.id}});
   lobby_changed=true;
   return;
  }
  auto* p=player(c.id);
  if(!p)throw std::runtime_error("Invalid LAN peer");
  if(type=="voice"){if(voice_allowed())receive_voice(c.id,j);return;}
  if((type=="car"||type=="ready")&&phase!=Phase::Lobby)return;
  if(type=="rules"||type=="start"){
   if(!dedicated||c.id!=owner()||phase!=Phase::Lobby)return;
   if(type=="rules")update_rules(read_rules(j.at("rules")));
   else if(all_ready())prepare();
   return;
  }
  if(type=="ready"&&phase==Phase::Lobby){p->ready=j.at("value").get<bool>();lobby_changed=true;}
  else if(type=="car"&&phase==Phase::Lobby){
   int car=j.at("car").get<int>(),skin=j.at("skin").get<int>();
   if(!valid_car(car,skin))throw std::runtime_error("Invalid LAN car");
   p->car_id=car;p->skin=skin;p->ready=false;lobby_changed=true;
  }else if(type=="loaded"&&phase==Phase::Loading){p->loaded=true;lobby_changed=true;}
  else if(type=="pose"&&phase==Phase::Playing){
   const auto pose=read_pose(j);
   auto& history=poses[c.id];
   if(Clock::now()-history.after<std::chrono::milliseconds(20))return;
   history.update(pose);
   broadcast(wire_pose(c.id,pose),c.id);
  }else throw std::runtime_error("Unexpected LAN message");
 }
 void process_client(const Json& j){
  const auto type=j.at("type").get<std::string>();
  if(type=="ping")return;
  if(type=="voice"){
   if(!voice_allowed())return;
   const int id=j.at("id").get<int>();
   if(id!=local.id&&player(id))receive_voice(id,j);
   return;
  }
  if(type=="welcome"&&phase==Phase::Connecting){
   const auto id=j.at("id").get<int>();
   if(j.at("version").get<int>()!=wire_version||id<1||id>lan_max_players)throw std::runtime_error("Incompatible LAN host");
   local.id=id;set_phase(Phase::Lobby);
   return;
  }
  if(type=="lobby"&&(phase==Phase::Lobby||phase==Phase::Loading||phase==Phase::Playing)){
   const auto& players=j.at("players");
   if(!players.is_array()||players.size()>lan_max_players)throw std::runtime_error("Invalid LAN roster");
   std::vector<LanPlayer> list;
   for(const auto& entry:players){
    auto p=read_player(entry);
    if(p.id<1||p.id>lan_max_players||(!list.empty()&&p.id<=list.back().id))throw std::runtime_error("Invalid LAN roster");
    list.push_back(std::move(p));
   }
   if(list.empty()||std::none_of(list.begin(),list.end(),[&](const auto& p){return p.id==local.id;}))throw std::runtime_error("Missing LAN roster member");
   if(phase!=Phase::Lobby&&(list.size()!=roster.size()||!std::equal(list.begin(),list.end(),roster.begin(),[](const auto& a,const auto& b){return a.id==b.id&&a.car_id==b.car_id&&a.skin==b.skin;})))
    throw std::runtime_error("LAN roster changed during race");
   auto selection=read_rules(j.at("rules"));
   if(phase!=Phase::Lobby&&wire_rules(selection)!=wire_rules(rules))throw std::runtime_error("LAN race rules changed");
   const int room_owner=j.at("owner").get<int>();
   if(room_owner!=list.front().id)throw std::runtime_error("Invalid room owner");
   room_name=j.value("room",std::string{});
   if(room_name.size()>80)throw std::runtime_error("Invalid room name");
   remote_owner=!room_name.empty()&&room_owner==local.id;
   rules=std::move(selection);roster=std::move(list);
   return;
  }
  if(type=="prepare"&&phase==Phase::Lobby){
   if(roster.size()<2||!valid_rules(rules))throw std::runtime_error("LAN race is missing selections");
   set_phase(Phase::Loading);request_load=true;
  }else if(type=="launch"&&phase==Phase::Loading){
   if(const auto* p=player(local.id);!p||!p->loaded)throw std::runtime_error("LAN race launched before loading");
   set_phase(Phase::Playing);
  }else if(type=="pose"&&phase==Phase::Playing){
   const int id=j.at("id").get<int>();
   const auto* p=player(id);
   if(!p||!p->connected||id==local.id)throw std::runtime_error("Unknown LAN driver");
   poses[id].update(read_pose(j));
  }else throw std::runtime_error("Unexpected LAN host message");
 }
 void pulse(){
  const auto now=Clock::now();
  if(last_ping!=Time{}&&now-last_ping<std::chrono::seconds(1))return;
  last_ping=now;
  const Json ping={{"type","ping"}};
  if(host)broadcast(ping);else if(phase!=Phase::Connecting)client_queue(ping);
 }
 void poll_host(){
  if(dedicated&&phase!=Phase::Lobby&&
     std::none_of(roster.begin(),roster.end(),[](const auto& p){return p.connected;})){
   roster.clear();poses.clear();voice.clear();rules=initial_rules;set_phase(Phase::Lobby);
  }
  if(phase==Phase::Lobby||directory){
   for(int i=0;i<lan_max_players;++i){
    SocketHandle fd(accept(listener.fd,nullptr,nullptr));
    if(!fd)break;
    if(clients.size()>=size_t(directory?lan_max_players+4:lan_max_players-1))continue;
    try{nonblocking(fd.fd);stream_low_latency(fd.fd);clients.emplace_back(std::move(fd));}catch(const std::exception&){}
   }
  }
  const auto now=Clock::now();
  for(size_t i=0;i<clients.size();){
   auto& peer=clients[i];
   const auto timeout=peer.id?std::chrono::seconds(phase==Phase::Loading?90:12):std::chrono::seconds(4);
   if(!peer.socket||now-peer.heard>timeout){drop(i);continue;}
   try{peer.receive([&](const Json& j){process_host(peer,j);});}
   catch(const std::exception&){drop(i);continue;}
   ++i;
  }
  if(lobby_changed){lobby_changed=false;broadcast(lobby());}
  if(phase==Phase::Loading){
   if(now-entered>std::chrono::seconds(100)){
    if(dedicated){clients.clear();roster.clear();poses.clear();rules=initial_rules;set_phase(Phase::Lobby);}
    else fail("LAN players did not finish loading");
    return;
   }
   if(std::all_of(roster.begin(),roster.end(),[](const auto& p){return !p.connected||p.loaded;})){
    broadcast({{"type","launch"}});set_phase(Phase::Playing);
   }
  }
  pulse();
  for(size_t i=0;i<clients.size();){
   if(!clients[i].socket){drop(i);continue;}
   try{
    clients[i].flush();
    if(clients[i].directory_done&&clients[i].output.empty()){drop(i);continue;}
   }catch(const std::exception&){drop(i);continue;}
   ++i;
  }
 }
 void poll_client(){
  const auto now=Clock::now();
  if(phase==Phase::Connecting&&!hello_sent){
   if(now-entered>std::chrono::seconds(7))throw std::runtime_error("LAN connection timed out");
   fd_set write,error_set;
   FD_ZERO(&write);FD_ZERO(&error_set);FD_SET(server->socket.fd,&write);FD_SET(server->socket.fd,&error_set);
   timeval zero{};
#ifdef _WIN32
   const auto count=select(0,nullptr,&write,&error_set,&zero);
#else
   const auto count=select(server->socket.fd+1,nullptr,&write,&error_set,&zero);
#endif
   if(count<0)throw std::runtime_error("LAN connection failed");
   if(!count)return;
   int value=0;
#ifdef _WIN32
   int size=sizeof(value);
#else
   socklen_t size=sizeof(value);
#endif
   if(getsockopt(server->socket.fd,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&value),&size)!=0||value)
    throw std::runtime_error("Could not connect to LAN host");
   stream_low_latency(server->socket.fd);
   server->queue({{"type","hello"},{"version",wire_version},{"player",wire_player(local)}});
   hello_sent=true;
  }
  if(now-server->heard>std::chrono::seconds(phase==Phase::Loading?100:12))throw std::runtime_error("LAN host stopped responding");
  server->receive([&](const Json& j){process_client(j);});
  pulse();
  server->flush();
 }
};

LanSession::LanSession():p_(std::make_unique<Impl>()){}
LanSession::~LanSession()=default;
void LanSession::close(){p_=std::make_unique<Impl>();}
void LanSession::host(uint16_t port,LanPlayer player,const LanRules& rules){
 close();
 if(!valid_rules(rules)||!valid_car(player.car_id,player.skin))throw std::runtime_error("Select a valid LAN track and car");
 p_->runtime.emplace();
 p_->listener=stream_socket();
 sockaddr_in address{};
 address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons(port);
#ifdef _WIN32
 const int exclusive=1;
 setsockopt(p_->listener.fd,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
#else
 const int reuse=1;
 setsockopt(p_->listener.fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
#endif
 if(bind(p_->listener.fd,reinterpret_cast<const sockaddr*>(&address),sizeof(address))!=0||
    listen(p_->listener.fd,lan_max_players)!=0){close();throw std::runtime_error("Could not open LAN host port");}
#ifdef _WIN32
 int size=sizeof(address);
#else
 socklen_t size=sizeof(address);
#endif
 if(getsockname(p_->listener.fd,reinterpret_cast<sockaddr*>(&address),&size)!=0){close();throw std::runtime_error("Could not read LAN host port");}
 p_->port=ntohs(address.sin_port);
 p_->rules=rules;p_->rules.environment.validate();
 player.id=1;player.ready=true;player.loaded=false;player.connected=true;
 p_->local=player;p_->roster.push_back(player);p_->host=true;p_->set_phase(Phase::Lobby);
}
void LanSession::join(const std::string& address,LanPlayer player){
 close();
 p_->runtime.emplace();
 const auto addr=endpoint(address);
 if(!valid_car(player.car_id,player.skin))throw std::runtime_error("Select a valid LAN car");
 auto socket=stream_socket();
 if(connect(socket.fd,reinterpret_cast<const sockaddr*>(&addr),sizeof(addr))!=0&&!would_block(socket_error()))
  throw std::runtime_error("Could not connect to LAN host");
 p_->port=ntohs(addr.sin_port);p_->local=std::move(player);p_->server.emplace(std::move(socket));p_->set_phase(Phase::Connecting);
}
void LanSession::poll(){
 try{
  if(p_->host&&p_->listener)p_->poll_host();
  else if(p_->server)p_->poll_client();
 }catch(const std::exception& e){p_->fail(e.what());}
}
LanSession::Phase LanSession::phase()const{return p_->phase;}
bool LanSession::is_host()const{return p_->host||p_->remote_owner;}
int LanSession::local_id()const{return p_->local.id;}
uint16_t LanSession::port()const{return p_->port;}
const std::string& LanSession::error()const{return p_->error;}
const LanRules& LanSession::rules()const{return p_->rules;}
const std::vector<LanPlayer>& LanSession::players()const{return p_->roster;}
bool LanSession::can_start()const{return is_host()&&p_->all_ready();}
bool LanSession::set_rules(const LanRules& rules){
 if(!is_host()||p_->phase!=Phase::Lobby||!valid_rules(rules))return false;
 if(p_->host)p_->update_rules(rules);
 else p_->client_queue({{"type","rules"},{"rules",wire_rules(rules)}});
 return true;
}
void LanSession::set_car(int id,int skin){
 if(p_->phase!=Phase::Lobby||!valid_car(id,skin))return;
 if(p_->host){
  auto& local=p_->roster.front();local.car_id=id;local.skin=skin;local.ready=false;p_->lobby_changed=true;
 }else p_->client_queue({{"type","car"},{"car",id},{"skin",skin}});
}
void LanSession::set_ready(bool ready){
 if(p_->phase!=Phase::Lobby)return;
 if(p_->host){
  if(auto* local=p_->player(p_->local.id)){local->ready=ready;p_->lobby_changed=true;}
 }else p_->client_queue({{"type","ready"},{"value",ready}});
}
bool LanSession::start(){
 if(!can_start())return false;
 if(p_->host)p_->prepare();
 else p_->client_queue({{"type","start"}});
 return true;
}
void LanSession::serve(uint16_t port,std::string room,const LanRules& rules){
 if(room.empty()||room.size()>80)throw std::runtime_error("Room names must contain 1–80 UTF-8 bytes");
 LanPlayer placeholder;placeholder.name="SERVER";host(port,placeholder,rules);
 p_->dedicated=true;p_->room_name=std::move(room);p_->initial_rules=rules;
 p_->roster.clear();p_->local={};
}
LanRoomInfo LanSession::room_info()const{
 const int count=int(std::count_if(p_->roster.begin(),p_->roster.end(),[](const auto& p){return p.connected;}));
 return {p_->room_name,p_->rules.stage,p_->port,count,p_->phase==Phase::Lobby&&count<lan_max_players};
}
const std::string& LanSession::room_name()const{return p_->room_name;}
void LanSession::set_directory(std::function<std::vector<LanRoomInfo>()> rooms){p_->directory=std::move(rooms);}
std::string LanSession::room_address(const std::string& address,uint16_t port){
 return address.substr(0,address.find(':'))+":"+std::to_string(port);
}
std::vector<LanRoomInfo> LanSession::list_rooms(const std::string& address){
 SocketRuntime runtime;
 auto target=endpoint(address);
 auto socket=stream_socket();
 if(connect(socket.fd,reinterpret_cast<const sockaddr*>(&target),sizeof(target))!=0&&!would_block(socket_error()))
  throw std::runtime_error("Could not reach OpenFO master server");
 Connection connection(std::move(socket));
 connection.queue({{"type","rooms"}});
 const auto deadline=Clock::now()+std::chrono::seconds(3);
 std::optional<std::vector<LanRoomInfo>> response;
 while(Clock::now()<deadline){
  fd_set ready;FD_ZERO(&ready);FD_SET(connection.socket.fd,&ready);
  timeval timeout{0,20000};
#ifdef _WIN32
  const int nfds=0;
#else
  const int nfds=connection.socket.fd+1;
#endif
  if(!connection.output.empty()){
   if(select(nfds,nullptr,&ready,nullptr,&timeout)>0)connection.flush();
   continue;
  }
  if(select(nfds,&ready,nullptr,nullptr,&timeout)<=0)continue;
  try{
   connection.receive([&](const Json& message){
    const auto& rooms=message.at("rooms");
    if(message.at("type")!="rooms"||message.at("version")!=wire_version||!rooms.is_array()||rooms.size()>16)
     throw std::runtime_error("Incompatible OpenFO room directory");
    std::vector<LanRoomInfo> result;
    for(const auto& row:rooms){
     LanRoomInfo room{row.at("name"),row.at("stage"),row.at("port"),row.at("players"),row.at("open")};
     if(room.port==0||room.name.empty()||room.name.size()>80||room.players<0||room.players>lan_max_players)
      throw std::runtime_error("Invalid room directory");
     result.push_back(std::move(room));
    }
    response=std::move(result);
   });
  }catch(const std::exception&){if(!response)throw;}
  if(response)return std::move(*response);
 }
 throw std::runtime_error("OpenFO master server did not answer");
}
bool LanSession::take_load_request(){const auto request=p_->request_load;p_->request_load=false;return request;}
void LanSession::loaded(){
 if(p_->phase!=Phase::Loading)return;
 if(auto* local=p_->player(p_->local.id))local->loaded=true;
 if(p_->host)p_->lobby_changed=true;else p_->client_queue({{"type","loaded"}});
}
void LanSession::publish(const LanPose& pose){
 if(p_->phase!=Phase::Playing||Clock::now()-p_->last_pose<std::chrono::milliseconds(50))return;
 p_->last_pose=Clock::now();
 const auto message=wire_pose(p_->local.id,pose);
 if(p_->host)p_->broadcast(message);else p_->client_queue(message);
}
void LanSession::send_voice(const LanVoiceFrame& samples){
 if(!p_->voice_allowed())return;
 Json message={{"type","voice"},{"id",p_->local.id},{"samples",samples}};
 if(p_->host)p_->broadcast_voice(message,0);
 else if(p_->server&&p_->server->output.size()<4096)p_->client_queue(message);
}
std::vector<LanVoicePacket> LanSession::take_voice(){
 std::vector<LanVoicePacket> voice;voice.swap(p_->voice);return voice;
}
std::optional<LanPose> LanSession::peer_pose(int id,bool interpolated)const{
 const auto it=p_->poses.find(id);
 if(it==p_->poses.end())return {};
 return interpolated?it->second.sample():it->second.current;
}
Mat lan_grid_spawn(Mat base,int slot,bool circuit){
 if(slot<=0)return base;
 // Keep single-start stages usable without stacking several cars at one point.
 const Vec offset=circuit?Vec{slot%2?2.6f:-2.6f,0,-5.5f*float((slot+1)/2)}:
                          Vec{2.8f*float(slot%3),0,-5.0f*float(slot/3+1)};
 return multiply(base,translate(offset));
}
}
