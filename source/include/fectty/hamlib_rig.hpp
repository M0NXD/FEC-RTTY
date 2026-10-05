#pragma once
#include "fectty/rig_control.hpp"
#ifdef FECTTY_HAS_HAMLIB
#include <hamlib/rig.h>
#include <vector>
namespace fectty {
std::vector<std::pair<int,std::string>> available_hamlib_models();
class HamlibRigControl final: public IRigControl {RIG* rig_=nullptr; int model_; std::string path_; int baud_; mutable RigState state_{};public:HamlibRigControl(int model,std::string path,int baud=9600):model_(model),path_(std::move(path)),baud_(baud){}~HamlibRigControl() override{disconnect();}bool connect() override;void disconnect() override;RigState state() const override{return state_;}bool read_state(RigState&) override;bool set_frequency(uint64_t) override;bool set_mode(RigMode) override;bool set_ptt(PttMode) override;};
}
#endif
