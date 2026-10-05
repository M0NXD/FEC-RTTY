#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace fectty { std::vector<uint8_t> convolutional_encode(std::span<const uint8_t> bits, bool terminate=true); }
