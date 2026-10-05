#include "fectty/crc16.hpp"
#include "fectty/frame.hpp"
#include "fectty/convolutional.hpp"
#include "fectty/viterbi.hpp"
#include "fectty/fsk4.hpp"
#include "fectty/interleaver.hpp"
#include "fectty/acquisition.hpp"
#include "fectty/rig_control.hpp"
#include "fectty/sync.hpp"
#include "fectty/timing.hpp"
#include "fectty/net_rigctl.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "fectty/omnirig.hpp"
#endif
#include "fectty/stream_receiver.hpp"
#include "fectty/session.hpp"
#include "fectty/spectrum.hpp"
#include "fectty/settings.hpp"
#include "fectty/channel.hpp"
#include "fectty/macros.hpp"
#include "fectty/qso_log.hpp"
#include "fectty/reliable_receiver.hpp"
#include "fectty/clock_drift.hpp"
#include "fectty/tx_watchdog.hpp"
#include "fectty/link_metrics.hpp"
#include "fectty/diagnostics.hpp"
#include "fectty/wav.hpp"
#include "fectty/level_meter.hpp"
#include "fectty/timing_tracker.hpp"
#include "fectty/interop.hpp"
#include "fectty/arq.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
static int fails=0;static void check(bool x,const char*n){if(!x){std::cerr<<"FAIL: "<<n<<"\n";++fails;}}
#ifdef _WIN32
static bool rigctl_loopback_test(){
 WSADATA data{};
 if(WSAStartup(MAKEWORD(2,2),&data)!=0)return false;
 SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 if(listener==INVALID_SOCKET)return false;
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
 if(bind(listener,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==SOCKET_ERROR){closesocket(listener);return false;}
 if(listen(listener,1)==SOCKET_ERROR){closesocket(listener);return false;}
 int length=sizeof(address);
 if(getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length)==SOCKET_ERROR){closesocket(listener);return false;}
 std::atomic<bool> accepted=false;
 std::string commands;
 std::thread server([&]{
  fd_set readable;FD_ZERO(&readable);FD_SET(listener,&readable);timeval wait{2,0};
  if(select(0,&readable,nullptr,nullptr,&wait)>0&&FD_ISSET(listener,&readable)){
   SOCKET peer=accept(listener,nullptr,nullptr);
   if(peer!=INVALID_SOCKET){
   accepted=true;
   const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    size_t replied=0;
    while(std::chrono::steady_clock::now()<deadline&&commands.find("T 0\n")==std::string::npos){
     fd_set peer_read;FD_ZERO(&peer_read);FD_SET(peer,&peer_read);timeval peer_wait{0,250000};
     if(select(0,&peer_read,nullptr,nullptr,&peer_wait)<=0)continue;
     char buffer[256];const int count=recv(peer,buffer,sizeof(buffer),0);
     if(count<=0)break;
     commands.append(buffer,static_cast<size_t>(count));
     while(replied<commands.size()){
      const auto end=commands.find('\n',replied);
      if(end==std::string::npos)break;
      const char response[]="RPRT 0\n";
      send(peer,response,static_cast<int>(sizeof(response)-1),0);
      replied=end+1;
     }
    }
    closesocket(peer);
   }
  }
 });
 bool connected=false;
 bool commands_sent=false;
 {
  fectty::NetRigctlControl rig("127.0.0.1",ntohs(address.sin_port));
  connected=rig.connect()&&rig.state().connected;
  commands_sent=connected&&rig.set_frequency(14070000)&&
                rig.set_mode(fectty::RigMode::DataUSB)&&
                rig.set_ptt(fectty::PttMode::Data)&&
                rig.set_ptt(fectty::PttMode::Off);
  rig.disconnect();
 }
 server.join();
 closesocket(listener);
 WSACleanup();
 return connected&&accepted.load()&&commands_sent&&
        commands.find("F 14070000\n")!=std::string::npos&&
        commands.find("M PKTUSB 0\n")!=std::string::npos&&
        commands.find("T 1\n")!=std::string::npos&&
        commands.find("T 0\n")!=std::string::npos;
}
#endif
int main(){std::string s="123456789";check(fectty::crc16_ccitt_false(std::span(reinterpret_cast<const uint8_t*>(s.data()),s.size()))==0x29B1,"CRC-16 check vector");
 fectty::Frame f;f.sequence=3;f.payload={'H','E','L','L','O'};auto raw=fectty::encode_frame(f);auto d=fectty::decode_frame(raw);check(d&&d->payload==f.payload&&d->sequence==3,"frame round trip");
 auto bits=fectty::bytes_to_bits(raw);auto enc=fectty::convolutional_encode(bits);std::vector<int8_t>soft;for(auto b:enc)soft.push_back(b?127:-127);auto dec=fectty::viterbi_decode(soft);check(dec==bits,"FEC round trip");
 auto il=fectty::interleave(enc);std::vector<int8_t> ils;for(auto b:il)ils.push_back(b?127:-127);auto dil=fectty::deinterleave_soft(ils);check(dil==soft,"interleaver round trip");
 auto audio=fectty::fsk4_modulate(enc);auto dem=fectty::fsk4_demodulate(audio,enc.size());auto dec2=fectty::viterbi_decode(dem);check(dec2==bits,"4-FSK loopback");
 fectty::FskConfig shifted;for(double& t:shifted.tones)t+=35.0;auto acq=fectty::acquisition_waveform(shifted);auto lock=fectty::acquire(acq);check(lock.locked&&lock.frequency_offset_hz>=30&&lock.frequency_offset_hz<=40,"acquisition/AFC +35 Hz");
 auto sb=fectty::sync_bits();std::vector<int8_t> ss;for(auto b:sb)ss.push_back(b?127:-127);check(fectty::sync_distance(ss)==0,"32-bit sync word");
 auto delayed=std::vector<float>(137,0.0f);delayed.insert(delayed.end(),audio.begin(),audio.end());auto tr=fectty::find_symbol_timing(delayed,enc.size()/2);check(tr.sample_offset>=128&&tr.sample_offset<=144,"symbol timing offset");
 std::string multiline="LINE ONE\nLINE TWO\n\nLINE FOUR";fectty::ModemSession multiline_tx;auto multiline_wave=multiline_tx.transmit(multiline);fectty::ReliableReceiver multiline_rx;auto multiline_out=multiline_rx.push(multiline_wave);multiline_out+=multiline_rx.finish();check(multiline_out==multiline&&multiline_rx.stats().frames_ok==4,"plaintext line-break preservation");
 fectty::StreamReceiver sr(shifted);std::optional<fectty::StreamLock> sl;for(size_t p=0;p<acq.size();p+=333){auto z=sr.push(std::span<const float>(acq).subspan(p,std::min<size_t>(333,acq.size()-p)));if(z)sl=z;}check(sl&&sl->locked,"streaming acquisition across chunks");
 fectty::NullRigControl rig;check(rig.connect()&&rig.set_frequency(14070000)&&rig.set_ptt(fectty::PttMode::Data)&&rig.state().transmitting,"rig abstraction");rig.set_ptt(fectty::PttMode::Off);check(!rig.state().transmitting,"PTT off");
 fectty::ModemSession txs,rxs;std::string msg="HELLO STREAMING WORLD";auto wave=txs.transmit(msg);std::vector<size_t> psz;for(size_t p=0;p<msg.size();p+=8)psz.push_back(std::min<size_t>(8,msg.size()-p));auto noisy=fectty::apply_channel(wave,{0.03,1,0,0,42});check(rxs.receive_known_frames(noisy,psz)==msg&&rxs.stats().frames_ok==psz.size(),"end-to-end session frames"); fectty::ModemSession autoRx;check(autoRx.receive(noisy)==msg,"automatic variable-length air-frame recovery");
 auto sp=fectty::spectrum(std::span<const float>(wave).first(std::min<size_t>(4096,wave.size())),48000,128);check(sp.size()==128,"spectrum bins");
 fectty::MacroSet ms;ms.set("cq","CQ CQ DE {CALL} K");check(ms.expand("cq",{{"CALL","G0ABC"}})=="CQ CQ DE G0ABC K","macro expansion");
 fectty::AppSettings as;as.callsign="G0ABC";as.audio_backend="portaudio";as.rig_port=4533;as.output_volume=0.42;as.station_id=7;as.peer_station_id=8;check(fectty::save_settings(as,"fectty_test.ini"),"settings save");fectty::AppSettings as2;check(fectty::load_settings(as2,"fectty_test.ini")&&as2.callsign=="G0ABC"&&as2.audio_backend=="portaudio"&&as2.rig_port==4533&&as2.station_id==7&&as2.peer_station_id==8&&as2.output_volume>0.419&&as2.output_volume<0.421,"settings load");
 fectty::QsoRecord qr;qr.call="G0XYZ";qr.frequency="14.070";check(fectty::append_adif(qr,"fectty_test.adi"),"ADIF logging");std::ifstream af("fectty_test.adi");std::string al((std::istreambuf_iterator<char>(af)),{});check(al.find("<CALL:5>G0XYZ")!=std::string::npos,"ADIF content");
 // v0.18-v0.27 reliability tests
 fectty::ModemSession rtx; auto burst=rtx.transmit("ARBITRARY STREAM RX"); std::vector<float> arbitrary(777,0.0f); arbitrary.insert(arbitrary.end(),burst.begin(),burst.end()); arbitrary.insert(arbitrary.end(),913,0.0f); fectty::ReliableReceiver rr; std::string rout=rr.push(arbitrary); rout+=rr.finish(); check(rout=="ARBITRARY STREAM RX"&&rr.stats().acquisitions>=1,"arbitrary continuous RX");
 fectty::ModemSession rtx2a,rtx2b; auto b1=rtx2a.transmit("ONE "); auto b2=rtx2b.transmit("TWO"); std::vector<float> two(503,0);two.insert(two.end(),b1.begin(),b1.end());two.insert(two.end(),2000,0);two.insert(two.end(),b2.begin(),b2.end()); fectty::ReliableReceiver rr2; auto twoout=rr2.push(two); twoout+=rr2.finish();check(twoout=="ONE TWO"&&rr2.stats().reacquisitions>=1&&rr2.stats().sequence_gaps==0,"loss of lock and independent burst reacquisition");
 std::vector<float> adjacent=b1;adjacent.insert(adjacent.end(),b2.begin(),b2.end()); fectty::ReliableReceiver rr3; auto adjacentout=rr3.push(adjacent); adjacentout+=rr3.finish();check(adjacentout=="ONE TWO"&&rr3.stats().frames_ok==2&&rr3.stats().sequence_gaps==0,"adjacent independent burst recovery");
 std::string longmsg="THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 CQ CQ CQ DE G0ABC G0ABC K THIS IS A MULTI FRAME LIVE MODEM TEST 1234567890";
 fectty::ModemSession longtx; auto longwave=longtx.transmit(longmsg); fectty::ReliableReceiver incremental;
 std::string longout; for(size_t p=0;p<longwave.size();){size_t n=std::min<size_t>(777,longwave.size()-p); longout+=incremental.push(std::span<const float>(longwave).subspan(p,n)); p+=n;}
 longout+=incremental.finish(); check(longout==longmsg&&incremental.stats().frames_ok==((longmsg.size()+7)/8)&&incremental.stats().crc_failures==0&&incremental.stats().sequence_gaps==0&&incremental.stats().acquisitions==1,"incremental multi-frame receive");
 std::string gui_stream_msg; for(char block='A';block<='T';++block) gui_stream_msg += std::string(1,block)+"0000000";
 fectty::ModemSession gui_stream_tx; auto gui_stream_wave=gui_stream_tx.transmit(gui_stream_msg); fectty::ReliableReceiver gui_stream_rx;
 std::string gui_stream_out; for(size_t p=0;p<gui_stream_wave.size();){size_t n=std::min<size_t>(2048,gui_stream_wave.size()-p); gui_stream_out+=gui_stream_rx.push(std::span<const float>(gui_stream_wave).subspan(p,n)); p+=n;}
 gui_stream_out+=gui_stream_rx.finish(); check(gui_stream_out==gui_stream_msg&&gui_stream_rx.stats().frames_ok==20&&gui_stream_rx.stats().crc_failures==0&&gui_stream_rx.stats().sequence_gaps==0,"2048-sample GUI streaming receive");
 auto drifted=fectty::resample_clock_error(burst,25.0);check(!drifted.empty()&&drifted.size()!=burst.size(),"sample clock drift model");
 fectty::NullRigControl wrig;wrig.connect();wrig.set_ptt(fectty::PttMode::Data);fectty::TxWatchdog wd(wrig,std::chrono::milliseconds(10));auto now=std::chrono::steady_clock::now();wd.arm(now);check(wd.poll(now+std::chrono::milliseconds(11))&&!wrig.state().transmitting,"TX watchdog releases PTT");
 check(fectty::noise_std_for_ebn0(10.0,0.5,100.0/48000.0)>0,"Eb/N0 noise calibration helper");
 check(fectty::format_session_stats(rxs.stats()).find("frames_ok=")!=std::string::npos,"operator diagnostics formatting");
 // Golden interoperability vector: header/payload encoding must remain stable.
 fectty::Frame gv;gv.sequence=3;gv.payload={'A','B'};auto gvb=fectty::encode_frame(gv);check(gvb.size()==6&&gvb[0]==0x00&&gvb[1]==0x32&&gvb[2]=='A'&&gvb[3]=='B',"protocol golden header vector");
 // v0.28-v0.32 field-debugging tests
 check(fectty::write_wav_mono16("fectty_test.wav",burst,48000),"WAV capture");fectty::WavData wav;check(fectty::read_wav_mono16("fectty_test.wav",wav)&&wav.sample_rate==48000&&wav.samples.size()==burst.size(),"WAV replay");fectty::ReliableReceiver wavrx;auto wavout=wavrx.push(wav.samples);wavout+=wavrx.finish();check(wavout=="ARBITRARY STREAM RX","WAV offline decode");
 auto lv=fectty::measure_level(std::vector<float>{0.0f,0.5f,-1.0f});check(lv.clipping&&lv.clipped_samples==1&&lv.peak_dbfs>-0.01,"audio level/clipping meter");
 auto tt=fectty::compensate_clock_search(fectty::resample_clock_error(acq,40.0),80,20);check(!tt.samples.empty()&&std::abs(tt.estimated_ppm)<=80,"adaptive timing search groundwork");
  auto io=fectty::simulate_two_station("TWO STATION TEST",0.01,0,0,777);check(io.ok&&io.received=="TWO STATION TEST","deterministic two-station interoperability");
  fectty::ArqConfig arq_a;arq_a.local_station_id=1;arq_a.peer_station_id=2;fectty::ArqConfig arq_b;arq_b.local_station_id=2;arq_b.peer_station_id=1;
  fectty::ArqSender arq_sender("ADDRESSED ARQ PAYLOAD",arq_a,0x37);check(arq_sender.valid()&&!arq_sender.complete()&&arq_sender.total_chunks()>1,"ARQ sender chunking");
  auto arq_frame=arq_sender.current_frame();check(arq_frame&&fectty::decode_arq_data(*arq_frame)&&fectty::decode_arq_data(*arq_frame)->destination==2,"ARQ addressed data frame");
   fectty::ModemSession arq_tx;auto arq_audio=arq_tx.transmit_frame(*arq_frame);fectty::ReliableReceiver arq_air_rx;arq_air_rx.push(arq_audio);arq_air_rx.finish();auto arq_air_frames=arq_air_rx.take_frames();check(arq_air_frames.size()==1&&fectty::decode_arq_data(arq_air_frames.front()),"ARQ direct frame decode");fectty::ChannelConfig arq_channel;arq_channel.noise_std=0.02;arq_channel.seed=781;auto arq_impaired=fectty::apply_channel(arq_audio,arq_channel);fectty::ReliableReceiver arq_noise_rx;arq_noise_rx.push(arq_impaired);arq_noise_rx.finish();auto arq_noise_frames=arq_noise_rx.take_frames();check(arq_noise_frames.size()==1&&fectty::decode_arq_data(arq_noise_frames.front()),"ARQ frame through noisy FEC audio");
  fectty::ArqReceiver arq_receiver(arq_b);fectty::Frame wrong=*arq_frame;wrong.payload[0]=9;auto ignored=arq_receiver.on_frame(wrong);check(ignored.ignored&&!ignored.response,"ARQ destination filtering");
  auto accepted=arq_receiver.on_frame(*arq_frame);check(accepted.accepted&&accepted.response&&fectty::decode_arq_control(*accepted.response)->code==fectty::ArqControlCode::Ack,"ARQ ACK response");
  auto duplicate=arq_receiver.on_frame(*arq_frame);check(duplicate.duplicate&&duplicate.response,"ARQ duplicate ACK response");
  check(arq_sender.on_control(*accepted.response)&&arq_sender.current_chunk()==1,"ARQ sender advances on ACK");
  check(arq_sender.backoff_ms(1)>0&&arq_sender.backoff_ms(2)!=arq_sender.backoff_ms(1),"ARQ deterministic exponential backoff");
  auto hello_frame=fectty::make_arq_hello_frame(2,1,7);auto hello=hello_frame?fectty::decode_arq_hello(*hello_frame):std::nullopt;check(hello&&hello->destination==2&&hello->source==1&&hello->protocol_version==fectty::kProtocolVersion&&(hello->capabilities&fectty::kArqCapabilityPlaintext)!=0&&(hello->capabilities&fectty::kArqCapabilityArq)!=0,"plaintext ARQ HELLO capabilities");
  fectty::ArqConfig wildcard_config=arq_b;wildcard_config.peer_station_id=fectty::kBroadcastStation;fectty::ArqReceiver wildcard_receiver(wildcard_config);fectty::Frame wildcard_frame=*arq_frame;wildcard_frame.payload[1]=9;auto wildcard=wildcard_receiver.on_frame(wildcard_frame);check(wildcard.accepted&&wildcard.response,"wildcard peer station filtering");
  std::string segmented_text(900,'X');fectty::ArqSender segmented_sender(segmented_text,arq_a,0x70);check(segmented_sender.valid()&&segmented_sender.total_chunks()==300,"long plaintext ARQ segmentation");
  bool segmented_acks=true;for(size_t i=0;i<255&&!segmented_sender.complete();++i){auto frame=segmented_sender.current_frame();auto data=frame?fectty::decode_arq_data(*frame):std::nullopt;auto ack=data?fectty::make_arq_control_frame(fectty::ArqControl{fectty::ArqControlCode::Ack,1,2,data->transfer_id,data->chunk_index},data->final(),static_cast<uint8_t>(i)):std::nullopt;if(!ack||!segmented_sender.on_control(*ack))segmented_acks=false;}auto next_segment_frame=segmented_sender.current_frame();auto next_segment=next_segment_frame?fectty::decode_arq_data(*next_segment_frame):std::nullopt;check(segmented_acks&&next_segment&&next_segment->transfer_id==0x71&&next_segment->chunk_index==0&&next_segment->total_chunks==45,"long ARQ segment transfer rollover");check(!fectty::ArqSender(std::string(fectty::kArqMaxMessageBytes+1,'X'),arq_a,1).valid(),"ARQ maximum plaintext message bound");
#ifdef _WIN32
 check(rigctl_loopback_test(),"rigctld loopback connect and commands");
 fectty::OmniRigControl omni;
 const bool omni_connected=omni.connect();
 check(!omni_connected||omni.state().connected,"OmniRig COM availability/cleanup");
 std::cout<<"OmniRig COM connect: "<<(omni_connected?"available":"unavailable")<<"\n";
 omni.disconnect();
#endif
 std::cout<<(fails?"Tests failed\n":"All tests passed\n");return fails?1:0;}
