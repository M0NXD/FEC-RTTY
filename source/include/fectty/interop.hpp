#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace fectty {
struct InteropResult { bool ok=false; std::string received; size_t frames=0; };
InteropResult simulate_two_station(std::string_view text,double noise_std=0.02,double freq_offset_hz=0,double clock_ppm=0,uint32_t seed=1234);

struct ArqInteropResult {
    bool ok = false;
    std::string received;
    size_t data_attempts = 0;
    size_t control_attempts = 0;
    size_t retransmissions = 0;
    size_t negative_acknowledgements = 0;
    size_t duplicate_frames = 0;
    size_t ignored_frames = 0;
    uint32_t max_backoff_ms = 0;
};

ArqInteropResult simulate_arq_two_station(
    std::string_view text, double noise_std = 0.02,
    double freq_offset_hz = 0, double clock_ppm = 0, uint32_t seed = 1234,
    bool drop_first_data = false, bool drop_first_control = false);
}
