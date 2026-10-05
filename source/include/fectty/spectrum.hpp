#pragma once
#include <span>
#include <vector>
namespace fectty {
struct SpectrumBin { double frequency_hz=0; double power_db=0; };
std::vector<SpectrumBin> spectrum(std::span<const float> samples,double sample_rate=48000.0,size_t bins=256);
}
