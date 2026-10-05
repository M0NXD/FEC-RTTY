#include "fectty/diagnostics.hpp"
#include <sstream>
namespace fectty {std::string format_session_stats(const SessionStats&s){std::ostringstream o;o<<"frames_ok="<<s.frames_ok<<" crc_failures="<<s.crc_failures<<" sequence_gaps="<<s.sequence_gaps<<" afc_hz="<<s.last_frequency_offset_hz;return o.str();}}
