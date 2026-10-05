#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace fectty {
std::vector<uint8_t> interleave(std::span<const uint8_t> bits, size_t columns=16);
std::vector<int8_t> deinterleave_soft(std::span<const int8_t> bits, size_t columns=16);
}
