#include "fectty/level_meter.hpp"
#include <algorithm>
#include <cmath>
namespace fectty { LevelReading measure_level(std::span<const float>x,double ct){LevelReading r;if(x.empty())return r;double ss=0;for(float v:x){double a=std::abs(double(v));r.peak=std::max(r.peak,a);ss+=double(v)*v;if(a>=ct)++r.clipped_samples;}r.rms=std::sqrt(ss/x.size());auto db=[](double v){return v>1e-6?20*std::log10(v):-120.0;};r.peak_dbfs=db(r.peak);r.rms_dbfs=db(r.rms);r.clipping=r.clipped_samples!=0;return r;} }
