#include "openfo_bed_settings.hpp"
#include "file_io.hpp"
#include "legacy_ui/compat.hpp"
#include <charconv>

namespace ofo {
namespace {
const Json& field(const Json& table,const char* name){
 static const Json absent;
 const auto entry=table.find(name);
 return entry==table.end()?absent:*entry;
}
double number(const Json& table,const char* key,double fallback,double minimum,double maximum){
 const auto& entry=field(table,key);
 if(!entry.is_number())return fallback;
 const auto value=entry.get<double>();
 return value>=minimum&&value<=maximum?value:fallback;
}
bool valid_server(const std::string& address){
 const auto colon=address.find(':');
 const auto host=address.substr(0,colon);
 if(host.empty()||host.size()>253||
    host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-")!=std::string::npos)
  return false;
 if(colon==std::string::npos)return true;
 int port=0;
 const auto end=address.data()+address.size();
 const auto parsed=std::from_chars(address.data()+colon+1,end,port);
 return parsed.ec==std::errc{}&&parsed.ptr==end&&port>=1&&port<=65535;
}
}
OpenFOBedSettings load_openfo_bed_settings(const fs::path& path){
 OpenFOBedSettings settings;
 Json data;
 try{
  data=parse_data(text_decode(read_file(path,64*1024)));
 }catch(const std::exception&){
  return settings;
 }
 const auto& root=field(data,"OpenFO");
 const auto& master=field(field(root,"Multiplayer"),"MasterServer");
 if(master.is_string()&&valid_server(master.get_ref<const std::string&>()))
  settings.master_server=master.get<std::string>();
 const auto& stunts=field(root,"Stunts");
 settings.stunts.time_scale=number(stunts,"AimTimeScale",settings.stunts.time_scale,.05,1);
 settings.stunts.degrees_per_second=number(stunts,"AngleDegreesPerSecond",settings.stunts.degrees_per_second,1,180);
 const auto& lobby=field(root,"Lobby");
 settings.lobby.settings_top=float(number(lobby,"SettingsTop",settings.lobby.settings_top,85,103));
 settings.lobby.settings_row_height=float(number(lobby,"SettingsRowHeight",settings.lobby.settings_row_height,28,32));
 settings.lobby.roster_row_height=float(number(lobby,"RosterRowHeight",settings.lobby.roster_row_height,26,30));
 return settings;
}
}
