#pragma once
#include <span>
#include <vector>
namespace fectty {
std::vector<float> resample_clock_error(std::span<const float> input,double ppm);
}
