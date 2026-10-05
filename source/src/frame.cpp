#include "fectty/frame.hpp"
#include "fectty/crc16.hpp"
namespace fectty {
std::vector<uint8_t> encode_frame(const Frame& f){
 if(f.payload.size()>8) return {};
 std::vector<uint8_t> o;
 o.push_back(uint8_t(((f.version&3)<<6)|((uint8_t(f.type)&3)<<4)|(f.flags&15)));
 o.push_back(uint8_t(((f.sequence&15)<<4)|(f.payload.size()&15)));
 o.insert(o.end(),f.payload.begin(),f.payload.end());
 auto c=crc16_ccitt_false(o); o.push_back(uint8_t(c>>8)); o.push_back(uint8_t(c)); return o;
}
std::optional<Frame> decode_frame(std::span<const uint8_t> b){
 if(b.size()<4) return std::nullopt;
 size_t n=b[1]&15;
 if(n>8||b.size()!=n+4) return std::nullopt;
 auto c=crc16_ccitt_false(b.first(b.size()-2)); uint16_t got=uint16_t(b[b.size()-2])<<8|b.back(); if(c!=got)return std::nullopt;
 Frame f; f.version=(b[0]>>6)&3; f.type=FrameType((b[0]>>4)&3); f.flags=b[0]&15; f.sequence=(b[1]>>4)&15; f.payload.assign(b.begin()+2,b.begin()+2+n); return f;
}
std::vector<uint8_t> bytes_to_bits(std::span<const uint8_t>b){std::vector<uint8_t>v;v.reserve(b.size()*8);for(auto x:b)for(int i=7;i>=0;--i)v.push_back((x>>i)&1);return v;}
std::vector<uint8_t> bits_to_bytes(std::span<const uint8_t>b){std::vector<uint8_t>v((b.size()+7)/8);for(size_t i=0;i<b.size();++i)v[i/8]|=(b[i]&1)<<(7-(i%8));return v;}
}
