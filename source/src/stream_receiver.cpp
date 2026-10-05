#include "fectty/stream_receiver.hpp"
#include <cmath>
namespace fectty {
std::optional<StreamLock> StreamReceiver::push(std::span<const float>x){buffer_.insert(buffer_.end(),x.begin(),x.end());size_t sps=size_t(std::llround(cfg_.sample_rate/cfg_.symbol_rate));size_t pre=25*sps;if(buffer_.size()<pre)return std::nullopt;for(size_t off=0;off<sps&&off+pre<=buffer_.size();off+=8){auto a=acquire(std::span<const float>(buffer_).subspan(off,pre),cfg_);if(a.locked&&a.score>lock_.score){lock_={true,consumed_+off,a.frequency_offset_hz,a.score};return lock_;}}if(buffer_.size()>pre+sps){size_t drop=buffer_.size()-pre-sps;buffer_.erase(buffer_.begin(),buffer_.begin()+drop);consumed_+=drop;}return std::nullopt;}
}
