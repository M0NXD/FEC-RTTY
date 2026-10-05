#pragma once
#include "fectty/rig_control.hpp"
#include <chrono>
namespace fectty {
class TxWatchdog { IRigControl& rig_; std::chrono::milliseconds limit_; bool armed_=false; std::chrono::steady_clock::time_point started_{}; public:
 TxWatchdog(IRigControl&r,std::chrono::milliseconds limit=std::chrono::seconds(180)):rig_(r),limit_(limit){}
 void arm(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()){armed_=true;started_=now;}
 void disarm(){armed_=false;} bool armed()const{return armed_;}
 bool poll(std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()){if(armed_&&now-started_>=limit_){rig_.set_ptt(PttMode::Off);armed_=false;return true;}return false;}
};
}
