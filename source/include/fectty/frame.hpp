#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace fectty {
enum class FrameType:uint8_t { Text=0, Control=1, Id=2, Reserved=3 };
struct Frame { uint8_t version=0; FrameType type=FrameType::Text; uint8_t flags=0, sequence=0; std::vector<uint8_t> payload; };
// Only the unaddressed v0 Text payload is operator text. Legacy envelopes and
// controls remain available through the decoded Frame API.
inline bool is_plain_text_frame(const Frame& frame) {
    return frame.version == 0 && frame.type == FrameType::Text && frame.flags == 0;
}
std::vector<uint8_t> encode_frame(const Frame& f);
std::optional<Frame> decode_frame(std::span<const uint8_t> bytes);
std::vector<uint8_t> bytes_to_bits(std::span<const uint8_t> bytes);
std::vector<uint8_t> bits_to_bytes(std::span<const uint8_t> bits);
}
