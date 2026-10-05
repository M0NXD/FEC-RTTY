#include "fectty/timing.hpp"
#include <cmath>
namespace fectty {
TimingResult find_symbol_timing(std::span<const float>x,size_t syms,const FskConfig&c){size_t sps=size_t(std::llround(c.sample_rate/c.symbol_rate));TimingResult best;for(size_t off=0;off<sps&&off+syms*sps<=x.size();off+=8){double q=0;for(size_t s=0;s<syms;s++){double e[4]{};for(int t=0;t<4;t++){double re=0,im=0,w=2*std::acos(-1.0)*c.tones[t]/c.sample_rate;for(size_t k=0;k<sps;k++){double a=w*k,v=x[off+s*sps+k];re+=v*std::cos(a);im-=v*std::sin(a);}e[t]=re*re+im*im;}double mx=e[0],sum=e[0];for(int t=1;t<4;t++){if(e[t]>mx)mx=e[t];sum+=e[t];}q+=sum>0?mx/sum:0;}q/=syms;if(q>best.score)best={off,q};}return best;}
}
