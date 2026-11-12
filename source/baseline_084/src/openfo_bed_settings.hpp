#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace ofo {
struct StuntAimSettings {
 double time_scale=.25;
 double degrees_per_second=70;
};
struct LobbyLayout {
 float settings_top=97;
 float settings_row_height=32;
 float roster_row_height=30;
};
struct OpenFOBedSettings {
 std::optional<std::string> master_server;
 StuntAimSettings stunts;
 LobbyLayout lobby;
};
OpenFOBedSettings load_openfo_bed_settings(const std::filesystem::path&);
}
