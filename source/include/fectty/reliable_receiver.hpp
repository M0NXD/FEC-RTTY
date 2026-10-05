#pragma once
#include "fectty/frame.hpp"
#include "fectty/fsk4.hpp"
#include "fectty/session.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <vector>
namespace fectty {
enum class ReliableRxState {
 Search,
 Acquire,
 Locked,
 Frame,
 Track,
 Lost,
 Reacquire
};

const char* reliable_rx_state_name(ReliableRxState state);

struct ReliableRxStats {
 size_t acquisitions=0,reacquisitions=0,frames_ok=0,crc_failures=0,
        sequence_gaps=0,bytes_out=0,samples_buffered=0,dropped_samples=0;
 double last_frequency_offset_hz=0;
 ReliableRxState state=ReliableRxState::Search;
};

class ReliableReceiver {
 FskConfig cfg_;
 FskConfig tuned_;
 std::vector<float> buffer_;
 size_t acquisition_search_offset_=0;
 ReliableRxStats stats_{};
 bool ever_locked_=false;
 bool have_rx_sequence_=false;
 bool frame_failed_=false;
 bool frame_error_counted_=false;
 uint8_t expected_rx_sequence_=0;
 std::vector<Frame> decoded_frames_;

 std::string process(bool final_chunk);
public:
 explicit ReliableReceiver(FskConfig c={}):cfg_(c),tuned_(c){}
 std::string push(std::span<const float> samples);
 std::string finish();
 std::vector<Frame> take_frames();
 void reset();
 const ReliableRxStats& stats() const { return stats_; }
};
}
