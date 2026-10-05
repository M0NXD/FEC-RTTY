#include "fectty/channel.hpp"
#include "fectty/session.hpp"
#include <iostream>
#include <string>
int main(int argc,char**argv){const std::string text=argc>1?argv[1]:"CQ CQ CQ DE TEST K";fectty::ModemSession tx,rx;auto air=tx.transmit(text);auto noisy=fectty::apply_channel(air,{0.12,1.0,0,0,0xFEC77});auto got=rx.receive(noisy);std::cout<<"FEC-RTTY modem simulator v"<<FECTTY_PROJECT_VERSION<<"\n\nTX: "<<text<<"\nRX: "<<got<<"\n\nFrames valid: "<<rx.stats().frames_ok<<"\nCRC failures: "<<rx.stats().crc_failures<<"\nSequence gaps: "<<rx.stats().sequence_gaps<<"\n";return got==text?0:1;}
