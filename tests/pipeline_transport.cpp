#include "../src/plugin/transport.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 2;std::filesystem::path home=argv[1];setenv("BRIDGELAB_HOME",argv[1],1);setenv("BRIDGELAB_PIPE_AUDIO","1",1);
 Transport transport;if(!transport.start("dummy.dll",32,48000,false))return 1;
 bool controlOk=false;std::thread control([&]{std::vector<uint8_t> result;controlOk=transport.control(GetState,{},result);});
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!std::filesystem::exists(home/"control-ready")&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 if(!std::filesystem::exists(home/"control-ready")){transport.stop();control.join();return 1;}
 for(uint64_t i=0;i<8;++i){Packet packet;packet.seq=i;if(!transport.requests.push(packet)){transport.stop();control.join();return 1;}}
 std::ofstream(home/"control-release")<<"ready";
 control.join();if(!controlOk)return 1;
 size_t received=0;Packet result;deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(received<8&&std::chrono::steady_clock::now()<deadline){
  if(transport.responses.pop(result)){if(result.seq!=received||result.response.left[0]!=float(received+1))return 1;++received;}
  else std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 bool ok=received==8&&!transport.failed;transport.stop();ok=ok&&std::filesystem::exists(home/"runner-exited");
 std::cout<<"Eight queued requests, fragmented responses, control/audio ordering and graceful shutdown: "<<(ok?"PASS":"FAIL")<<"\n";return ok?0:1;
}
