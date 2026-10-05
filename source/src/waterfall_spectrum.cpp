#include "fectty/waterfall_spectrum.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace fectty {

WaterfallSpectrum::Row WaterfallSpectrum::analyse(std::span<const float> samples) {
    std::vector<std::complex<double>> bins(fft_size);
    double window_sum = 0;
    for (size_t i = 0; i < fft_size; ++i) {
        const double window = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / (fft_size - 1));
        window_sum += window;
        bins[i] = (std::isfinite(samples[i]) ? samples[i] : 0.0f) * window;
    }
    for (size_t i = 1, reversed = 0; i < fft_size; ++i) {
        size_t bit = fft_size >> 1;
        for (; reversed & bit; bit >>= 1) reversed ^= bit;
        reversed ^= bit;
        if (i < reversed) std::swap(bins[i], bins[reversed]);
    }
    for (size_t length = 2; length <= fft_size; length <<= 1) {
        const auto step = std::polar(1.0, -2 * std::numbers::pi / length);
        for (size_t base = 0; base < fft_size; base += length) {
            std::complex<double> phase(1, 0);
            for (size_t k = 0; k < length / 2; ++k) {
                const auto even = bins[base + k];
                const auto odd = bins[base + k + length / 2] * phase;
                bins[base + k] = even + odd;
                bins[base + k + length / 2] = even - odd;
                phase *= step;
            }
        }
    }
    Row row(columns);
    for (size_t k = 0; k < columns; ++k) {
        const double factor = k == 0 ? 1 : 2;
        const double amplitude = std::abs(bins[k]) * factor / window_sum;
        row[k] = static_cast<float>(std::clamp(20 * std::log10(std::max(amplitude, 1e-6)), -120.0, 0.0));
    }
    return row;
}

std::vector<WaterfallSpectrum::Row> WaterfallSpectrum::push(std::span<const float> samples) {
    std::vector<Row> rows;
    // Consume arbitrarily large callers a block at a time. The retained audio
    // buffer never grows beyond one FFT window.
    while (!samples.empty()) {
        const size_t count = std::min(fft_size - pending_.size(), samples.size());
        pending_.insert(pending_.end(), samples.begin(), samples.begin() + count);
        samples = samples.subspan(count);
        if (pending_.size() == fft_size) {
            rows.push_back(analyse(pending_));
            pending_.erase(pending_.begin(), pending_.begin() + hop_size);
        }
    }
    return rows;
}

} // namespace fectty
