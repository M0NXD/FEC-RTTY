#pragma once

#include "fectty/frame.hpp"
#include "fectty/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fectty {

// ARQ is deliberately a session layer over the one FEC-RTTY waveform.  It
// does not add a user-selectable modem mode.
constexpr uint8_t kArqAddressedFlag = kFlagAddressed;
constexpr uint8_t kArqFinalFlag = kFlagFinal;
constexpr uint8_t kBroadcastStation = 0xff;
constexpr size_t kArqEnvelopeBytes = 5;
constexpr size_t kArqMaxDataBytes = kMaxPayload - kArqEnvelopeBytes;
constexpr size_t kArqMaxChunksPerTransfer = 255;
constexpr size_t kArqMaxTransfersPerMessage = 255;
// A long application message is carried as a sequence of addressed ARQ
// transfers. Each segment gets its own transfer ID while retaining the same
// stop-and-wait framing and bounded receiver memory.
constexpr size_t kArqMaxMessageBytes =
    kArqMaxDataBytes * kArqMaxChunksPerTransfer *
    kArqMaxTransfersPerMessage;

// HELLO is deliberately an informational, plaintext capability advertisement.
// There is no key exchange, authentication, or encryption in this protocol.
constexpr uint8_t kArqCapabilityPlaintext = 0x01;
constexpr uint8_t kArqCapabilityFec = 0x02;
constexpr uint8_t kArqCapabilityInterleaving = 0x04;
constexpr uint8_t kArqCapabilityArq = 0x08;
constexpr uint8_t kArqCapabilityBroadcast = 0x10;
constexpr uint8_t kArqCapabilityMask = kArqCapabilityPlaintext |
                                        kArqCapabilityFec |
                                        kArqCapabilityInterleaving |
                                        kArqCapabilityArq |
                                        kArqCapabilityBroadcast;

enum class ArqControlCode : uint8_t {
    Ack = 1,
    Nack = 2,
    Busy = 3,
    Hello = 4,
    Done = 5
};

struct ArqConfig {
    uint8_t local_station_id = 1;
    uint8_t peer_station_id = 2;
    uint8_t max_retries = 4;
    uint16_t slot_ms = 250;
    uint32_t backoff_seed = 0xFEC77u;
};

struct ArqData {
    uint8_t destination = 0;
    uint8_t source = 0;
    uint8_t transfer_id = 0;
    uint8_t chunk_index = 0;
    uint8_t total_chunks = 0;
    std::vector<uint8_t> payload;

    bool final() const {
        return total_chunks != 0 &&
               static_cast<unsigned>(chunk_index) + 1u >= total_chunks;
    }
};

struct ArqControl {
    ArqControlCode code = ArqControlCode::Ack;
    uint8_t destination = 0;
    uint8_t source = 0;
    uint8_t transfer_id = 0;
    uint8_t chunk_index = 0;
    bool final_flag = false;

    bool final() const { return final_flag; }
};

std::optional<ArqData> decode_arq_data(const Frame& frame);
std::optional<ArqControl> decode_arq_control(const Frame& frame);
std::optional<Frame> make_arq_data_frame(const ArqData& data,
                                          uint8_t sequence = 0);
std::optional<Frame> make_arq_control_frame(const ArqControl& control,
                                             bool final = false,
                                             uint8_t sequence = 0);

// A HELLO encodes the fixed protocol version in transfer_id and the
// supported-feature bitmap in chunk_index. It is optional and never gates
// transmission; peers that do not implement it can still use ARQ directly.
struct ArqHello {
    uint8_t destination = 0;
    uint8_t source = 0;
    uint8_t protocol_version = kProtocolVersion;
    uint8_t capabilities = kArqCapabilityMask;
};

std::optional<ArqHello> decode_arq_hello(const Frame& frame);
std::optional<Frame> make_arq_hello_frame(uint8_t destination,
                                           uint8_t source,
                                           uint8_t sequence = 0);

struct ArqReceiveResult {
    std::optional<Frame> response;
    std::string delivered;
    bool accepted = false;
    bool duplicate = false;
    bool ignored = false;
    bool busy = false;
};

class ArqReceiver {
    ArqConfig config_;
    bool active_ = false;
    uint8_t active_source_ = 0;
    uint8_t active_transfer_id_ = 0;
    uint8_t expected_chunk_ = 0;
    uint8_t total_chunks_ = 0;
    std::string assembled_;
    bool have_completed_ = false;
    uint8_t completed_source_ = 0;
    uint8_t completed_transfer_id_ = 0;
    uint8_t completed_chunk_ = 0;
    std::optional<Frame> completed_ack_;

    bool station_matches(uint8_t expected, uint8_t actual) const;
    std::optional<Frame> response(ArqControlCode code,
                                  uint8_t source,
                                  uint8_t transfer_id,
                                  uint8_t chunk_index,
                                  bool final = false) const;

public:
    explicit ArqReceiver(ArqConfig config = {}) : config_(config) {}

    ArqReceiveResult on_frame(const Frame& frame);
    std::optional<Frame> timeout_nack() const;
    void reset();
    bool active() const { return active_; }
    uint8_t expected_chunk() const { return expected_chunk_; }
};

struct ArqSendStats {
    size_t acknowledgements = 0;
    size_t negative_acknowledgements = 0;
    size_t busy_responses = 0;
    size_t timeouts = 0;
    size_t retransmissions = 0;
    uint32_t max_backoff_ms = 0;
};

class ArqSender {
    ArqConfig config_;
    uint8_t transfer_id_ = 0;
    std::vector<ArqData> chunks_;
    size_t current_chunk_ = 0;
    bool valid_ = true;
    ArqSendStats stats_{};

public:
    ArqSender(std::string_view text, ArqConfig config = {},
              uint8_t transfer_id = 1);

    bool valid() const { return valid_; }
    bool complete() const { return valid_ && current_chunk_ >= chunks_.size(); }
    size_t total_chunks() const { return chunks_.size(); }
    size_t current_chunk() const { return current_chunk_; }
    size_t max_attempts() const {
        return static_cast<size_t>(config_.max_retries) + 1;
    }
    std::optional<Frame> current_frame() const;
    bool on_control(const Frame& frame);
    void note_timeout() { ++stats_.timeouts; }
    void note_retry() { ++stats_.retransmissions; }
    uint32_t backoff_ms(uint8_t retry_number);
    const ArqSendStats& stats() const { return stats_; }
};

} // namespace fectty
