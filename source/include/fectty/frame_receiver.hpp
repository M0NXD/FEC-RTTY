#pragma once
#include "fectty/frame.hpp"
#include "fectty/fsk4.hpp"
#include <cstddef>
#include <optional>
#include <span>
namespace fectty {
struct DecodedAirFrame { Frame frame; size_t samples_consumed=0; int sync_errors=0; };
std::optional<DecodedAirFrame> decode_air_frame(std::span<const float> audio,const FskConfig& cfg={});
}
