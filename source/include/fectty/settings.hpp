#pragma once
#include <cstdint>
#include <string>
namespace fectty {
struct AppSettings { std::string callsign; std::string audio_backend="winmm"; std::string rig_backend="none"; std::string rig_host="127.0.0.1"; uint16_t rig_port=4532; std::string audio_input; std::string audio_output; std::string audio_input_name; std::string audio_output_name; double output_volume=1.0; double center_hz=1500; double tx_center_hz=1500; bool link_offsets=true; int waterfall_floor_dbfs=-85; int ptt_lead_ms=150; int ptt_tail_ms=100; int idle_release_ms=750; uint8_t station_id=1; uint8_t peer_station_id=2; };
bool save_settings(const AppSettings&,const std::string& path);
bool load_settings(AppSettings&,const std::string& path);
}
