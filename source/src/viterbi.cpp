#include "fectty/viterbi.hpp"
#include <array>
#include <algorithm>
#include <bit>
#include <limits>
namespace fectty {
static uint8_t parity(uint8_t x){return std::popcount(x)&1;}
static int cost(int8_t s,uint8_t bit){int target=bit?127:-127;int d=int(s)-target;return d*d;}
std::vector<uint8_t> viterbi_decode(std::span<const int8_t> soft,bool terminated){
 if(soft.size()%2)return {};
 size_t n=soft.size()/2;
 const int INF=std::numeric_limits<int>::max()/8;
 std::array<int,64> prev,cur; prev.fill(INF);prev[0]=0; std::vector<std::array<uint8_t,64>> ps(n),pb(n);
 for(size_t t=0;t<n;++t){cur.fill(INF);for(int st=0;st<64;++st)if(prev[st]<INF){for(int bit=0;bit<2;++bit){uint8_t reg=uint8_t(((st<<1)|bit)&0x7f), ns=reg&0x3f;uint8_t a=parity(reg&0171),b=parity(reg&0133);int m=prev[st]+cost(soft[2*t],a)+cost(soft[2*t+1],b);if(m<cur[ns]){cur[ns]=m;ps[t][ns]=st;pb[t][ns]=bit;}}}prev=cur;}
 int st=terminated?0:int(std::min_element(prev.begin(),prev.end())-prev.begin());std::vector<uint8_t> out(n);for(size_t t=n;t-->0;){out[t]=pb[t][st];st=ps[t][st];}if(terminated&&out.size()>=6)out.resize(out.size()-6);return out;
}
}
