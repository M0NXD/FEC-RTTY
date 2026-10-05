#include "fectty/clock_drift.hpp"
#include <cmath>
namespace fectty {
std::vector<float> resample_clock_error(std::span<const float>x,double ppm){double ratio=1.0+ppm*1e-6;if(x.empty()||!std::isfinite(ratio)||ratio<=0)return {};double count=std::floor(x.size()/ratio);if(!std::isfinite(count)||count>=double(std::vector<float>{}.max_size()))return {};size_t n=size_t(count);std::vector<float>o(n);for(size_t i=0;i<n;i++){double p=i*ratio;size_t j=size_t(p);double f=p-j;if(j+1<x.size())o[i]=float(x[j]*(1-f)+x[j+1]*f);else o[i]=x.back();}return o;}
}
