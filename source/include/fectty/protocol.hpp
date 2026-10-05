#pragma once
#include <cstddef>
#include <cstdint>
namespace fectty {
constexpr uint8_t kProtocolVersion=0;
constexpr size_t kMaxPayload=8;
// Low four-bit frame flags are reserved in the byte-level header. These flags
// belong to the transparent plaintext ARQ session layer; they do not select a
// second modem mode.
constexpr uint8_t kFlagAddressed=0x1;
constexpr uint8_t kFlagFinal=0x2;
struct ProtocolCapabilities { bool utf8=true; bool fec=true; bool interleaving=true; bool soft_decode=true; uint8_t protocol_version=kProtocolVersion; };
}
