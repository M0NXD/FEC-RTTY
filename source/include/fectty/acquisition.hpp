#pragma once
#include "fectty/fsk4.hpp"
#include <cstddef>
#include <span>
#include <vector>
namespace fectty {
struct AcquisitionResult { bool locked=false; size_t data_sample=0; double frequency_offset_hz=0; double score=0; };
std::vector<float> acquisition_waveform(const FskConfig& c={});
AcquisitionResult acquire(std::span<const float> samples,const FskConfig& c={},double search_hz=100.0,double step_hz=5.0,size_t analysis_stride=1);
}
