#pragma once
#include "fectty/rig_control.hpp"
#ifdef _WIN32
#include <windows.h>
namespace fectty {
class OmniRigControl final: public IRigControl {IDispatch* omni_=nullptr;IDispatch* rig_=nullptr;int rig_number_;bool com_initialized_=false;mutable RigState state_{};bool put_long(const wchar_t*,long);bool get_dispatch(const wchar_t*,IDispatch**);public:explicit OmniRigControl(int rig=1):rig_number_(rig){}~OmniRigControl()override{disconnect();}bool connect()override;void disconnect()override;RigState state()const override{return state_;}bool set_frequency(uint64_t)override;bool set_mode(RigMode)override;bool set_ptt(PttMode)override;};
}
#endif
