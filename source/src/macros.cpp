#include "fectty/macros.hpp"
namespace fectty {std::string MacroSet::expand(const std::string&n,const std::map<std::string,std::string>&vars)const{auto i=m_.find(n);if(i==m_.end())return {};std::string s=i->second;for(auto&[k,v]:vars){std::string p="{"+k+"}";size_t q=0;while((q=s.find(p,q))!=std::string::npos){s.replace(q,p.size(),v);q+=v.size();}}return s;}}
