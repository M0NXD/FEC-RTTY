#include "fectty/link_metrics.hpp"
#include <cmath>
namespace fectty {double noise_std_for_ebn0(double db,double p,double bps){double ebn0=std::pow(10.0,db/10.0);double eb=p/bps;double n0=eb/ebn0;return std::sqrt(n0/2.0);}}
