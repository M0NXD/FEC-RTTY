#include "fectty/settings.hpp"
#include "fectty/parse.hpp"
#include "fectty/paths.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fectty {
namespace {
std::string escape(std::string value) {
    for (auto& c : value) if (c == '\n' || c == '\r') c = ' ';
    return value;
}
template<class T> bool bounded(std::string_view text, T& target, T low, T high) {
    T value{};
    if (!parse_number(text, value) || value < low || value > high) return false;
    target = value;
    return true;
}
} // namespace

bool save_settings(const AppSettings& s, const std::string& path) {
    std::ostringstream data;
    data << std::setprecision(std::numeric_limits<double>::max_digits10)
         << "callsign=" << escape(s.callsign)
         << "\naudio_backend=" << escape(s.audio_backend)
         << "\nrig_backend=" << escape(s.rig_backend)
         << "\nrig_host=" << escape(s.rig_host)
         << "\nrig_port=" << s.rig_port
         << "\nhamlib_model=" << s.hamlib_model
         << "\nhamlib_device=" << escape(s.hamlib_device)
         << "\nhamlib_baud=" << s.hamlib_baud
         << "\nomnirig_number=" << s.omnirig_number
         << "\ntx_limit_seconds=" << s.tx_limit_seconds
         << "\nptt_source=" << escape(s.ptt_source)
         << "\naudio_input=" << escape(s.audio_input)
         << "\naudio_output=" << escape(s.audio_output)
         << "\naudio_input_name=" << escape(s.audio_input_name)
         << "\naudio_output_name=" << escape(s.audio_output_name)
         << "\noutput_volume=" << s.output_volume
         << "\ncenter_hz=" << s.center_hz
         << "\ntx_center_hz=" << s.tx_center_hz
         << "\nlink_offsets=" << (s.link_offsets ? 1 : 0)
         << "\nwaterfall_floor_dbfs=" << s.waterfall_floor_dbfs
         << "\nptt_lead_ms=" << s.ptt_lead_ms
         << "\nptt_tail_ms=" << s.ptt_tail_ms
         << "\nidle_release_ms=" << s.idle_release_ms
         << "\nstation_id=" << unsigned(s.station_id)
         << "\npeer_station_id=" << unsigned(s.peer_station_id) << '\n';
    static std::atomic<unsigned> serial{0};
    const auto target = utf8_file_path(path);
    auto temporary = target;
    temporary += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
                          + "." + std::to_string(serial.fetch_add(1));
    std::ofstream file(temporary, std::ios::binary);
    if (!file) return false;
    file << data.str();
    file.close();
    std::error_code error;
    if (file.fail()) { std::filesystem::remove(temporary, error); return false; }
#ifdef _WIN32
    const bool replaced = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    std::filesystem::rename(temporary, target, error);
    const bool replaced = !error;
#endif
    if (!replaced) std::filesystem::remove(temporary, error);
    return replaced;
}

bool load_settings(AppSettings& settings, const std::string& path) {
    std::ifstream file(utf8_file_path(path));
    if (!file) return false;
    auto candidate = settings;
    bool have_tx_center = false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto split = line.find('=');
        if (split == std::string::npos) continue;
        const auto key = line.substr(0, split), value = line.substr(split + 1);
        bool valid = true;
        if (key == "callsign") candidate.callsign = value;
        else if (key == "audio_backend") candidate.audio_backend = value;
        else if (key == "rig_backend") candidate.rig_backend = value;
        else if (key == "rig_host") candidate.rig_host = value;
        else if (key == "rig_port") valid = bounded(value, candidate.rig_port, uint16_t{1}, uint16_t{65535});
        else if (key == "hamlib_model") valid = bounded(value, candidate.hamlib_model, 0, 999999);
        else if (key == "hamlib_device") candidate.hamlib_device = value;
        else if (key == "hamlib_baud") valid = bounded(value, candidate.hamlib_baud, 300, 115200);
        else if (key == "omnirig_number") valid = bounded(value, candidate.omnirig_number, 1, 2);
        else if (key == "tx_limit_seconds") valid = bounded(value, candidate.tx_limit_seconds, 1, 600);
        else if (key == "ptt_source") { valid=value=="on"||value=="mic"||value=="data"; if(valid)candidate.ptt_source=value; }
        else if (key == "audio_input") candidate.audio_input = value;
        else if (key == "audio_output") candidate.audio_output = value;
        else if (key == "audio_input_name") candidate.audio_input_name = value;
        else if (key == "audio_output_name") candidate.audio_output_name = value;
        else if (key == "output_volume") valid = bounded(value, candidate.output_volume, 0.0, 1.0);
        else if (key == "center_hz") valid = bounded(value, candidate.center_hz, 300.0, 3000.0);
        else if (key == "tx_center_hz") { valid = bounded(value, candidate.tx_center_hz, 300.0, 3000.0); have_tx_center = true; }
        else if (key == "link_offsets") { unsigned linked = 0; valid = bounded(value, linked, 0u, 1u); if (valid) candidate.link_offsets = linked != 0; }
        else if (key == "waterfall_floor_dbfs") valid = bounded(value, candidate.waterfall_floor_dbfs, -110, -20);
        else if (key == "ptt_lead_ms") valid = bounded(value, candidate.ptt_lead_ms, 0, 2000);
        else if (key == "ptt_tail_ms") valid = bounded(value, candidate.ptt_tail_ms, 0, 2000);
        else if (key == "idle_release_ms") valid = bounded(value, candidate.idle_release_ms, 0, 60000);
        else if (key == "station_id") valid = bounded(value, candidate.station_id, uint8_t{1}, uint8_t{254});
        else if (key == "peer_station_id") valid = bounded(value, candidate.peer_station_id, uint8_t{1}, uint8_t{255});
        if (!valid) return false;
    }
    if (file.bad()) return false;
    if (!have_tx_center || candidate.link_offsets) candidate.tx_center_hz = candidate.center_hz;
    settings = std::move(candidate);
    return true;
}
} // namespace fectty
