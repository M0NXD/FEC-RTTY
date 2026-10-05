#include "fectty/timing_tracker.hpp"
#include "fectty/acquisition.hpp"
#include "fectty/clock_drift.hpp"
#include <cmath>
namespace fectty { TimingTrackResult compensate_clock_search(std::span<const float>x,double maxppm,double step){TimingTrackResult best;if(!std::isfinite(maxppm)||!std::isfinite(step)||maxppm<0||step<=0)return best;best.samples.assign(x.begin(),x.end());best.score=-1;for(double p=-maxppm;p<=maxppm+1e-9;p+=step){auto y=resample_clock_error(x,-p);auto a=acquire(y);if(a.score>best.score){best.score=a.score;best.estimated_ppm=p;best.samples=std::move(y);}}return best;} }
