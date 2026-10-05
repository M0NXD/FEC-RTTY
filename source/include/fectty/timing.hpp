#pragma once
#include "fectty/fsk4.hpp"
#include <cstddef>
#include <span>
namespace fectty {
struct TimingResult{size_t sample_offset=0;double score=0;};
TimingResult find_symbol_timing(std::span<const float> samples,size_t symbols,const FskConfig& cfg={});
}
