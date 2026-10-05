#pragma once
#include <cstdint>
#include <string>
namespace fectty {
// PTT arming is intentionally never persisted.
struct AppSettings {
 std::string callsign;
 std::string audio_backend="winmm", rig_backend="none", rig_host="127.0.0.1";
 uint16_t rig_port=4532;
 int hamlib_model=0, hamlib_baud=9600, omnirig_number=1, tx_limit_seconds=120;
 std::string hamlib_device, ptt_source="on";
 std::string audio_input, audio_output, audio_input_name, audio_output_name;
 double output_volume=1.0, center_hz=1500, tx_center_hz=1500;
 bool link_offsets=true;
 int waterfall_floor_dbfs=-85, ptt_lead_ms=150, ptt_tail_ms=100, idle_release_ms=750;
 uint8_t station_id=1, peer_station_id=2;
};
bool save_settings(const AppSettings&,const std::string& path);
bool load_settings(AppSettings&,const std::string& path);
}
