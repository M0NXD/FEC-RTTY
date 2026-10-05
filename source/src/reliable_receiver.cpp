#include "fectty/reliable_receiver.hpp"

#include "fectty/acquisition.hpp"
#include "fectty/frame_receiver.hpp"
#include "fectty/sync.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace fectty {
namespace {

size_t samples_per_symbol(const FskConfig& config) {
    return size_t(std::llround(config.sample_rate / config.symbol_rate));
}

size_t preamble_samples(const FskConfig& config) {
    return 25 * samples_per_symbol(config);
}

size_t frame_samples(size_t payload_size, const FskConfig& config) {
    const size_t raw_bits = (payload_size + 4) * 8;
    const size_t coded_bits = (raw_bits + 6) * 2;
    const size_t total_bits = 32 + coded_bits;
    return ((total_bits + 1) / 2) * samples_per_symbol(config);
}

bool has_signal(std::span<const float> samples) {
    return std::any_of(samples.begin(), samples.end(), [](float sample) {
        return std::abs(sample) >= 0.001f;
    });
}

} // namespace

const char* reliable_rx_state_name(ReliableRxState state) {
    switch (state) {
    case ReliableRxState::Search: return "SEARCH";
    case ReliableRxState::Acquire: return "ACQUIRE";
    case ReliableRxState::Locked: return "LOCKED";
    case ReliableRxState::Frame: return "FRAME";
    case ReliableRxState::Track: return "TRACK";
    case ReliableRxState::Lost: return "LOST";
    case ReliableRxState::Reacquire: return "REACQUIRE";
    }
    return "UNKNOWN";
}

void ReliableReceiver::reset() {
    buffer_.clear();
    acquisition_search_offset_ = 0;
    stats_ = {};
    tuned_ = cfg_;
    ever_locked_ = false;
    have_rx_sequence_ = false;
    frame_failed_ = false;
    frame_error_counted_ = false;
    expected_rx_sequence_ = 0;
    decoded_frames_.clear();
}

std::vector<Frame> ReliableReceiver::take_frames() {
    auto frames = std::move(decoded_frames_);
    decoded_frames_.clear();
    return frames;
}

std::string ReliableReceiver::push(std::span<const float> samples) {
    // Large WAV/network callers must visit the same sliding search windows
    // as live audio callbacks. Searching just the start of one large buffer
    // could discard a later burst after noise before ever examining it.
    const size_t block_size = std::max<size_t>(1, 2 * samples_per_symbol(cfg_));
    std::string output;
    if (samples.empty()) return process(false);
    while (!samples.empty()) {
        const size_t count = std::min(block_size, samples.size());
        const auto block = samples.first(count);
        buffer_.insert(buffer_.end(), block.begin(), block.end());
        samples = samples.subspan(count);
        output += process(false);
    }
    return output;
}

std::string ReliableReceiver::finish() {
    return process(true);
}

