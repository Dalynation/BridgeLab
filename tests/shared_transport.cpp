#include "../src/plugin/transport.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 2;std::filesystem::path home=argv[1];setenv("BRIDGELAB_HOME",argv[1],1);unsetenv("BRIDGELAB_PIPE_AUDIO");
 Transport transport;if(!transport.start("dummy.dll",32,48000,false))return 1;
 auto wait=[&](auto predicate){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!predicate()){if(std::chrono::steady_clock::now()>deadline)return false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return true;};
 bool ok=true;uint64_t seq=0;
 // A delayed UI request must not stop audio. State replacement must stop it,
 // then resume correctly on successive requests (no stale pause acknowledgement).
 for(uint32_t command:{uint32_t(GetState),uint32_t(SetState),uint32_t(SetState)}){
  bool controlOk=false;std::thread control([&]{std::vector<uint8_t> result;controlOk=transport.control(command,{},result);});
  if(!wait([&]{return std::filesystem::exists(home/"control-ready");}))ok=false;
  Packet packet;packet.seq=seq++;if(!transport.submit(packet,false))ok=false;
  Packet result;
  if(command==GetState){if(!wait([&]{return transport.receive(result);})||result.seq!=packet.seq||result.response.left[0]!=float(packet.seq+1))ok=false;}
  else{std::this_thread::sleep_for(std::chrono::milliseconds(30));if(transport.receive(result))ok=false;}
  std::ofstream(home/"control-release")<<"go";control.join();if(!controlOk)ok=false;
  if(command==SetState&&(!wait([&]{return transport.receive(result);})||result.seq!=packet.seq))ok=false;
  if(!wait([&]{return !std::filesystem::exists(home/"control-ready");}))ok=false;
 }
 // Wrap the bounded rings multiple times and preserve sequence/sample identity.
 for(int i=0;i<400&&ok;++i){Packet packet;packet.seq=seq++;Packet result;if(!transport.submit(packet,true)||!transport.receive(result)||result.seq!=packet.seq||result.response.left[0]!=float(packet.seq+1))ok=false;}
 std::ifstream marker(home/"mapping-path");std::string path;std::getline(marker,path);transport.stop();if(path.empty()||std::filesystem::exists(path))ok=false;
 std::cout<<"Shared audio during delayed controls, ordered state replacement, ring wrap and cleanup: "<<(ok?"PASS":"FAIL")<<"\n";return ok?0:1;
}
