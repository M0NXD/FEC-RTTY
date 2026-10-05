#pragma once
#include <cstddef>
namespace fectty {
struct LinkMetrics { size_t frames=0, good=0, bytes=0; double frame_success_rate()const{return frames?static_cast<double>(good)/static_cast<double>(frames):0;} };
double noise_std_for_ebn0(double ebn0_db,double signal_power,double coded_bits_per_sample);
}
