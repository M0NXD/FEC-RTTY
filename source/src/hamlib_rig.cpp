#include "fectty/hamlib_rig.hpp"
#ifdef FECTTY_HAS_HAMLIB
#include <algorithm>
#include <cmath>
namespace fectty {
std::vector<std::pair<int,std::string>> available_hamlib_models(){
 rig_set_debug(RIG_DEBUG_NONE);rig_load_all_backends();
 std::vector<std::pair<int,std::string>> models;
 rig_list_foreach([](const rig_caps* caps,rig_ptr_t context)->int{
  auto& result=*static_cast<std::vector<std::pair<int,std::string>>*>(context);
  result.emplace_back(caps->rig_model,std::string(caps->mfg_name)+" "+caps->model_name+" ["+std::to_string(caps->rig_model)+"]");return 1;
 },&models);
 std::sort(models.begin(),models.end(),[](const auto&a,const auto&b){return a.second<b.second;});return models;
}
bool HamlibRigControl::connect(){
 disconnect();rig_load_all_backends();rig_=rig_init(model_);if(!rig_)return false;
 // Without a backend getter Hamlib can report frontend transmit state instead
 // of observing the radio. Refuse that model before opening its port.
 if(!rig_->caps->get_freq||!rig_->caps->get_mode||!rig_->caps->get_ptt){rig_cleanup(rig_);rig_=nullptr;return false;}
 if(!path_.empty()&&rig_set_conf(rig_,rig_token_lookup(rig_,"rig_pathname"),path_.c_str())!=RIG_OK){disconnect();return false;}
 const auto speed=std::to_string(baud_);const auto token=rig_token_lookup(rig_,"serial_speed");
 if(token!=RIG_CONF_END&&rig_set_conf(rig_,token,speed.c_str())!=RIG_OK){disconnect();return false;}
 if(rig_open(rig_)!=RIG_OK){rig_cleanup(rig_);rig_=nullptr;return false;}
 // Setter-populated cache entries must never stand in for TX/RX or dial readback.
 if(rig_set_cache_timeout_ms(rig_,HAMLIB_CACHE_ALL,0)!=RIG_OK){disconnect();return false;}
 state_.connected=true;return true;
}
void HamlibRigControl::disconnect(){if(rig_){rig_close(rig_);rig_cleanup(rig_);rig_=nullptr;}state_={};}
bool HamlibRigControl::set_frequency(uint64_t f){if(!rig_||rig_set_freq(rig_,RIG_VFO_CURR,f)!=RIG_OK)return false;state_.frequency_hz=f;return true;}
bool HamlibRigControl::set_mode(RigMode m){if(m==RigMode::Unknown)return false;rmode_t x=m==RigMode::LSB?RIG_MODE_LSB:m==RigMode::DataUSB?RIG_MODE_PKTUSB:m==RigMode::DataLSB?RIG_MODE_PKTLSB:RIG_MODE_USB;if(!rig_||rig_set_mode(rig_,RIG_VFO_CURR,x,RIG_PASSBAND_NOCHANGE)!=RIG_OK)return false;state_.mode=m;return true;}
bool HamlibRigControl::set_ptt(PttMode m){ptt_t p=m==PttMode::Off?RIG_PTT_OFF:m==PttMode::Data?RIG_PTT_ON_DATA:m==PttMode::Mic?RIG_PTT_ON_MIC:RIG_PTT_ON;if(!rig_||rig_set_ptt(rig_,RIG_VFO_CURR,p)!=RIG_OK)return false;state_.transmitting=m!=PttMode::Off;return true;}
bool HamlibRigControl::read_state(RigState& out){freq_t f=0;rmode_t m=0;pbwidth_t width=0;ptt_t p=RIG_PTT_OFF;if(!rig_||rig_get_freq(rig_,RIG_VFO_CURR,&f)!=RIG_OK||!std::isfinite(f)||f<=0||f>=18446744073709551616.0||rig_get_mode(rig_,RIG_VFO_CURR,&m,&width)!=RIG_OK||rig_get_ptt(rig_,RIG_VFO_CURR,&p)!=RIG_OK||(p!=RIG_PTT_OFF&&p!=RIG_PTT_ON&&p!=RIG_PTT_ON_MIC&&p!=RIG_PTT_ON_DATA)){state_.connected=false;return false;}state_={true,p!=RIG_PTT_OFF,static_cast<uint64_t>(f),m==RIG_MODE_USB?RigMode::USB:m==RIG_MODE_LSB?RigMode::LSB:m==RIG_MODE_PKTUSB?RigMode::DataUSB:m==RIG_MODE_PKTLSB?RigMode::DataLSB:RigMode::Unknown};out=state_;return true;}
}
#endif
