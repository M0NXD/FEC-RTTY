#include "fectty/session.hpp"
#include "fectty/channel.hpp"
#include <iostream>
int main(){bool ok=true;std::string msg="THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789";for(double n:{0.02,0.05,0.10,0.15,0.20}){fectty::ModemSession tx,rx;auto a=tx.transmit(msg);auto b=fectty::apply_channel(a,{n,1,0,0,12345});std::vector<size_t> sizes;for(size_t p=0;p<msg.size();p+=8)sizes.push_back(std::min<size_t>(8,msg.size()-p));auto got=rx.receive_known_frames(b,sizes);ok=ok&&got==msg;std::cout<<"noise="<<n<<" frames="<<rx.stats().frames_ok<<" crc_fail="<<rx.stats().crc_failures<<" text_ok="<<(got==msg?"yes":"no")<<"\n";}return ok?0:1;}
