#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace fectty {

// Fixed-format audio display analysis; independent of the modem decoder.
class WaterfallSpectrum {
public:
    static constexpr size_t fft_size = 4096;
    static constexpr size_t hop_size = 2048;
    static constexpr double sample_rate = 48000.0;
    static constexpr double bin_hz = sample_rate / fft_size;
    static constexpr double max_hz = 3500.0;
    static constexpr size_t columns = size_t(max_hz / bin_hz) + 2;
    using Row = std::vector<float>;

    std::vector<Row> push(std::span<const float> samples);
    void reset() { pending_.clear(); }

private:
    std::vector<float> pending_;
    static Row analyse(std::span<const float> samples);
};

} // namespace fectty
