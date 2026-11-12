#include "frontend.hpp"
#include "file_io.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace ofo;

int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::runtime_error("Expected configured master server argument");
  const std::string built_in=argv[1];
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root=fs::temp_directory_path()/("openfo-master-"+std::to_string(stamp));
  fs::create_directory(root);
  struct Cleanup {
   fs::path path;
   ~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}
  } cleanup{root};
  Resources assets(root);
  Frontend frontend(assets);
  const auto settings=root/"online_server.txt";
  auto require_address=[&](const std::string& expected){
   frontend.setup_master_server(settings);
   if(frontend.master_address!=expected)throw std::runtime_error("Expected '"+expected+"', got '"+frontend.master_address+"'");
  };
  require_address(built_in);
  write_file_atomic(settings,"  \r\n");
  require_address(built_in);
  write_file_atomic(settings,"  example.test:28267\r\n");
  require_address("example.test:28267");
  write_file_atomic(settings,"\xef\xbb\xbf" "example.test:28267\r\n");
  require_address("example.test:28267");
  write_file_atomic(settings,"34.116.190.254:27267\r\n");
  require_address(built_in=="34.118.26.31:27267"?built_in:"34.116.190.254:27267");
  write_file_atomic(settings,"34.116.190.254:28267\r\n");
  require_address("34.116.190.254:28267");
  fs::remove(settings);
  const auto bed=root/"openfo_scripts"/"settings.bed";
  fs::create_directory(bed.parent_path());
  write_file_atomic(bed,R"bed(OpenFO = {
    Multiplayer = { MasterServer = "rooms.example.test:27280" },
    Stunts = { AimTimeScale = 0.5, AngleDegreesPerSecond = 55 },
    Lobby = { SettingsTop = 91, SettingsRowHeight = 30, RosterRowHeight = 29 },
  })bed");
  require_address("rooms.example.test:27280");
  if(frontend.bed_settings.stunts.time_scale!=.5||frontend.bed_settings.stunts.degrees_per_second!=55||
     frontend.bed_settings.lobby.settings_top!=91||frontend.bed_settings.lobby.settings_row_height!=30||
     frontend.bed_settings.lobby.roster_row_height!=29)
   throw std::runtime_error("Valid stunt and lobby BED fields were ignored");
  write_file_atomic(settings,"manual.example.test:27281");
  require_address("manual.example.test:27281");
  fs::remove(settings);
  write_file_atomic(bed,R"bed(OpenFO = {
    Multiplayer = { MasterServer = "https://invalid.example/" },
    Stunts = { AimTimeScale = 0, AngleDegreesPerSecond = "bad" },
    Lobby = { SettingsTop = 1000 },
  })bed");
  require_address(built_in);
  if(frontend.bed_settings.stunts.time_scale!=.25||frontend.bed_settings.stunts.degrees_per_second!=70||
     frontend.bed_settings.lobby.settings_top!=97)
   throw std::runtime_error("Invalid BED fields must fall back individually");
  write_file_atomic(bed,"OpenFO = {");
  require_address(built_in);
  std::cout<<"Master defaults, endpoint migration, editable BED settings and override priority passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
