#include "fectty/omnirig.hpp"
#ifdef _WIN32
#include <oleauto.h>
#include <limits>
namespace fectty {
namespace {
constexpr long rx=0x00200000, tx=0x00400000;
constexpr long usb=0x02000000, lsb=0x04000000, data_usb=0x08000000, data_lsb=0x10000000;
bool dispid(IDispatch* dispatch,const wchar_t* name,DISPID& id) {
    LPOLESTR text=const_cast<LPOLESTR>(name);
    return SUCCEEDED(dispatch->GetIDsOfNames(IID_NULL,&text,1,LOCALE_USER_DEFAULT,&id));
}
}
bool OmniRigControl::get_dispatch(const wchar_t* name,IDispatch** out) {
    *out=nullptr;
    DISPID id;
    if(!dispid(omni_,name,id))return false;
    DISPPARAMS parameters{};
    VARIANT value;VariantInit(&value);
    const auto result=omni_->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_PROPERTYGET,&parameters,&value,nullptr,nullptr);
    const bool ok=SUCCEEDED(result)&&value.vt==VT_DISPATCH&&value.pdispVal;
    if(ok){*out=value.pdispVal;value.vt=VT_EMPTY;}
    VariantClear(&value);
    return ok;
}
bool OmniRigControl::get_long(const wchar_t* name,long& out) {
    DISPID id;
    if(!rig_||!dispid(rig_,name,id))return false;
    DISPPARAMS parameters{};
    VARIANT value;VariantInit(&value);
    const auto result=rig_->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_PROPERTYGET,&parameters,&value,nullptr,nullptr);
    bool ok=SUCCEEDED(result)&&SUCCEEDED(VariantChangeType(&value,&value,0,VT_I4));
    if(ok)out=value.lVal;
    VariantClear(&value);
    return ok;
}
bool OmniRigControl::put_long(const wchar_t* name,long number) {
    DISPID id;
    if(!rig_||!dispid(rig_,name,id))return false;
    VARIANT value;VariantInit(&value);value.vt=VT_I4;value.lVal=number;
    DISPID named=DISPID_PROPERTYPUT;
    DISPPARAMS parameters{&value,&named,1,1};
    return SUCCEEDED(rig_->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_PROPERTYPUT,&parameters,nullptr,nullptr,nullptr));
}
bool OmniRigControl::connect() {
    disconnect();
    if(rig_number_!=1&&rig_number_!=2)return false;
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized))return false;
    com_initialized_=true;
    if(factory_)omni_=factory_(); // Test-only injected dispatch, same ownership/thread rules.
    else {
        CLSID clsid;
        if(FAILED(CLSIDFromProgID(L"OmniRig.OmniRigX",&clsid))||
           FAILED(CoCreateInstance(clsid,nullptr,CLSCTX_LOCAL_SERVER,IID_IDispatch,reinterpret_cast<void**>(&omni_)))) {
            disconnect();return false;
        }
    }
    if(!omni_||!get_dispatch(rig_number_==2?L"Rig2":L"Rig1",&rig_)){disconnect();return false;}
    RigState read;
    if(!read_state(read)){disconnect();return false;}
    return true;
}
void OmniRigControl::disconnect() {
    if(rig_){rig_->Release();rig_=nullptr;}
    if(omni_){omni_->Release();omni_=nullptr;}
    state_={};
    if(com_initialized_){CoUninitialize();com_initialized_=false;}
}
bool OmniRigControl::read_state(RigState& out) {
    long status=0,frequency=0,mode=0,ptt=0;
    if(!get_long(L"Status",status)||status!=4||!get_long(L"Freq",frequency)||frequency<=0||
       !get_long(L"Mode",mode)||!get_long(L"Tx",ptt)||(ptt!=rx&&ptt!=tx)) {
        state_.connected=false;return false;
    }
    state_={true,ptt==tx,static_cast<uint64_t>(frequency),
        mode==usb?RigMode::USB:mode==lsb?RigMode::LSB:mode==data_usb?RigMode::DataUSB:mode==data_lsb?RigMode::DataLSB:RigMode::Unknown};
    out=state_;
    return true;
}
bool OmniRigControl::set_frequency(uint64_t frequency) {
    if(!frequency||frequency>static_cast<uint64_t>(std::numeric_limits<long>::max())||!put_long(L"Freq",static_cast<long>(frequency)))return false;
    return true; // OmniRig queues writes; the controller confirms by polling.
}
bool OmniRigControl::set_mode(RigMode mode) {
    if(mode==RigMode::Unknown)return false;
    return put_long(L"Mode",mode==RigMode::LSB?lsb:mode==RigMode::DataUSB?data_usb:mode==RigMode::DataLSB?data_lsb:usb);
}
bool OmniRigControl::set_ptt(PttMode mode) {
    // OmniRig exposes RX/TX, not Hamlib's separate mic/data keying selectors.
    return put_long(L"Tx",mode==PttMode::Off?rx:tx);
}
}
#endif
