#include "fectty/channel.hpp"
#include <random>
namespace fectty {std::vector<float> apply_channel(std::span<const float>x,const ChannelConfig&c){std::mt19937 r(c.seed);std::normal_distribution<float> n(0,float(c.noise_std));std::vector<float>o(x.begin(),x.end());for(size_t i=0;i<o.size();++i){o[i]=float(o[i]*c.gain)+n(r);if(c.impulse_period&&i%c.impulse_period==0)o[i]+=float(c.impulse_amplitude);}return o;}}
