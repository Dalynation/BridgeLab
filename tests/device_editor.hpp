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
 std::string path;void* frame=MAP_FAILED;size_t bytes=16+2048*2048*4;bool sq=false,changed=false;std::vector<char> before;
 void message(const char* id,const void* data=nullptr,uint32_t length=0){
  auto msg=Steinberg::owned(static_cast<Steinberg::Vst::IMessage*>(new Steinberg::Vst::HostMessage));msg->setMessageID(id);
  if(length)msg->getAttributes()->setBinary("payload",data,length);
  if(!connection||connection->notify(msg)!=Steinberg::kResultOk)throw std::runtime_error(id);
 }
 std::vector<char> state(){Steinberg::MemoryStream stream;if(component->getState(&stream)!=Steinberg::kResultOk)throw std::runtime_error("editor state");auto p=static_cast<const char*>(stream.getData());return {p,p+stream.getSize()};}
 void input(uint32_t kind,int32_t x,int32_t y,uint32_t flags){struct {uint32_t kind;int32_t x,y;uint32_t flags;} value{kind,x,y,flags};message("BridgeLab.EditorInput",&value,sizeof(value));std::this_thread::sleep_for(std::chrono::milliseconds(40));}
 DeviceEditor(Steinberg::Vst::IComponent* c,const char* name):component(c),connection(c),sq(!std::strcmp(name,"SQ8L")){
  char file[]="/tmp/BridgeLab-device-editor-XXXXXX";int fd=mkstemp(file);if(fd<0)throw std::runtime_error("editor tempfile");path=file;
  if(ftruncate(fd,bytes)){close(fd);unlink(file);throw std::runtime_error("editor mapping size");}frame=mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);if(frame==MAP_FAILED){unlink(file);throw std::runtime_error("editor mmap");}
  std::string windows="Z:"+path;message("BridgeLab.EmbedEditor",windows.data(),uint32_t(windows.size()));std::this_thread::sleep_for(std::chrono::seconds(1));
  auto header=static_cast<uint32_t*>(frame);if(header[0]!=0x424c4631||!header[1]||!header[2]||!header[3])throw std::runtime_error("editor frame missing");
  if(sq){input(0x201,295,46,1);input(0x202,295,46,0);}before=state();
 }
 void edit(){int x=sq?234:42,y=sq?326:322;input(0x200,x,y,0);input(0x201,x,y,1);for(int target:sq?std::vector<int>{320,310,290}:std::vector<int>{342,362,382})input(0x200,x,target,1);input(0x202,x,sq?290:382,0);changed=state()!=before;if(!changed)throw std::runtime_error("live editor knob did not change state");}
 ~DeviceEditor(){try{message("BridgeLab.CloseEditor");}catch(...){}if(frame!=MAP_FAILED)munmap(frame,bytes);if(!path.empty())unlink(path.c_str());}
};
