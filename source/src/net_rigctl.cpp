#include "fectty/net_rigctl.hpp"
#include <sstream>
#include <chrono>
#include "fectty/parse.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace fectty {
using SocketHandle = std::intptr_t;

static bool socket_invalid(SocketHandle s){
#ifdef _WIN32
 return static_cast<SOCKET>(s)==INVALID_SOCKET;
#else
 return s<0;
#endif
}

static void closesock(SocketHandle s){
 if(socket_invalid(s))return;
#ifdef _WIN32
 closesocket(static_cast<SOCKET>(s));
#else
 ::close(static_cast<int>(s));
#endif
}

#ifdef _WIN32
static bool winsock_ready() {
 static const bool ready = [] {
  WSADATA data{};
  return WSAStartup(MAKEWORD(2, 2), &data) == 0;
 }();
 return ready;
}
#endif

static void set_socket_timeouts(SocketHandle s) {
#ifdef _WIN32
 DWORD timeout_ms = 1500;
 SOCKET native = static_cast<SOCKET>(s);
 setsockopt(native, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
 setsockopt(native, SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
 timeval timeout{1, 500000};
 int native = static_cast<int>(s);
 setsockopt(native, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
 setsockopt(native, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

static bool connect_with_timeout(SocketHandle socket_handle, const addrinfo& address) {
#ifdef _WIN32
 const auto native = static_cast<SOCKET>(socket_handle);
 u_long nonblocking = 1;
 if(ioctlsocket(native,FIONBIO,&nonblocking)!=0)return false;
 const int connected = ::connect(native,address.ai_addr,static_cast<int>(address.ai_addrlen));
 const bool pending = connected != 0 && WSAGetLastError() == WSAEWOULDBLOCK;
#else
 const auto native = static_cast<int>(socket_handle);
 const int flags = fcntl(native,F_GETFL,0);
 if(flags<0||fcntl(native,F_SETFL,flags|O_NONBLOCK)<0)return false;
 const int connected = ::connect(native,address.ai_addr,address.ai_addrlen);
 const bool pending = connected != 0 && errno == EINPROGRESS;
#endif
 bool ok = connected == 0;
 if(pending){
  fd_set writable,errors;FD_ZERO(&writable);FD_ZERO(&errors);
  FD_SET(native,&writable);FD_SET(native,&errors);
  timeval timeout{1,500000};
#ifdef _WIN32
  const int ready = select(0,nullptr,&writable,&errors,&timeout);
  int error=0,length=sizeof(error);
  ok=ready>0&&getsockopt(native,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length)==0&&error==0;
#else
  const int ready = select(native+1,nullptr,&writable,&errors,&timeout);
  int error=0;socklen_t length=sizeof(error);
  ok=ready>0&&getsockopt(native,SOL_SOCKET,SO_ERROR,&error,&length)==0&&error==0;
#endif
 }
#ifdef _WIN32
 nonblocking=0;
 if(ioctlsocket(native,FIONBIO,&nonblocking)!=0)ok=false;
#else
 if(fcntl(native,F_SETFL,flags)<0)ok=false;
#endif
 return ok;
}

static bool send_all(SocketHandle s, const std::string& payload) {
    size_t offset = 0;
    while (offset < payload.size()) {
#ifdef _WIN32
        const int sent = send(static_cast<SOCKET>(s), payload.data() + offset,
                              static_cast<int>(payload.size() - offset), 0);
#else
        const auto sent = send(static_cast<int>(s), payload.data() + offset,
                               payload.size() - offset,
#ifdef MSG_NOSIGNAL
                               MSG_NOSIGNAL
#else
                               0
#endif
                               );
#endif
        if (sent <= 0) return false;
        offset += static_cast<size_t>(sent);
    }
    return true;
}

static bool receive_line(SocketHandle s, std::string& line, bool extended) {
    line.clear();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1500);
    std::string current_line;
    while (line.size() < 4096) {
        const auto left=std::chrono::duration_cast<std::chrono::microseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(left<=0)return false;
        fd_set readable;FD_ZERO(&readable);
#ifdef _WIN32
        const auto native=static_cast<SOCKET>(s);
#else
        const auto native=static_cast<int>(s);
#endif
        FD_SET(native,&readable);
        timeval timeout{static_cast<long>(left/1000000),static_cast<long>(left%1000000)};
        char byte{};
#ifdef _WIN32
        if(select(0,&readable,nullptr,nullptr,&timeout)<=0)return false;
        const int count = recv(static_cast<SOCKET>(s), &byte, 1, 0);
#else
        if(select(static_cast<int>(s)+1,&readable,nullptr,nullptr,&timeout)<=0)return false;
        const auto count = recv(static_cast<int>(s), &byte, 1, 0);
#endif
        if (count <= 0) return false;
        line.push_back(byte);
        if(byte=='\n') {
            if(!extended || current_line.starts_with("RPRT "))return true;
            current_line.clear();
        } else if(byte!='\r') current_line.push_back(byte);
    }
    return false;
}

static bool rigctl_reply_ok(std::string reply) {
    const auto end = reply.find_first_of("\r\n");
    if (end != std::string::npos) reply.resize(end);
    std::istringstream stream(reply);
    std::string tag;
    int code = -1;
    stream >> tag >> code;
    stream >> std::ws;
    return tag == "RPRT" && code == 0 && stream.eof();
}

bool NetRigctlControl::connect(){
 disconnect();
#ifdef _WIN32
 if(!winsock_ready()) return false;
#endif
 addrinfo h{},*r=nullptr;
 h.ai_family=AF_UNSPEC;
 h.ai_socktype=SOCK_STREAM;
 // Numeric addresses (and localhost) avoid unbounded system DNS resolution
 // during PTT recovery. Hostname users should resolve/configure their IP first.
 const auto address = host_=="localhost" ? std::string("127.0.0.1") : host_;
 h.ai_flags=AI_NUMERICHOST;
 if(getaddrinfo(address.c_str(),std::to_string(port_).c_str(),&h,&r)) return false;
 for(auto*p=r;p;p=p->ai_next){
  auto native = socket(p->ai_family,p->ai_socktype,p->ai_protocol);
#ifdef _WIN32
  if(native!=INVALID_SOCKET){
#else
  if(native>=0){
#endif
   sock_=static_cast<SocketHandle>(native);
   set_socket_timeouts(sock_);
   if(connect_with_timeout(sock_,*p)) break;
  }
  closesock(sock_);
  sock_=-1;
 }
 freeaddrinfo(r);
 state_.connected=!socket_invalid(sock_);
 return state_.connected;
}
void NetRigctlControl::disconnect(){closesock(sock_);sock_=-1;state_={};}
bool NetRigctlControl::command(const std::string& c,std::string* reply){
 if(socket_invalid(sock_))return false;
 std::string q=c+"\n";
 if(!send_all(sock_,q)){state_.connected=false;closesock(sock_);sock_=-1;return false;}
 std::string response;
 if(!receive_line(sock_,response,c.starts_with('+'))){state_.connected=false;closesock(sock_);sock_=-1;return false;}
 if(reply)*reply=std::move(response);
 return true;
}
RigState NetRigctlControl::state() const{return state_;}
bool NetRigctlControl::set_frequency(uint64_t hz){std::string reply;if(!hz||!command("F "+std::to_string(hz),&reply)||!rigctl_reply_ok(reply))return false;state_.frequency_hz=hz;return true;}
bool NetRigctlControl::set_mode(RigMode m){if(m==RigMode::Unknown)return false;const char*s=m==RigMode::LSB?"LSB":m==RigMode::DataUSB?"PKTUSB":m==RigMode::DataLSB?"PKTLSB":"USB";std::string reply;if(!command(std::string("M ")+s+" 0",&reply)||!rigctl_reply_ok(reply))return false;state_.mode=m;return true;}
bool NetRigctlControl::set_ptt(PttMode m){std::string reply;const char* value=m==PttMode::Off?"0":m==PttMode::Mic?"2":m==PttMode::Data?"3":"1";if(!command(std::string("T ")+value,&reply)||!rigctl_reply_ok(reply))return false;state_.transmitting=m!=PttMode::Off;return true;}
static bool extended_value(const std::string& response,const std::string& name,std::string& value) {
 std::istringstream lines(response);std::string line;bool found=false,success=false;
 while(std::getline(lines,line)){
  if(line.starts_with(name+":")){value=line.substr(name.size()+1);const auto begin=value.find_first_not_of(" \t");value=begin==std::string::npos?"":value.substr(begin);const auto end=value.find_last_not_of(" \t\r");value=end==std::string::npos?"":value.substr(0,end+1);found=true;}
  if(line.starts_with("RPRT "))success=rigctl_reply_ok(line);
 }
 return found&&success;
}
bool NetRigctlControl::read_state(RigState& out) {
 std::string reply,value;uint64_t frequency=0;int ptt=-1;
 if(!command("+f",&reply)||!extended_value(reply,"Frequency",value)||!parse_number(value,frequency)||!frequency)return false;
 if(!command("+m",&reply)||!extended_value(reply,"Mode",value))return false;
 const auto mode=value=="USB"?RigMode::USB:value=="LSB"?RigMode::LSB:value=="PKTUSB"?RigMode::DataUSB:value=="PKTLSB"?RigMode::DataLSB:RigMode::Unknown;
 if(!command("+t",&reply)||!extended_value(reply,"PTT",value)||!parse_number(value,ptt)||ptt<0||ptt>3)return false;
 state_={true,ptt!=0,frequency,mode};out=state_;return true;
}
}
