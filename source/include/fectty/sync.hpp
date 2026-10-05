#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace fectty {
constexpr uint32_t kSyncWord=0xD391C5A7u;
std::vector<uint8_t> sync_bits();
int sync_distance(std::span<const int8_t> soft,size_t bit_offset=0);
}
