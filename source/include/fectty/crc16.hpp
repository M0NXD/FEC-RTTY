#pragma once
#include <cstdint>
#include <span>
namespace fectty { uint16_t crc16_ccitt_false(std::span<const uint8_t> data); }
