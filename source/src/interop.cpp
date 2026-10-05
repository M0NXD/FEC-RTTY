#include "fectty/interop.hpp"
#include "fectty/arq.hpp"
#include "fectty/channel.hpp"
#include "fectty/clock_drift.hpp"
#include "fectty/reliable_receiver.hpp"
#include "fectty/session.hpp"

namespace fectty {

InteropResult simulate_two_station(std::string_view text, double ns, double fo,
                                   double ppm, uint32_t seed) {
    FskConfig config;
    for (auto& tone : config.tones) tone += fo;
    ModemSession a(config);
    auto w = a.transmit(text);
    ChannelConfig c;
    c.noise_std = ns;
    c.seed = seed;
    auto impaired = resample_clock_error(apply_channel(w, c), ppm);
    // Include the quiet live-stream tail: positive clock error can shorten the
    // burst by a few samples relative to the receiver's fixed symbol grid.
    impaired.insert(impaired.end(), 960, 0.0f);
    ReliableReceiver b;
    auto r = b.push(impaired);
    r += b.finish();
    return {r == text, r, b.stats().frames_ok};
}

namespace {

std::vector<Frame> decode_burst(std::span<const float> samples) {
    ReliableReceiver receiver;
    receiver.push(samples);
    receiver.finish();
    return receiver.take_frames();
}

std::vector<float> impair_burst(const std::vector<float>& samples,
                                double noise_std, double clock_ppm,
                                uint32_t seed) {
    ChannelConfig channel;
    channel.noise_std = noise_std;
    channel.seed = seed;
    auto impaired = resample_clock_error(apply_channel(samples, channel), clock_ppm);
    impaired.insert(impaired.end(), 960, 0.0f);
    return impaired;
}

} // namespace

ArqInteropResult simulate_arq_two_station(std::string_view text, double noise_std,
                                          double freq_offset_hz,
                                          double clock_ppm, uint32_t seed,
                                           bool drop_first_data,
                                           bool drop_first_control) {
    ArqInteropResult result;
    ArqConfig a_config;
    a_config.local_station_id = 1;
    a_config.peer_station_id = 2;
    a_config.backoff_seed = seed;
    ArqConfig b_config = a_config;
    b_config.local_station_id = 2;
    b_config.peer_station_id = 1;
    ArqSender sender(text, a_config, 0x42);
    ArqReceiver receiver(b_config);
    if (!sender.valid()) return result;

    FskConfig a_waveform, b_waveform;
    for (auto& tone : a_waveform.tones) tone += freq_offset_hz;
    for (auto& tone : b_waveform.tones) tone -= freq_offset_hz;
    ModemSession transmitter(a_waveform);
    ModemSession control_transmitter(b_waveform);
    bool first_data = true;
    bool first_control = true;

    while (!sender.complete()) {
        bool advanced = false;
        const auto max_attempts = static_cast<size_t>(a_config.max_retries) + 1;
        for (size_t attempt = 0; attempt < max_attempts; ++attempt) {
            const auto data_frame = sender.current_frame();
            if (!data_frame) return result;
            ++result.data_attempts;
            const auto data_wave = transmitter.transmit_frame(*data_frame);
            std::vector<Frame> received_data;
            if (drop_first_data && first_data) {
                first_data = false;
            } else {
                first_data = false;
                received_data = decode_burst(impair_burst(
                    data_wave, noise_std, clock_ppm,
                    seed ^ static_cast<uint32_t>(result.data_attempts * 17u)));
            }

            std::optional<Frame> control_frame;
            for (const auto& frame : received_data) {
                const auto rx = receiver.on_frame(frame);
                if (rx.duplicate) ++result.duplicate_frames;
                if (rx.ignored) ++result.ignored_frames;
                if (rx.response) control_frame = rx.response;
                result.received += rx.delivered;
            }
            if (received_data.empty()) control_frame = receiver.timeout_nack();

            bool control_seen = false;
            if (control_frame) {
                ++result.control_attempts;
                if (drop_first_control && first_control) {
                    first_control = false;
                } else {
                    first_control = false;
                    const auto control_wave =
                        control_transmitter.transmit_frame(*control_frame);
                    const auto decoded_control = decode_burst(impair_burst(
                        control_wave, noise_std, clock_ppm,
                        seed ^ static_cast<uint32_t>(0x9000u +
                                                     result.control_attempts * 31u)));
                    for (const auto& decoded : decoded_control) {
                        control_seen = true;
                        const auto acked = sender.on_control(decoded);
                        if (acked) advanced = true;
                    }
                }
            }
            if (!control_seen) sender.note_timeout();
            if (advanced) break;
            if (attempt + 1 < max_attempts) {
                sender.note_retry();
                const auto backoff = sender.backoff_ms(
                    static_cast<uint8_t>(attempt + 1));
                result.max_backoff_ms = std::max(result.max_backoff_ms, backoff);
            }
        }
        if (!advanced) return result;
    }
    result.retransmissions = sender.stats().retransmissions;
    result.negative_acknowledgements =
        sender.stats().negative_acknowledgements;
    result.max_backoff_ms =
        std::max(result.max_backoff_ms, sender.stats().max_backoff_ms);
    result.ok = result.received == text;
    return result;
}

} // namespace fectty
