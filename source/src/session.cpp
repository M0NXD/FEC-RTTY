#include "fectty/session.hpp"
#include "fectty/acquisition.hpp"
#include "fectty/convolutional.hpp"
#include "fectty/frame.hpp"
#include "fectty/frame_receiver.hpp"
#include "fectty/interleaver.hpp"
#include "fectty/sync.hpp"
#include "fectty/viterbi.hpp"
#include <algorithm>
namespace fectty {
static std::vector<uint8_t> concat(std::span<const uint8_t>a,std::span<const uint8_t>b){std::vector<uint8_t>o(a.begin(),a.end());o.insert(o.end(),b.begin(),b.end());return o;}
ModemTransmitStream::ModemTransmitStream(std::string_view text, FskConfig config)
 :modem_(config),text_(text),block_(text.empty()?std::vector<float>{}:acquisition_waveform(config)){}
std::vector<float> ModemTransmitStream::next(size_t max_samples){
 if(max_samples==0)return {};
 std::vector<float> out;
 out.reserve(max_samples);
 while(out.size()<max_samples){
  if(sample_offset_==block_.size()){
   if(text_offset_==text_.size())break;
   Frame frame;frame.sequence=sequence_++&15;
   const size_t count=std::min<size_t>(8,text_.size()-text_offset_);
   frame.payload.assign(text_.begin()+text_offset_,text_.begin()+text_offset_+count);
   text_offset_+=count;
   block_=modem_.transmit_frame(frame,false);
   sample_offset_=0;
  }
  const size_t count=std::min(max_samples-out.size(),block_.size()-sample_offset_);
  out.insert(out.end(),block_.begin()+sample_offset_,block_.begin()+sample_offset_+count);
  sample_offset_+=count;
 }
 return out;
}
std::vector<float> ModemSession::transmit_frame(const Frame& frame,bool with_acquisition) const {auto raw=encode_frame(frame);if(raw.empty())return {};auto bits=bytes_to_bits(raw);auto coded=interleave(convolutional_encode(bits));auto all=concat(sync_bits(),coded);auto out=with_acquisition?acquisition_waveform(cfg_):std::vector<float>{};auto a=fsk4_modulate(all,cfg_);out.insert(out.end(),a.begin(),a.end());return out;}
std::vector<float> ModemSession::transmit(std::string_view text){std::vector<float> out=acquisition_waveform(cfg_);for(size_t p=0;p<text.size();p+=8){Frame f;f.sequence=tx_seq_++&15;size_t n=std::min<size_t>(8,text.size()-p);f.payload.assign(text.begin()+p,text.begin()+p+n);auto a=transmit_frame(f,false);out.insert(out.end(),a.begin(),a.end());}return out;}
std::string ModemSession::receive_known_frames(std::span<const float> audio,const std::vector<size_t>& sizes,double off){std::string text;auto c=cfg_;for(auto&t:c.tones)t+=off;stats_.last_frequency_offset_hz=off;size_t sps=size_t(c.sample_rate/c.symbol_rate),pos=25*sps;for(size_t n:sizes){size_t rawbits=(n+4)*8,codedbits=(rawbits+6)*2,totalbits=32+codedbits,symbols=(totalbits+1)/2,samples=symbols*sps;if(pos+samples>audio.size())break;auto soft=fsk4_demodulate(audio.subspan(pos,samples),totalbits,c);if(sync_distance(soft)>4){++stats_.crc_failures;pos+=samples;continue;}std::vector<int8_t> payloadsoft(soft.begin()+32,soft.end());auto db=viterbi_decode(deinterleave_soft(payloadsoft));auto bytes=bits_to_bytes(db);bytes.resize(n+4);auto f=decode_frame(bytes);if(!f){++stats_.crc_failures;}else{if(have_rx_seq_&&f->sequence!=expected_rx_seq_)++stats_.sequence_gaps;expected_rx_seq_=(f->sequence+1)&15;have_rx_seq_=true;if(is_plain_text_frame(*f))text.append(f->payload.begin(),f->payload.end());++stats_.frames_ok;}pos+=samples;}return text;}
std::string ModemSession::receive(std::span<const float> audio,double off){std::string text;auto c=cfg_;for(auto&t:c.tones)t+=off;stats_.last_frequency_offset_hz=off;size_t sps=size_t(c.sample_rate/c.symbol_rate),pos=25*sps;while(pos<audio.size()){auto f=decode_air_frame(audio.subspan(pos),c);if(!f){++stats_.crc_failures;break;}if(have_rx_seq_&&f->frame.sequence!=expected_rx_seq_)++stats_.sequence_gaps;expected_rx_seq_=(f->frame.sequence+1)&15;have_rx_seq_=true;if(is_plain_text_frame(f->frame))text.append(f->frame.payload.begin(),f->frame.payload.end());++stats_.frames_ok;pos+=f->samples_consumed;}return text;}

}
