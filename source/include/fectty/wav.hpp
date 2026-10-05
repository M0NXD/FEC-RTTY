#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace fectty {
struct WavData { uint32_t sample_rate=48000; std::vector<float> samples; };
bool write_wav_mono16(const std::string& path,std::span<const float> samples,uint32_t sample_rate=48000);
bool read_wav_mono16(const std::string& path,WavData& out);
}
