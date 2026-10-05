#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace fectty { std::vector<uint8_t> viterbi_decode(std::span<const int8_t> soft, bool terminated=true); }
