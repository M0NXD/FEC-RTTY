#include "fectty/spectrum.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace fectty {
std::vector<SpectrumBin> spectrum(std::span<const float> x,double fs,size_t bins){
 if(x.empty()||bins<2) return {};
 const size_t n=std::min(x.size(),2*(bins-1));
 std::vector<SpectrumBin> out; out.reserve(bins);
 for(size_t k=0;k<bins;++k){
  double re=0,im=0;
  for(size_t i=0;i<n;++i){const double w=n>1?0.5-0.5*std::cos(2*std::numbers::pi*i/(n-1)):1;const double a=-2*std::numbers::pi*k*i/n;re+=x[i]*w*std::cos(a);im+=x[i]*w*std::sin(a);}
  const double p=(re*re+im*im)/(n*n+1e-30);out.push_back({double(k)*fs/n,10*std::log10(p+1e-15)});
 }
 return out;
}
}
