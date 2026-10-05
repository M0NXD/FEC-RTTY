#include "fectty/convolutional.hpp"
#include <bit>
namespace fectty {
static uint8_t parity(uint8_t x){return std::popcount(x)&1;}
std::vector<uint8_t> convolutional_encode(std::span<const uint8_t>b,bool term){std::vector<uint8_t>o;o.reserve((b.size()+(term?6:0))*2);uint8_t s=0;auto put=[&](uint8_t bit){s=uint8_t(((s<<1)|(bit&1))&0x7f);o.push_back(parity(s&0171));o.push_back(parity(s&0133));};for(auto x:b)put(x);if(term)for(int i=0;i<6;i++)put(0);return o;}
}
