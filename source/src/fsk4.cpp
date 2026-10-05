#include "fectty/fsk4.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace fectty {
static int tone_for(uint8_t a,uint8_t b){ if(a==0&&b==0)return 0;if(a==0&&b==1)return 1;if(a==1&&b==1)return 2;return 3; }
static void bits_for(int t,uint8_t&a,uint8_t&b){static const uint8_t m[4][2]={{0,0},{0,1},{1,1},{1,0}};a=m[t][0];b=m[t][1];}
std::vector<float> fsk4_modulate(std::span<const uint8_t>b,const FskConfig&c){size_t sps=size_t(std::llround(c.sample_rate/c.symbol_rate));std::vector<float>o;o.reserve((b.size()/2)*sps);double ph=0,twopi=2*std::acos(-1.0);for(size_t i=0;i+1<b.size();i+=2){int t=tone_for(b[i],b[i+1]);double d=twopi*c.tones[t]/c.sample_rate;for(size_t k=0;k<sps;k++){o.push_back(float(std::sin(ph)));ph+=d;if(ph>twopi)ph-=twopi;}}return o;}
std::vector<int8_t> fsk4_demodulate(std::span<const float>x,size_t bit_count,const FskConfig&c){size_t sps=size_t(std::llround(c.sample_rate/c.symbol_rate)),syms=bit_count/2;std::vector<int8_t>o;o.reserve(syms*2);double twopi=2*std::acos(-1.0);std::array<std::vector<double>,4> cs, sn;for(int t=0;t<4;t++){cs[t].resize(sps);sn[t].resize(sps);double w=twopi*c.tones[t]/c.sample_rate;for(size_t k=0;k<sps;k++){double a=w*k;cs[t][k]=std::cos(a);sn[t][k]=std::sin(a);}}for(size_t s=0;s<syms&&((s+1)*sps)<=x.size();++s){double e[4]{};for(int t=0;t<4;t++){double re=0,im=0;for(size_t k=0;k<sps;k++){double v=x[s*sps+k];re+=v*cs[t][k];im-=v*sn[t][k];}e[t]=re*re+im*im;}double maxe=*std::max_element(e,e+4);if(maxe<1e-12)maxe=1;double score0[2]={-1e300,-1e300},score1[2]={-1e300,-1e300};for(int t=0;t<4;t++){uint8_t a,b;bits_for(t,a,b);score0[a]=std::max(score0[a],e[t]);score1[b]=std::max(score1[b],e[t]);}auto q=[&](double one,double zero){double v=(one-zero)/maxe*127.0;return int8_t(std::clamp(v,-127.0,127.0));};o.push_back(q(score0[1],score0[0]));o.push_back(q(score1[1],score1[0]));}return o;}
}
