#pragma once
#include "fectty/rig_control.hpp"
#include <chrono>
namespace fectty {
enum class TxState { Rx, PttRequest, Lead, Acquisition, Data, Idle, Tail, Abort };
struct TxTiming { std::chrono::milliseconds lead{150}, idle{750}, tail{100}; };
class TxController { IRigControl& rig_; TxTiming timing_; TxState state_=TxState::Rx; public: explicit TxController(IRigControl&r,TxTiming t={}):rig_(r),timing_(t){} TxState state()const{return state_;} bool begin(){if(state_!=TxState::Rx)return false;state_=TxState::PttRequest;if(!rig_.set_ptt(PttMode::Data)){state_=TxState::Rx;return false;}state_=TxState::Lead;return true;} void acquisition(){if(state_==TxState::Lead)state_=TxState::Acquisition;} void data(){state_=TxState::Data;} void idle(){if(state_==TxState::Data)state_=TxState::Idle;} void tail(){state_=TxState::Tail;} void finish(){rig_.set_ptt(PttMode::Off);state_=TxState::Rx;} void abort(){state_=TxState::Abort;rig_.set_ptt(PttMode::Off);state_=TxState::Rx;} const TxTiming& timing()const{return timing_;} };
}
