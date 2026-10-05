#include "fectty/arq.hpp"

#include <algorithm>
#include <limits>

namespace fectty {
namespace {

bool valid_control_code(uint8_t code) {
    return code >= static_cast<uint8_t>(ArqControlCode::Ack) &&
           code <= static_cast<uint8_t>(ArqControlCode::Done);
}

bool station_matches(uint8_t expected, uint8_t actual) {
    return expected == kBroadcastStation || expected == actual;
}

bool destination_matches(uint8_t expected, uint8_t actual) {
    return expected == kBroadcastStation || actual == kBroadcastStation ||
           expected == actual;
}

uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

} // namespace

std::optional<ArqData> decode_arq_data(const Frame& frame) {
    if (frame.type != FrameType::Text ||
        (frame.flags & kArqAddressedFlag) == 0 ||
        frame.payload.size() < kArqEnvelopeBytes ||
        frame.payload.size() > kMaxPayload) {
        return std::nullopt;
    }
    ArqData data;
    data.destination = frame.payload[0];
    data.source = frame.payload[1];
    data.transfer_id = frame.payload[2];
    data.chunk_index = frame.payload[3];
    data.total_chunks = frame.payload[4];
    if (data.total_chunks == 0 || data.chunk_index >= data.total_chunks) {
        return std::nullopt;
    }
    data.payload.assign(frame.payload.begin() + kArqEnvelopeBytes,
                        frame.payload.end());
    return data;
}

std::optional<ArqControl> decode_arq_control(const Frame& frame) {
    if (frame.type != FrameType::Control ||
        (frame.flags & kArqAddressedFlag) == 0 || frame.payload.size() != 5 ||
        !valid_control_code(frame.payload[0])) {
        return std::nullopt;
    }
    ArqControl control;
    control.code = static_cast<ArqControlCode>(frame.payload[0]);
    control.destination = frame.payload[1];
    control.source = frame.payload[2];
    control.transfer_id = frame.payload[3];
    control.chunk_index = frame.payload[4];
    control.final_flag = (frame.flags & kArqFinalFlag) != 0;
    return control;
}

std::optional<Frame> make_arq_data_frame(const ArqData& data, uint8_t sequence) {
    if (data.total_chunks == 0 || data.chunk_index >= data.total_chunks ||
        data.payload.size() > kArqMaxDataBytes) {
        return std::nullopt;
    }
    Frame frame;
    frame.type = FrameType::Text;
    frame.flags = kArqAddressedFlag | (data.final() ? kArqFinalFlag : 0);
    frame.sequence = sequence & 0x0f;
    frame.payload = {data.destination, data.source, data.transfer_id,
                     data.chunk_index, data.total_chunks};
    frame.payload.insert(frame.payload.end(), data.payload.begin(),
                         data.payload.end());
    return frame;
}

std::optional<Frame> make_arq_control_frame(const ArqControl& control,
                                             bool final,
                                             uint8_t sequence) {
    if (!valid_control_code(static_cast<uint8_t>(control.code))) {
        return std::nullopt;
    }
    Frame frame;
    frame.type = FrameType::Control;
    frame.flags = kArqAddressedFlag | (final ? kArqFinalFlag : 0);
    frame.sequence = sequence & 0x0f;
    frame.payload = {static_cast<uint8_t>(control.code), control.destination,
                     control.source, control.transfer_id, control.chunk_index};
    return frame;
}

std::optional<ArqHello> decode_arq_hello(const Frame& frame) {
    const auto control = decode_arq_control(frame);
    if (!control || control->code != ArqControlCode::Hello) {
        return std::nullopt;
    }
    return ArqHello{control->destination, control->source,
                    control->transfer_id, control->chunk_index};
}

std::optional<Frame> make_arq_hello_frame(uint8_t destination,
                                           uint8_t source,
                                           uint8_t sequence) {
    return make_arq_control_frame(
        ArqControl{ArqControlCode::Hello, destination, source,
                   kProtocolVersion, kArqCapabilityMask},
        false, sequence);
}

bool ArqReceiver::station_matches(uint8_t expected, uint8_t actual) const {
    return ::fectty::station_matches(expected, actual);
}

std::optional<Frame> ArqReceiver::response(ArqControlCode code,
                                             uint8_t source,
                                             uint8_t transfer_id,
                                             uint8_t chunk_index,
                                             bool final) const {
    return make_arq_control_frame(
        ArqControl{code, source, config_.local_station_id, transfer_id,
                   chunk_index},
        final, chunk_index);
}

ArqReceiveResult ArqReceiver::on_frame(const Frame& frame) {
    ArqReceiveResult result;
    const auto data = decode_arq_data(frame);
    if (!data) {
        result.ignored = true;
        return result;
    }
    if (!destination_matches(config_.local_station_id, data->destination) ||
        !station_matches(config_.peer_station_id, data->source)) {
        result.ignored = true;
        return result;
    }

    if (!active_ && have_completed_ &&
        data->source == completed_source_ &&
        data->transfer_id == completed_transfer_id_ &&
        data->chunk_index == completed_chunk_) {
        result.duplicate = true;
        result.response = completed_ack_;
        return result;
    }

    if (active_ && (data->source != active_source_ ||
                    data->transfer_id != active_transfer_id_)) {
        result.busy = true;
        result.response = response(ArqControlCode::Busy, data->source,
                                   active_transfer_id_, expected_chunk_);
        return result;
    }

    if (!active_) {
        if (data->chunk_index != 0) {
            result.response = response(ArqControlCode::Nack, data->source,
                                       data->transfer_id, 0);
            return result;
        }
        active_ = true;
        active_source_ = data->source;
        active_transfer_id_ = data->transfer_id;
        expected_chunk_ = 0;
        total_chunks_ = data->total_chunks;
        assembled_.clear();
    }

    if (data->total_chunks != total_chunks_) {
        result.response = response(ArqControlCode::Nack, data->source,
                                   data->transfer_id, expected_chunk_);
        return result;
    }
    if (data->chunk_index < expected_chunk_) {
        result.duplicate = true;
        result.response = response(ArqControlCode::Ack, data->source,
                                   data->transfer_id, data->chunk_index,
                                   data->final());
        return result;
    }
    if (data->chunk_index > expected_chunk_) {
        result.response = response(ArqControlCode::Nack, data->source,
                                   data->transfer_id, expected_chunk_);
        return result;
    }

    assembled_.append(data->payload.begin(), data->payload.end());
    result.accepted = true;
    const bool final = data->chunk_index + 1u == data->total_chunks;
    result.response = response(ArqControlCode::Ack, data->source,
                               data->transfer_id, data->chunk_index, final);
    if (final) {
        result.delivered = assembled_;
        assembled_.clear();
        active_ = false;
        have_completed_ = true;
        completed_source_ = data->source;
        completed_transfer_id_ = data->transfer_id;
        completed_chunk_ = data->chunk_index;
        completed_ack_ = result.response;
    } else {
        ++expected_chunk_;
    }
    return result;
}

std::optional<Frame> ArqReceiver::timeout_nack() const {
    if (!active_) return std::nullopt;
    return response(ArqControlCode::Nack, active_source_, active_transfer_id_,
                    expected_chunk_);
}

void ArqReceiver::reset() {
    active_ = false;
    active_source_ = 0;
    active_transfer_id_ = 0;
    expected_chunk_ = 0;
    total_chunks_ = 0;
    assembled_.clear();
    have_completed_ = false;
    completed_source_ = 0;
    completed_transfer_id_ = 0;
    completed_chunk_ = 0;
    completed_ack_.reset();
}

ArqSender::ArqSender(std::string_view text, ArqConfig config,
                     uint8_t transfer_id)
    : config_(config), transfer_id_(transfer_id) {
    if (text.size() > kArqMaxMessageBytes) {
        valid_ = false;
        return;
    }
    const size_t max_transfer_bytes =
        kArqMaxDataBytes * kArqMaxChunksPerTransfer;
    const size_t transfer_count = text.empty()
                                      ? 1
                                      : (text.size() + max_transfer_bytes - 1) /
                                            max_transfer_bytes;
    chunks_.reserve((text.size() + kArqMaxDataBytes - 1) /
                        kArqMaxDataBytes +
                    (text.empty() ? 1 : 0));

    size_t offset = 0;
    for (size_t transfer = 0; transfer < transfer_count; ++transfer) {
        const size_t remaining = text.size() - offset;
        const size_t transfer_bytes =
            text.empty() ? 0 : std::min(max_transfer_bytes, remaining);
        const size_t total = transfer_bytes == 0
                                 ? 1
                                 : (transfer_bytes + kArqMaxDataBytes - 1) /
                                       kArqMaxDataBytes;
        const uint8_t segment_id = static_cast<uint8_t>(
            static_cast<unsigned>(transfer_id_) +
            static_cast<unsigned>(transfer));
        for (size_t chunk = 0; chunk < total; ++chunk) {
            const size_t begin = offset + chunk * kArqMaxDataBytes;
            const size_t count = begin < text.size()
                                     ? std::min(kArqMaxDataBytes,
                                                text.size() - begin)
                                     : 0;
            ArqData data;
            data.destination = config_.peer_station_id;
            data.source = config_.local_station_id;
            data.transfer_id = segment_id;
            data.chunk_index = static_cast<uint8_t>(chunk);
            data.total_chunks = static_cast<uint8_t>(total);
            data.payload.assign(
                text.begin() +
                    static_cast<std::string_view::difference_type>(begin),
                text.begin() + static_cast<std::string_view::difference_type>(
                                   begin + count));
            chunks_.push_back(std::move(data));
        }
        offset += transfer_bytes;
    }
}

std::optional<Frame> ArqSender::current_frame() const {
    if (complete()) return std::nullopt;
    return make_arq_data_frame(chunks_[current_chunk_],
                               static_cast<uint8_t>(current_chunk_ & 0x0f));
}

bool ArqSender::on_control(const Frame& frame) {
    const auto control = decode_arq_control(frame);
    if (!control || complete() ||
        !station_matches(config_.local_station_id, control->destination) ||
        !station_matches(config_.peer_station_id, control->source) ||
        control->transfer_id != chunks_[current_chunk_].transfer_id ||
        control->chunk_index != chunks_[current_chunk_].chunk_index) {
        return false;
    }
    switch (control->code) {
    case ArqControlCode::Ack:
    case ArqControlCode::Done:
        ++stats_.acknowledgements;
        ++current_chunk_;
        return true;
    case ArqControlCode::Nack:
        ++stats_.negative_acknowledgements;
        return false;
    case ArqControlCode::Busy:
        ++stats_.busy_responses;
        return false;
    case ArqControlCode::Hello:
        return false;
    }
    return false;
}

uint32_t ArqSender::backoff_ms(uint8_t retry_number) {
    const uint8_t bounded = std::min<uint8_t>(retry_number, 5);
    const uint32_t window = 1u << bounded;
    const uint32_t input = config_.backoff_seed ^
                           (static_cast<uint32_t>(config_.local_station_id) << 24) ^
                           (static_cast<uint32_t>(config_.peer_station_id) << 16) ^
                           (static_cast<uint32_t>(
                                chunks_.empty()
                                    ? transfer_id_
                                    : chunks_[current_chunk_].transfer_id)
                            << 8) ^
                           static_cast<uint32_t>(current_chunk_) ^
                           static_cast<uint32_t>(retry_number);
    const uint32_t slots = (mix32(input) % window) + 1u;
    const uint32_t bounded_slots = std::min<uint32_t>(slots, 32u);
    const uint64_t result = static_cast<uint64_t>(config_.slot_ms) * bounded_slots;
    const auto bounded_result = static_cast<uint32_t>(std::min<uint64_t>(
        result, std::numeric_limits<uint32_t>::max()));
    stats_.max_backoff_ms = std::max(stats_.max_backoff_ms, bounded_result);
    return bounded_result;
}

} // namespace fectty
