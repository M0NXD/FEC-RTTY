#pragma once
#include "fectty/fsk4.hpp"
#include "fectty/frame.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace fectty {
struct SessionStats { size_t frames_ok=0,crc_failures=0,sequence_gaps=0; double last_frequency_offset_hz=0; };
class ModemSession { FskConfig cfg_; uint8_t tx_seq_=0,expected_rx_seq_=0;bool have_rx_seq_=false;SessionStats stats_{};public:
 explicit ModemSession(FskConfig c={}):cfg_(c){}
 std::vector<float> transmit(std::string_view text);
 std::vector<float> transmit_frame(const Frame& frame,bool with_acquisition=true) const;
 std::string receive_known_frames(std::span<const float> audio,const std::vector<size_t>& payload_sizes,double frequency_offset_hz=0);
 std::string receive(std::span<const float> audio,double frequency_offset_hz=0);
 const SessionStats& stats()const{return stats_;} void reset_stats(){stats_={};have_rx_seq_=false;}
};
// Generates the identical continuous burst while retaining at most one frame
// of audio, even for long pasted messages.
class ModemTransmitStream {
 ModemSession modem_;
 std::string text_;
 std::vector<float> block_;
 size_t text_offset_=0, sample_offset_=0;
 uint8_t sequence_=0;
public:
 explicit ModemTransmitStream(std::string_view text, FskConfig config={});
 std::vector<float> next(size_t max_samples=2048);
};
}
