#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace fectty {
struct FskConfig { double sample_rate=48000, symbol_rate=50; double tones[4]={1425,1475,1525,1575}; };
std::vector<float> fsk4_modulate(std::span<const uint8_t> coded_bits,const FskConfig& c={});
std::vector<int8_t> fsk4_demodulate(std::span<const float> samples,size_t bit_count,const FskConfig& c={});
}
