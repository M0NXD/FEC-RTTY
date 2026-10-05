#include "fectty/frame_receiver.hpp"

#include "fectty/convolutional.hpp"
#include "fectty/interleaver.hpp"
#include "fectty/sync.hpp"
#include "fectty/viterbi.hpp"

#include <algorithm>
#include <cmath>

namespace fectty {
namespace {

size_t frame_samples(size_t payload_size, const FskConfig& config) {
    const size_t samples_per_symbol =
        size_t(std::llround(config.sample_rate / config.symbol_rate));
    const size_t raw_bits = (payload_size + 4) * 8;
    const size_t coded_bits = (raw_bits + 6) * 2;
    const size_t total_bits = 32 + coded_bits;
    return ((total_bits + 1) / 2) * samples_per_symbol;
}

} // namespace

std::optional<DecodedAirFrame> decode_air_frame(
    std::span<const float> audio, const FskConfig& config) {
    size_t largest_payload = 8;
    while (largest_payload > 0 &&
           audio.size() < frame_samples(largest_payload, config)) {
        --largest_payload;
    }
    if (audio.size() < frame_samples(0, config)) return std::nullopt;

    const size_t raw_bits = (largest_payload + 4) * 8;
    const size_t coded_bits = (raw_bits + 6) * 2;
    const size_t total_bits = 32 + coded_bits;
    const size_t demodulated_samples = frame_samples(largest_payload, config);
    const auto soft = fsk4_demodulate(
        audio.first(demodulated_samples), total_bits, config);

    // The first symbols are common to every legal payload length. Demodulate
    // once, then try the legal FEC/interleaver lengths using soft-bit prefixes.
    for (size_t n = largest_payload + 1; n-- > 0;) {
        const size_t candidate_raw_bits = (n + 4) * 8;
        const size_t candidate_coded_bits = (candidate_raw_bits + 6) * 2;
        const size_t candidate_total_bits = 32 + candidate_coded_bits;
        if (candidate_total_bits > soft.size()) continue;
        const int sync_errors = sync_distance(
            std::span<const int8_t>(soft).first(candidate_total_bits));
        if (sync_errors > 4) continue;
        std::vector<int8_t> body(
            soft.begin() + 32,
            soft.begin() + candidate_total_bits);
        auto decoded_bits = viterbi_decode(deinterleave_soft(body));
        auto bytes = bits_to_bytes(decoded_bits);
        bytes.resize(n + 4);
        auto frame = decode_frame(bytes);
        if (frame && frame->payload.size() == n) {
            return DecodedAirFrame{
                *frame, frame_samples(n, config), sync_errors};
        }
    }
    return std::nullopt;
}

} // namespace fectty