std::string ReliableReceiver::process(bool final_chunk) {
    std::string output;
    const size_t sps = samples_per_symbol(cfg_);
    const size_t preamble = preamble_samples(cfg_);
    const size_t shortest_frame = frame_samples(0, cfg_);
    const size_t longest_frame = frame_samples(8, cfg_);
    const size_t acquisition_step = std::max<size_t>(1, sps / 16);
    const size_t sync_samples = 16 * sps;
    // Acquisition is committed only after sync arrives. Preserve that entire
    // window (plus a symbol for search-grid overlap), not only the preamble.
    const size_t acquisition_history = preamble + sync_samples + sps;
    auto update_buffered = [&]() {
        stats_.samples_buffered = buffer_.size();
    };
    auto discard_prefix = [&](size_t count) {
        count = std::min(count, buffer_.size());
        using Difference = std::vector<float>::difference_type;
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<Difference>(count));
        acquisition_search_offset_ -= std::min(acquisition_search_offset_, count);
    };
    auto decode_at = [&](size_t offset) -> std::optional<DecodedAirFrame> {
        if (offset >= buffer_.size()) return std::nullopt;
        const auto audio = std::span<const float>(buffer_).subspan(offset);
        if (auto decoded = decode_air_frame(audio, tuned_)) return decoded;
        if (final_chunk) {
            // Acquisition can land a few samples after the true start under
            // quantization/noise. At EOF, permit only that sub-symbol timing
            // uncertainty; CRC/FEC must still validate the complete frame.
            // Do not append a whole symbol or invent additional payload bits.
            for (size_t payload_size = 0; payload_size <= 8; ++payload_size) {
                const size_t required = frame_samples(payload_size, cfg_);
                if (required > audio.size() && required - audio.size() <= acquisition_step) {
                    std::vector<float> padded(audio.begin(), audio.end());
                    padded.resize(required, 0.0f);
                    return decode_air_frame(padded, tuned_);
                }
            }
        }
        return std::nullopt;
    };
    auto sync_candidate = [&](size_t offset) {
        if (offset > buffer_.size() || buffer_.size() - offset < sync_samples) {
            return false;
        }
        const auto soft = fsk4_demodulate(
            std::span<const float>(buffer_).subspan(offset, sync_samples),
            32, tuned_);
        return sync_distance(soft) <= 4;
    };

    while (true) {
        const bool need_acquisition =
            !ever_locked_ || stats_.state == ReliableRxState::Search ||
            stats_.state == ReliableRxState::Lost ||
            stats_.state == ReliableRxState::Reacquire;

        if (need_acquisition) {
            stats_.state = ever_locked_ ? ReliableRxState::Reacquire
                                         : ReliableRxState::Search;
            // A partial preamble can score well before its true start/sync
            // boundary has arrived. Do not commit a symbol grid without the
            // following sync word; otherwise short final frames can be lost.
            if (buffer_.size() < preamble + sync_samples) break;

            // Avoid repeatedly running the expensive tone search over a quiet
            // cable. Keep enough tail data for a preamble split across pushes.
            if (!has_signal(buffer_)) {
                const size_t keep = acquisition_history;
                if (buffer_.size() > keep) discard_prefix(buffer_.size() - keep);
                stats_.state = ever_locked_ ? ReliableRxState::Lost
                                             : ReliableRxState::Search;
                break;
            }

            stats_.state = ReliableRxState::Acquire;
            // Examine every newly eligible complete preamble+sync window.
            // Remember the next coarse position so retaining overlap does not
            // repeatedly rescan old noise on every small audio callback.
            const size_t max_offset = buffer_.size() - preamble - sync_samples;
            const size_t search_begin = acquisition_search_offset_;
            const size_t search_end = max_offset;
            // Coarse half-symbol spacing is sufficient to find the 25-symbol
            // pattern; refine only a detected candidate. Searching every 1/16
            // symbol across ordinary microphone/radio noise is too expensive.
            const size_t search_step = std::max<size_t>(1, sps / 2);
            bool found = false;
            size_t found_offset = 0;
            AcquisitionResult best{};
            for (size_t offset = search_begin; offset <= search_end;
                 offset += search_step) {
                acquisition_search_offset_ = offset + search_step;
                const auto candidate = acquire(
                    std::span<const float>(buffer_).subspan(offset, preamble), cfg_, 100, 5, 4);
                if (candidate.locked && (!found || candidate.score > best.score)) {
                    found = true;
                    found_offset = offset;
                    best = candidate;
                }
                if (search_end - offset < search_step) break;
            }
            if (found) {
                const size_t begin = found_offset > search_step ? found_offset - search_step : 0;
                const size_t end = std::min(search_end, found_offset + search_step);
                for (size_t offset = begin; offset <= end; offset += acquisition_step) {
                    const auto candidate = acquire(std::span<const float>(buffer_).subspan(offset, preamble), cfg_);
                    if (candidate.locked && candidate.score > best.score) { found_offset = offset; best = candidate; }
                    if (end - offset < acquisition_step) break;
                }
            }
            if (found) {
                // The coarse search can leave the frame a few samples short at
                // the end of a burst. Use the following sync word to refine
                // the actual sample boundary.
                const size_t refine_begin = found_offset > acquisition_step
                    ? found_offset - acquisition_step : 0;
                const size_t refine_end = std::min(
                    search_end, found_offset + acquisition_step);
                auto sync_config = cfg_;
                for (auto& tone : sync_config.tones) tone += best.frequency_offset_hz;
                int best_sync_errors = 32;
                double best_refinement_score = -1;
                for (size_t offset = refine_begin; offset <= refine_end; ++offset) {
                    if (offset + preamble + sync_samples > buffer_.size()) continue;
                    const auto soft = fsk4_demodulate(
                        std::span<const float>(buffer_).subspan(
                            offset + preamble, sync_samples), 32, sync_config);
                    const int errors = sync_distance(soft);
                    const auto refinement = acquire(std::span<const float>(buffer_).subspan(offset, preamble), sync_config, 0, 5);
                    if (errors < best_sync_errors ||
                        (errors == best_sync_errors && refinement.score > best_refinement_score)) {
                        best_sync_errors = errors;
                        best_refinement_score = refinement.score;
                        found_offset = offset;
                    }
                }
                if (best_sync_errors > 4) found = false;
            }
            if (!found) {
                const size_t keep = acquisition_history;
                if (buffer_.size() > keep) discard_prefix(buffer_.size() - keep);
                stats_.state = ever_locked_ ? ReliableRxState::Reacquire
                                             : ReliableRxState::Search;
                break;
            }

            // A validated acquisition field starts a new direct-text burst,
            // even when background noise means no quiet gap was detected.
            // Sequence continuity applies within that burst, not across sends.
            have_rx_sequence_ = false;
            ++stats_.acquisitions;
            if (ever_locked_) ++stats_.reacquisitions;
            ever_locked_ = true;
            frame_failed_ = false;
            frame_error_counted_ = false;
            stats_.last_frequency_offset_hz = best.frequency_offset_hz;
            tuned_ = cfg_;
            for (auto& tone : tuned_.tones) tone += best.frequency_offset_hz;
            discard_prefix(found_offset + preamble);
            acquisition_search_offset_ = 0;
            stats_.state = ReliableRxState::Locked;
            continue;
        }

        stats_.state = ReliableRxState::Frame;
        if (buffer_.size() < shortest_frame &&
            (!final_chunk || shortest_frame - buffer_.size() > acquisition_step)) {
            stats_.state = ReliableRxState::Locked;
            break;
        }

        // Waiting for the largest legal frame avoids repeatedly demodulating
        // an incomplete variable-length frame on every small audio callback.
        // A final flush is allowed to decode a shorter last frame.
        if (!final_chunk && buffer_.size() < longest_frame) {
            stats_.state = ReliableRxState::Locked;
            break;
        }

        std::optional<DecodedAirFrame> frame;
        if (!frame_failed_) frame = decode_at(0);
        if (frame) {
            if (have_rx_sequence_ && frame->frame.sequence != expected_rx_sequence_) {
                ++stats_.sequence_gaps;
            }
            expected_rx_sequence_ = uint8_t((frame->frame.sequence + 1) & 15);
            have_rx_sequence_ = true;
            if (is_plain_text_frame(frame->frame)) {
                output.append(frame->frame.payload.begin(), frame->frame.payload.end());
                stats_.bytes_out += frame->frame.payload.size();
            }
            decoded_frames_.push_back(frame->frame);
            ++stats_.frames_ok;
            discard_prefix(frame->samples_consumed);
            frame_failed_ = false;
            frame_error_counted_ = false;
            stats_.state = ReliableRxState::Track;
            continue;
        }
        if (!frame_failed_ && sync_candidate(0)) {
            ++stats_.crc_failures;
            frame_error_counted_ = true;
        }
        frame_failed_ = true;

        // A variable-length frame cannot be declared bad until enough audio
        // exists to contain the largest legal frame. At a live stream boundary
        // finish() provides the final decision point for a short last frame.
        // The current frame may be damaged, but the next frame can still be
        // found by its sync word. Frame starts remain on the acquired symbol
        // grid, so the legal frame-length candidates are sufficient here.
        std::optional<size_t> next_frame;
        for (size_t payload_size = 0; payload_size <= 8; ++payload_size) {
            const size_t offset = frame_samples(payload_size, cfg_);
            if (offset >= buffer_.size() ||
                buffer_.size() - offset < shortest_frame) {
                continue;
            }
            if (sync_candidate(offset) && decode_at(offset)) {
                next_frame = offset;
                break;
            }
        }
        if (next_frame) {
            if (!frame_error_counted_) ++stats_.crc_failures;
            ++stats_.reacquisitions;
            discard_prefix(*next_frame);
            frame_failed_ = false;
            frame_error_counted_ = false;
            stats_.state = ReliableRxState::Track;
            continue;
        }

        // A damaged last frame can be followed immediately by a fresh burst.
        // Try its legal end positions before the more expensive broad search.
        bool following_burst = false;
        for (size_t payload_size = 0; payload_size <= 8; ++payload_size) {
            const size_t offset = frame_samples(payload_size, cfg_);
            if (offset > buffer_.size() || buffer_.size() - offset < preamble + sync_samples) continue;
            const auto candidate = acquire(std::span<const float>(buffer_).subspan(offset, preamble), cfg_);
            if (candidate.locked) {
                auto candidate_config = cfg_;
                for (auto& tone : candidate_config.tones) tone += candidate.frequency_offset_hz;
                const auto sync = fsk4_demodulate(std::span<const float>(buffer_).subspan(offset + preamble, sync_samples), 32, candidate_config);
                if (sync_distance(sync) > 4) continue;
                discard_prefix(offset);
                have_rx_sequence_ = false;
                stats_.state = ReliableRxState::Reacquire;
                following_burst = true;
                break;
            }
        }
        if (following_burst) continue;

        // A new preamble may follow a gap, so give the acquisition state one
        // chance to recover before declaring the stream lost.
        size_t quiet_run = 0;
        bool signal_after_quiet = false;
        for (const float sample : buffer_) {
            if (std::abs(sample) < 0.001f) {
                ++quiet_run;
            } else {
                if (quiet_run >= sps) {
                    signal_after_quiet = true;
                    break;
                }
                quiet_run = 0;
            }
        }
        if (signal_after_quiet) {
            stats_.state = ReliableRxState::Reacquire;
            continue;
        }
        if (final_chunk) {
            // Once a real frame sync has been seen, a fresh complete burst
            // cannot begin before that frame's shortest legal end. Avoid
            // repeatedly scanning a damaged final frame as a new preamble.
            const size_t required = preamble + shortest_frame +
                                    (frame_error_counted_ ? shortest_frame : 0);
            if (buffer_.size() >= required) {
                stats_.state = ReliableRxState::Reacquire;
                continue;
            }
            stats_.state = ReliableRxState::Lost;
            break;
        }
        if (buffer_.size() >= 2 * longest_frame) {
            stats_.state = ReliableRxState::Reacquire;
            continue;
        }
        stats_.state = ReliableRxState::Locked;
        break;
    }

    update_buffered();
    return output;
}

} // namespace fectty
