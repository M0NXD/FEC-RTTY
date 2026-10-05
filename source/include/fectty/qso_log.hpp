#pragma once
#include <string>
namespace fectty {struct QsoRecord{std::string utc,call,frequency,mode="FECTTY",sent,received,notes;};bool append_adif(const QsoRecord&,const std::string&path);}
