#include "../src/plugin/transport.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
int main(int argc,char** argv){
 if(argc!=2)return 2;std::filesystem::path home=argv[1];setenv("BRIDGELAB_HOME",argv[1],1);setenv("BRIDGELAB_PIPE_AUDIO","1",1);
 for(int i=0;i<12;++i){
  auto marker=home/"runner-exited";std::filesystem::remove(marker);
  Transport transport;if(!transport.start("dummy.dll",32,48000,false))return 1;
  transport.stop();if(!std::filesystem::exists(marker)){std::cerr<<"Runner did not finish graceful cleanup\n";return 1;}
 }
 // A hung child must still be reaped within a bounded teardown.
 std::ofstream(home/"hang-after-eof")<<"hang";
 Transport hung;if(!hung.start("dummy.dll",32,48000,false))return 1;
 int child=0;std::ifstream(home/"child-pid")>>child;
 auto start=std::chrono::steady_clock::now();hung.stop();
 auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::filesystem::remove(home/"hang-after-eof");
 if(child<=0||kill(child,0)==0||seconds>3){std::cerr<<"Hung runner was not reaped in time\n";return 1;}
 std::ofstream(home/"hang-mid-request")<<"hang";
 Transport blocked;if(!blocked.start("dummy.dll",32,48000,false))return 1;Packet packet;blocked.submit(packet,false);
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 while(!std::filesystem::exists(home/"request-received")&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 if(!std::filesystem::exists(home/"request-received"))return 1;
 std::ifstream(home/"child-pid")>>child;start=std::chrono::steady_clock::now();blocked.stop();
 seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::filesystem::remove(home/"hang-mid-request");std::filesystem::remove(home/"request-received");
 if(kill(child,0)==0||seconds>3){std::cerr<<"Blocked worker prevented bounded shutdown\n";return 1;}
 std::cout<<"Twelve activation/teardown cycles: runner receives EOF and finishes cleanup; hung child is reaped: PASS\n";return 0;
}
