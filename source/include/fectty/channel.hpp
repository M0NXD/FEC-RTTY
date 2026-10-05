#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace fectty {
struct ChannelConfig { double noise_std=0; double gain=1; size_t impulse_period=0; double impulse_amplitude=0; uint32_t seed=0xFEC77; };
std::vector<float> apply_channel(std::span<const float>,const ChannelConfig&);
}
