#pragma once
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
// Exercise the same editor messages as the Mac view while Core Audio runs.
struct DeviceEditor {
 Steinberg::Vst::IComponent* component;Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> connection;
 std::string path;void* frame=MAP_FAILED;size_t bytes=16+2048*2048*4+4;bool sq=false,changed=false;std::string fixture;double readySeconds=0,ackSeconds=0,stateWaitSeconds=0;std::vector<char> before;
 void message(const char* id,const void* data=nullptr,uint32_t length=0){
  auto msg=Steinberg::owned(static_cast<Steinberg::Vst::IMessage*>(new Steinberg::Vst::HostMessage));msg->setMessageID(id);
  if(length)msg->getAttributes()->setBinary("payload",data,length);
  if(!connection||connection->notify(msg)!=Steinberg::kResultOk)throw std::runtime_error(id);
 }
 std::vector<char> state(){Steinberg::MemoryStream stream;if(component->getState(&stream)!=Steinberg::kResultOk)throw std::runtime_error("editor state");auto p=static_cast<const char*>(stream.getData());return {p,p+stream.getSize()};}
 void input(uint32_t kind,int32_t x,int32_t y,uint32_t flags){struct {uint32_t kind;int32_t x,y;uint32_t flags;} value{kind,x,y,flags};message("BridgeLab.EditorInput",&value,sizeof(value));std::this_thread::sleep_for(std::chrono::milliseconds(40));}
 DeviceEditor(Steinberg::Vst::IComponent* c,const char* name):component(c),connection(c),sq(!std::strcmp(name,"SQ8L")),fixture(name){
  char file[]="/tmp/BridgeLab-device-editor-XXXXXX";int fd=mkstemp(file);if(fd<0)throw std::runtime_error("editor tempfile");path=file;
  if(ftruncate(fd,bytes)){close(fd);unlink(file);throw std::runtime_error("editor mapping size");}frame=mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);if(frame==MAP_FAILED){unlink(file);throw std::runtime_error("editor mmap");}
  std::string windows="Z:"+path;auto ackStart=std::chrono::steady_clock::now();message("BridgeLab.EmbedEditor",windows.data(),uint32_t(windows.size()));ackSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-ackStart).count();if(fixture=="SLOW"){if(ackSeconds>2)throw std::runtime_error("slow editor blocked UI acknowledgement");auto start=std::chrono::steady_clock::now();before=state();stateWaitSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();if(stateWaitSeconds<5)throw std::runtime_error("slow editor fixture did not exercise delayed state");}auto began=std::chrono::steady_clock::now();
  auto header=static_cast<uint32_t*>(frame);while(std::chrono::steady_clock::now()-began<std::chrono::seconds(45)){if(header[0]==0x424c4631&&header[1]&&header[2]&&header[3])break;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
  readySeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
  if(header[0]!=0x424c4631||!header[1]||!header[2]||!header[3])throw std::runtime_error("editor frame missing");
  if(fixture=="J8X"||fixture=="Pha"||fixture=="UNO-LX"){int x=fixture=="J8X"?770:fixture=="Pha"?684:803,y=fixture=="J8X"?523:fixture=="Pha"?479:388;input(0x201,x,y,1);input(0x202,x,y,0);}
  if(sq){input(0x201,295,46,1);input(0x202,295,46,0);}before=state();
 }
 void edit(){if(fixture=="SLOW"){input(0x201,40,35,1);input(0x202,40,35,0);changed=state()!=before;if(!changed)throw std::runtime_error("slow editor control did not change state");return;}if(fixture=="J8X"||fixture=="Pha"||fixture=="UNO-LX"){int x=fixture=="J8X"?1060:fixture=="Pha"?907:52,y=fixture=="J8X"?500:fixture=="Pha"?355:351,delta=fixture=="UNO-LX"?30:80;input(0x200,x,y,0);input(0x201,x,y,1);for(int d=5;d<=delta;d+=5)input(0x200,x,y+d,1);input(0x202,x,y+delta,0);changed=state()!=before;if(!changed)throw std::runtime_error("original GPU editor knob did not change state");return;}int x=sq?234:42,y=sq?326:322;input(0x200,x,y,0);input(0x201,x,y,1);for(int target:sq?std::vector<int>{320,310,290}:std::vector<int>{342,362,382})input(0x200,x,target,1);input(0x202,x,sq?290:382,0);changed=state()!=before;if(!changed)throw std::runtime_error("live editor knob did not change state");}
 ~DeviceEditor(){try{message("BridgeLab.CloseEditor");}catch(...){}if(frame!=MAP_FAILED)munmap(frame,bytes);if(!path.empty())unlink(path.c_str());}
};
