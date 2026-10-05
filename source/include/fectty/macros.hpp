#pragma once
#include <map>
#include <string>
namespace fectty {
class MacroSet { std::map<std::string,std::string> m_; public: void set(std::string n,std::string v){m_[std::move(n)]=std::move(v);} std::string expand(const std::string&n,const std::map<std::string,std::string>&vars={})const; };
}
