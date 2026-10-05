#pragma once
#include "fectty/rig_control.hpp"
#include <cstdint>
#include <string>
namespace fectty {
class NetRigctlControl final: public IRigControl {
 std::string host_; uint16_t port_; std::intptr_t sock_=-1; mutable RigState state_{};
 bool command(const std::string& cmd,std::string* reply=nullptr) const;
public:
 explicit NetRigctlControl(std::string host="127.0.0.1",uint16_t port=4532):host_(std::move(host)),port_(port){}
 ~NetRigctlControl() override{disconnect();}
 bool connect() override; void disconnect() override; RigState state() const override;
 bool set_frequency(uint64_t) override; bool set_mode(RigMode) override; bool set_ptt(PttMode) override;
};
}
