#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "fectty/omnirig.hpp"
#endif
#include "fectty/radio_controller.hpp"
#include "fectty/net_rigctl.hpp"
#include "fectty/hamlib_rig.hpp"
#include "fectty/settings.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace fectty;
using namespace std::chrono_literals;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string("check failed: ")+ #x); ++checks; } while(false)
static unsigned checks=0;
struct Plan {
 RigState state{true,false,14080000,RigMode::USB};
 bool fail_on=false,fail_off=false,fail_read=false,wrong_mode=false;
 unsigned on=0,off=0,destructed=0;
 std::thread::id owner;
 bool affine=true;
};
class FakeRig final:public IRigControl {
 std::shared_ptr<Plan> p;
 void thread(){p->affine=p->affine&&(p->owner==std::this_thread::get_id());}
public:
 explicit FakeRig(std::shared_ptr<Plan> plan):p(std::move(plan)){p->owner=std::this_thread::get_id();}
 ~FakeRig(){thread();++p->destructed;}
 bool connect()override{thread();p->state.connected=true;return true;}
 void disconnect()override{thread();p->state.connected=false;}
 RigState state()const override{return p->state;}
 bool read_state(RigState& out)override{thread();if(p->fail_read){p->state.connected=false;return false;}out=p->state;return true;}
 bool set_frequency(uint64_t f)override{thread();p->state.frequency_hz=f;return true;}
 bool set_mode(RigMode m)override{thread();if(!p->wrong_mode)p->state.mode=m;return true;}
 bool set_ptt(PttMode m)override{thread();if(m==PttMode::Off){++p->off;if(p->fail_off)return false;p->state.transmitting=false;return true;}
  ++p->on;p->state.transmitting=true;return !p->fail_on;}
};
static void controller_tests(){
 auto p=std::make_shared<Plan>();
 {RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(p->on==0&&p->off==0);CHECK(c.apply(7100000,RigMode::DataLSB).get());
  CHECK(c.status().rig.frequency_hz==7100000&&c.status().rig.mode==RigMode::DataLSB);
  CHECK(c.begin_tx(PttMode::Data,2s).get());CHECK(c.status().ptt_owned);
  CHECK(!c.apply(14080000,RigMode::USB).get());CHECK(!c.begin_tx(PttMode::On,2s).get());
  CHECK(c.end_tx().get());CHECK(!c.status().ptt_owned&&!c.status().rig.transmitting);
  CHECK(c.begin_tx(PttMode::Mic,2s).get());
 }
 CHECK(p->off==2&&p->on==2&&p->destructed==1&&p->affine);
 p=std::make_shared<Plan>();p->fail_on=true;
 {RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(!c.begin_tx(PttMode::On,2s).get());CHECK(p->on==1&&p->off==1);
  CHECK(c.status().fault&&!c.status().ptt_owned);CHECK(!c.begin_tx(PttMode::On,2s).get());}
 p=std::make_shared<Plan>();p->fail_off=true;
 {RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(c.begin_tx(PttMode::On,2s).get());CHECK(!c.end_tx().get());
  CHECK(c.status().fault&&c.status().ptt_owned);CHECK(!c.disconnect().get());
  CHECK(!c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(c.status().message.find("unkey")!=std::string::npos);
  // Failed release must not become an endless background retry loop.
  const auto before=p->off;std::this_thread::sleep_for(100ms);CHECK(before==p->off);
  p->fail_off=false;CHECK(c.emergency_off().get());CHECK(!c.status().ptt_owned);CHECK(c.disconnect().get());}
 p=std::make_shared<Plan>();
 {RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(c.begin_tx(PttMode::On,80ms).get());std::this_thread::sleep_for(160ms);
  CHECK(c.status().fault&&!c.status().ptt_owned&&!c.status().rig.transmitting);CHECK(p->off==1);}
 for(bool external:{false,true}){
  p=std::make_shared<Plan>();p->state.transmitting=external;if(!external)p->state.mode=RigMode::Unknown;
  RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(!c.begin_tx(PttMode::On,2s).get());CHECK(p->on==0&&p->off==0);
 }
 p=std::make_shared<Plan>();p->wrong_mode=true;
 {RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  CHECK(!c.apply(7100000,RigMode::LSB).get());CHECK(c.status().fault);}
 p=std::make_shared<Plan>();
 {RadioController c;CHECK(c.connect([p]{return std::make_unique<FakeRig>(p);}).get());
  std::vector<std::future<bool>> reads;for(int i=0;i<16;++i)reads.push_back(c.refresh());
  for(auto& f:reads)CHECK(f.get());CHECK(p->affine);
  p->fail_read=true;CHECK(!c.refresh().get());CHECK(c.status().fault);CHECK(!c.begin_tx(PttMode::On,1s).get());}
}
#ifdef _WIN32
class Server {
 SOCKET listener=INVALID_SOCKET;std::atomic<SOCKET> client{INVALID_SOCKET};std::atomic<bool> stop{false};std::thread worker;
 public:
 unsigned short port=0;std::atomic<int> ptt{0},bad{0};std::atomic<uint64_t> frequency{14080000};
 std::string mode="USB";std::vector<std::string> commands;std::mutex mutex;bool fragment=false,plain=false;
 Server(){WSADATA data{};WSAStartup(MAKEWORD(2,2),&data);listener=socket(AF_INET,SOCK_STREAM,0);
  sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
  if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(listener,2))throw std::runtime_error("test server bind failed");
  int size=sizeof(addr);getsockname(listener,reinterpret_cast<sockaddr*>(&addr),&size);port=ntohs(addr.sin_port);
  worker=std::thread([this]{run();});}
 ~Server(){stop=true;const auto c=client.load();if(c!=INVALID_SOCKET)shutdown(c,SD_BOTH);closesocket(listener);worker.join();}
 void run(){while(!stop){auto c=accept(listener,nullptr,nullptr);if(c==INVALID_SOCKET)break;client=c;std::string command;char b=0;
   while(!stop&&recv(c,&b,1,0)>0){if(b!='\n'){command+=b;continue;}
    {std::lock_guard lock(mutex);commands.push_back(command);}
    std::string reply="RPRT 0\n";
    if(plain&&command=="f")reply=std::to_string(frequency.load())+"\n";
    else if(plain&&command=="m")reply=mode+"\n2400\n";
    else if(plain&&command=="t")reply=std::to_string(ptt.load())+"\n";
    else if(command=="+f")reply="get_freq:\nFrequency: "+std::to_string(frequency.load())+"\nRPRT 0\n";
    else if(command=="+m")reply="get_mode:\nMode: "+mode+"\nPassband: 2400\nRPRT 0\n";
    else if(command=="+t")reply="get_ptt:\nPTT: "+std::to_string(ptt.load())+"\nRPRT 0\n";
    else if(command.starts_with("F "))frequency=std::stoull(command.substr(2));
    else if(command.starts_with("M "))mode=command.substr(2,command.find(' ',2)-2);
    else if(command.starts_with("T "))ptt=std::stoi(command.substr(2));
    if(bad==1)reply="RPRT -1\n";
    if(bad==2)reply="RPRT 0 junk\n";
    if(bad==3)reply="get_freq:\nFrequency: -1\nRPRT 0\n";
    if(bad==4){shutdown(c,SD_BOTH);break;}
    if(bad!=5){if(fragment){for(char byte:reply)if(send(c,&byte,1,0)!=1)break;}else send(c,reply.c_str(),static_cast<int>(reply.size()),0);}
    command.clear();
   }client=INVALID_SOCKET;closesocket(c);
  }}
};
static void tcp_tests(){
 Server server;server.fragment=true;NetRigctlControl rig("localhost",server.port);
 CHECK(rig.connect());RigState read;CHECK(rig.read_state(read));CHECK(read.frequency_hz==14080000&&read.mode==RigMode::USB&&!read.transmitting);
 CHECK(rig.set_frequency(7100000));CHECK(rig.set_mode(RigMode::DataLSB));CHECK(rig.read_state(read));CHECK(read.frequency_hz==7100000&&read.mode==RigMode::DataLSB);
 for(auto [m,v]:{std::pair{PttMode::On,1},std::pair{PttMode::Mic,2},std::pair{PttMode::Data,3},std::pair{PttMode::Off,0}}){
  CHECK(rig.set_ptt(m));CHECK(server.ptt==v);CHECK(rig.read_state(read));CHECK(read.transmitting==(v!=0));}
 CHECK(!rig.set_frequency(0));CHECK(!rig.set_mode(RigMode::Unknown));
 server.bad=1;CHECK(!rig.set_ptt(PttMode::On));CHECK(!rig.read_state(read));
 server.bad=2;CHECK(!rig.set_ptt(PttMode::On));
 server.bad=3;CHECK(!rig.read_state(read));
 server.bad=4;CHECK(!rig.read_state(read));CHECK(!rig.state().connected);
 server.bad=0;CHECK(rig.connect());CHECK(rig.read_state(read));
 server.bad=5;const auto begin=std::chrono::steady_clock::now();CHECK(!rig.read_state(read));
 CHECK(std::chrono::steady_clock::now()-begin<3s);CHECK(!rig.state().connected);
 rig.disconnect();server.bad=0;server.ptt=0;
 {RadioController c;CHECK(c.connect([&]{return std::make_unique<NetRigctlControl>("127.0.0.1",server.port);}).get());
  CHECK(c.begin_tx(PttMode::On,2s).get());CHECK(c.end_tx().get());CHECK(server.ptt==0);}
 Server standard;standard.fragment=true;standard.plain=true;NetRigctlControl plain_rig("127.0.0.1",standard.port);
 CHECK(plain_rig.connect());CHECK(plain_rig.read_state(read));CHECK(read.frequency_hz==14080000&&read.mode==RigMode::USB&&!read.transmitting);
 CHECK(plain_rig.set_frequency(7100000));CHECK(plain_rig.set_mode(RigMode::DataLSB));CHECK(plain_rig.read_state(read));
 CHECK(read.frequency_hz==7100000&&read.mode==RigMode::DataLSB);CHECK(plain_rig.set_ptt(PttMode::On));CHECK(plain_rig.read_state(read)&&read.transmitting);
 CHECK(plain_rig.set_ptt(PttMode::Off));CHECK(plain_rig.read_state(read)&&!read.transmitting);plain_rig.disconnect();
}
struct OmniState {long freq=14080000,mode=0x02000000,tx=0x00200000,status=4;int slot=0;bool affine=true;std::thread::id owner;};
class Dispatch final:public IDispatch{
 std::atomic<ULONG> refs{1};std::shared_ptr<OmniState> state;bool root;
 void affine(){state->affine=state->affine&&state->owner==std::this_thread::get_id();}
public:
 Dispatch(std::shared_ptr<OmniState> s,bool r):state(std::move(s)),root(r){if(root)state->owner=std::this_thread::get_id();}
 ~Dispatch(){affine();}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(iid==IID_IUnknown||iid==IID_IDispatch){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{const auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n)override{*n=0;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT count,LCID,DISPID* ids)override{
  affine();for(UINT i=0;i<count;++i){std::wstring name(names[i]);ids[i]=name==L"Rig1"?1:name==L"Rig2"?2:name==L"Status"?3:name==L"Freq"?4:name==L"Mode"?5:name==L"Tx"?6:0;if(!ids[i])return DISP_E_UNKNOWNNAME;}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD flags,DISPPARAMS* args,VARIANT* out,EXCEPINFO*,UINT*)override{
  affine();if(root){if(id!=1&&id!=2)return DISP_E_MEMBERNOTFOUND;state->slot=id;out->vt=VT_DISPATCH;out->pdispVal=new Dispatch(state,false);return S_OK;}
  long* field=id==3?&state->status:id==4?&state->freq:id==5?&state->mode:id==6?&state->tx:nullptr;if(!field)return DISP_E_MEMBERNOTFOUND;
  if(flags&DISPATCH_PROPERTYPUT){if(args->cArgs!=1||args->rgvarg[0].vt!=VT_I4)return DISP_E_TYPEMISMATCH;*field=args->rgvarg[0].lVal;}
  else {out->vt=VT_I4;out->lVal=*field;}return S_OK;
 }
};
static void omni_tests(){
 for(int slot:{1,2}){
  auto s=std::make_shared<OmniState>();
  {RadioController c;CHECK(c.connect([s,slot]{return std::make_unique<OmniRigControl>(slot,[s]{return new Dispatch(s,true);});}).get());
   CHECK(s->slot==slot);CHECK(c.apply(7100000,RigMode::DataUSB).get());CHECK(s->mode==0x08000000);
   CHECK(c.apply(7100000,RigMode::DataLSB).get());CHECK(s->mode==0x10000000);
   CHECK(c.begin_tx(PttMode::On,2s).get());CHECK(s->tx==0x00400000);
   CHECK(c.end_tx().get());CHECK(s->tx==0x00200000);
   CHECK(!c.apply(2147483648ULL,RigMode::USB).get());CHECK(s->freq==7100000);CHECK(c.disconnect().get());}
  CHECK(s->affine);
 }
 auto s=std::make_shared<OmniState>();s->status=3;
 {RadioController c;CHECK(!c.connect([s]{return std::make_unique<OmniRigControl>(1,[s]{return new Dispatch(s,true);});}).get());CHECK(c.status().fault);}
 {RadioController c;CHECK(!c.connect([]{return std::make_unique<OmniRigControl>(3);}).get());}
}
#endif
static void settings_tests(){
 auto file=std::filesystem::temp_directory_path()/"fec-rtty-cat-settings-test.ini";
 AppSettings a,b;a.rig_backend="hamlib";a.hamlib_model=1;a.hamlib_device="COM123";a.hamlib_baud=115200;
 a.omnirig_number=2;a.tx_limit_seconds=321;a.ptt_source="data";
 CHECK(save_settings(a,file.string()));CHECK(load_settings(b,file.string()));
 CHECK(b.rig_backend==a.rig_backend&&b.hamlib_model==1&&b.hamlib_device=="COM123"&&b.hamlib_baud==115200&&b.omnirig_number==2&&b.tx_limit_seconds==321&&b.ptt_source=="data");
 std::filesystem::remove(file);
}
#ifdef FECTTY_HAS_HAMLIB
static void hamlib_tests(){
 auto models=available_hamlib_models();CHECK(models.size()>100);
 int no_readback=0;
 rig_list_foreach([](const rig_caps* caps,rig_ptr_t context)->int{
  if(!caps->get_ptt){*static_cast<int*>(context)=caps->rig_model;return 0;}return 1;
 },&no_readback);
 CHECK(no_readback!=0);
 // This fails before port open: a frontend's cached PTT is not radio feedback.
 HamlibRigControl unsupported(no_readback,"");CHECK(!unsupported.connect());
 RadioController c;CHECK(c.connect([]{return std::make_unique<HamlibRigControl>(1,"");}).get());
 for(auto mode:{RigMode::USB,RigMode::LSB,RigMode::DataUSB,RigMode::DataLSB})CHECK(c.apply(14080000,mode).get());
 CHECK(c.begin_tx(PttMode::On,2s).get());CHECK(c.status().rig.transmitting);CHECK(c.end_tx().get());
 CHECK(!c.status().rig.transmitting);CHECK(c.disconnect().get());
}
#endif
int main(){try{
 std::cout<<"Controller and settings tests\n";controller_tests();settings_tests();
#ifdef _WIN32
 std::cout<<"rigctld TCP tests\n";tcp_tests();std::cout<<"OmniRig dispatch tests\n";omni_tests();
#endif
#ifdef FECTTY_HAS_HAMLIB
 std::cout<<"Hamlib Dummy tests\n";hamlib_tests();
#endif
 std::cout<<"CAT regression checks passed: "<<checks<<" (no physical radio or RF)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
