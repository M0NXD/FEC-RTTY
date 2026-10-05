#pragma once
#include <span>
#include <vector>
namespace fectty {
struct TimingTrackResult { std::vector<float> samples; double estimated_ppm=0; double score=0; };
TimingTrackResult compensate_clock_search(std::span<const float> samples,double max_ppm=100,double step_ppm=10);
}
