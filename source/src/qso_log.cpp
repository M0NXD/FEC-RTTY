#include "fectty/qso_log.hpp"
#include "fectty/paths.hpp"
#include <fstream>
namespace fectty {bool append_adif(const QsoRecord&r,const std::string&p){std::ofstream f(utf8_file_path(p),std::ios::app);if(!f)return false;auto w=[&](const char*n,const std::string&v){if(!v.empty())f<<"<"<<n<<":"<<v.size()<<">"<<v<<" ";};w("CALL",r.call);w("FREQ",r.frequency);w("MODE",r.mode);w("RST_SENT",r.sent);w("RST_RCVD",r.received);w("COMMENT",r.notes);f<<"<EOR>\n";f.close();return !f.fail();}}
