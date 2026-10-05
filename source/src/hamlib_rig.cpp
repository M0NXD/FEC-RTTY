#include "fectty/hamlib_rig.hpp"
#ifdef FECTTY_HAS_HAMLIB
namespace fectty {
bool HamlibRigControl::connect(){disconnect();rig_=rig_init(model_);if(!rig_)return false;if(!path_.empty())rig_set_conf(rig_,rig_token_lookup(rig_,"rig_pathname"),path_.c_str());if(rig_open(rig_)!=RIG_OK){rig_cleanup(rig_);rig_=nullptr;return false;}state_.connected=true;return true;}
void HamlibRigControl::disconnect(){if(rig_){rig_close(rig_);rig_cleanup(rig_);rig_=nullptr;}state_={};}
bool HamlibRigControl::set_frequency(uint64_t f){if(!rig_||rig_set_freq(rig_,RIG_VFO_CURR,f)!=RIG_OK)return false;state_.frequency_hz=f;return true;}
bool HamlibRigControl::set_mode(RigMode m){rmode_t x=m==RigMode::LSB?RIG_MODE_LSB:m==RigMode::DataUSB?RIG_MODE_PKTUSB:m==RigMode::DataLSB?RIG_MODE_PKTLSB:RIG_MODE_USB;if(!rig_||rig_set_mode(rig_,RIG_VFO_CURR,x,RIG_PASSBAND_NOCHANGE)!=RIG_OK)return false;state_.mode=m;return true;}
bool HamlibRigControl::set_ptt(PttMode m){ptt_t p=m==PttMode::Off?RIG_PTT_OFF:RIG_PTT_ON;if(!rig_||rig_set_ptt(rig_,RIG_VFO_CURR,p)!=RIG_OK)return false;state_.transmitting=m!=PttMode::Off;return true;}
}
#endif
