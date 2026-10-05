#pragma once
#include "fectty/rig_control.hpp"
#ifdef FECTTY_HAS_HAMLIB
#include <hamlib/rig.h>
namespace fectty {
class HamlibRigControl final: public IRigControl {RIG* rig_=nullptr; int model_; std::string path_; mutable RigState state_{};public:HamlibRigControl(int model,std::string path):model_(model),path_(std::move(path)){}~HamlibRigControl() override{disconnect();}bool connect() override;void disconnect() override;RigState state() const override{return state_;}bool set_frequency(uint64_t) override;bool set_mode(RigMode) override;bool set_ptt(PttMode) override;};
}
#endif
