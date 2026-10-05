#include "fectty/interleaver.hpp"
#include <algorithm>
namespace fectty {
std::vector<uint8_t> interleave(std::span<const uint8_t> in,size_t cols){if(!cols||in.empty())return {in.begin(),in.end()};size_t rows=(in.size()+cols-1)/cols;std::vector<uint8_t> out;out.reserve(in.size());for(size_t c=0;c<cols;c++)for(size_t r=0;r<rows;r++){size_t i=r*cols+c;if(i<in.size())out.push_back(in[i]);}return out;}
std::vector<int8_t> deinterleave_soft(std::span<const int8_t> in,size_t cols){if(!cols||in.empty())return {in.begin(),in.end()};size_t rows=(in.size()+cols-1)/cols;std::vector<int8_t> out(in.size());size_t p=0;for(size_t c=0;c<cols;c++)for(size_t r=0;r<rows;r++){size_t i=r*cols+c;if(i<in.size())out[i]=in[p++];}return out;}
}
