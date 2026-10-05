#include "fectty/sync.hpp"
namespace fectty {
std::vector<uint8_t> sync_bits(){std::vector<uint8_t> b;for(int i=31;i>=0;--i)b.push_back((kSyncWord>>i)&1u);return b;}
int sync_distance(std::span<const int8_t>s,size_t p){if(p+32>s.size())return 32;int d=0;for(int i=0;i<32;i++){int hard=s[p+i]>=0;int want=(kSyncWord>>(31-i))&1u;d+=hard!=want;}return d;}
}
