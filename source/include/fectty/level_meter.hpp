#pragma once
#include <cstddef>
#include <span>
namespace fectty {
struct LevelReading { double peak=0,rms=0,peak_dbfs=-120,rms_dbfs=-120; size_t clipped_samples=0; bool clipping=false; };
LevelReading measure_level(std::span<const float> samples,double clip_threshold=0.98);
}
