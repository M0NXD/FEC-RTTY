#include "fectty/omnirig.hpp"
#ifdef _WIN32
#include <oleauto.h>
namespace fectty {
static bool dispid(IDispatch*d,const wchar_t*n,DISPID&i){LPOLESTR x=const_cast<LPOLESTR>(n);return SUCCEEDED(d->GetIDsOfNames(IID_NULL,&x,1,LOCALE_USER_DEFAULT,&i));}
bool OmniRigControl::get_dispatch(const wchar_t*n,IDispatch**out){*out=nullptr;DISPID id;if(!dispid(omni_,n,id))return false;DISPPARAMS p{};VARIANT v;VariantInit(&v);const auto result=omni_->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_PROPERTYGET,&p,&v,nullptr,nullptr);const bool ok=SUCCEEDED(result)&&v.vt==VT_DISPATCH&&v.pdispVal;if(ok){*out=v.pdispVal;v.vt=VT_EMPTY;}VariantClear(&v);return ok;}
bool OmniRigControl::put_long(const wchar_t*n,long val){DISPID id;if(!rig_||!dispid(rig_,n,id))return false;VARIANT v;VariantInit(&v);v.vt=VT_I4;v.lVal=val;DISPID named=DISPID_PROPERTYPUT;DISPPARAMS p{&v,&named,1,1};return SUCCEEDED(rig_->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_PROPERTYPUT,&p,nullptr,nullptr,nullptr));}
bool OmniRigControl::connect(){
 disconnect();
 const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return false;
 com_initialized_=SUCCEEDED(initialized);
 CLSID c;
 if(FAILED(CLSIDFromProgID(L"OmniRig.OmniRigX",&c))){disconnect();return false;}
 if(FAILED(CoCreateInstance(c,nullptr,CLSCTX_LOCAL_SERVER|CLSCTX_INPROC_SERVER,IID_IDispatch,(void**)&omni_))){disconnect();return false;}
 if(!get_dispatch(rig_number_==2?L"Rig2":L"Rig1",&rig_)){disconnect();return false;}
 state_.connected=true;
 return true;
}
void OmniRigControl::disconnect(){
 if(rig_){rig_->Release();rig_=nullptr;}
 if(omni_){omni_->Release();omni_=nullptr;}
 state_={};
 if(com_initialized_){CoUninitialize();com_initialized_=false;}
}
bool OmniRigControl::set_frequency(uint64_t f){if(!put_long(L"Freq",long(f)))return false;state_.frequency_hz=f;return true;}
bool OmniRigControl::set_mode(RigMode m){long v=m==RigMode::LSB?0x04000000:0x02000000;if(!put_long(L"Mode",v))return false;state_.mode=m;return true;}
bool OmniRigControl::set_ptt(PttMode m){if(!put_long(L"Tx",m==PttMode::Off?0:1))return false;state_.transmitting=m!=PttMode::Off;return true;}
}
#endif
