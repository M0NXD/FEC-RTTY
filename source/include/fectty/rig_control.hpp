#pragma once
#include <cstdint>
#include <string>
namespace fectty {
enum class PttMode { Off, Data, Mic };
enum class RigMode { Unknown, USB, LSB, DataUSB, DataLSB };
struct RigState { bool connected=false; bool transmitting=false; uint64_t frequency_hz=0; RigMode mode=RigMode::Unknown; };
class IRigControl { public: virtual ~IRigControl()=default; virtual bool connect()=0; virtual void disconnect()=0; virtual RigState state() const=0; virtual bool set_frequency(uint64_t)=0; virtual bool set_mode(RigMode)=0; virtual bool set_ptt(PttMode)=0; };
class NullRigControl final: public IRigControl { RigState s_{}; public: bool connect() override{s_.connected=true;return true;} void disconnect() override{s_={};} RigState state() const override{return s_;} bool set_frequency(uint64_t hz) override{s_.frequency_hz=hz;return true;} bool set_mode(RigMode m) override{s_.mode=m;return true;} bool set_ptt(PttMode m) override{s_.transmitting=m!=PttMode::Off;return true;} };
}
