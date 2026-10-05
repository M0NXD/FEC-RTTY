#pragma once
#include "fectty/acquisition.hpp"
#include <span>
#include <optional>
#include <vector>
namespace fectty {
struct StreamLock{bool locked=false;size_t sample_index=0;double frequency_offset_hz=0;double score=0;};
class StreamReceiver {
 FskConfig cfg_;std::vector<float> buffer_;size_t consumed_=0;StreamLock lock_{};
public:
 explicit StreamReceiver(FskConfig c={}):cfg_(c){}
 std::optional<StreamLock> push(std::span<const float> samples);
 void reset(){buffer_.clear();consumed_=0;lock_={};}
 const StreamLock& lock()const{return lock_;}
};
}
