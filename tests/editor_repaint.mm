#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/gui/iplugview.h"
#include "../src/wire.hpp"
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
using namespace Steinberg; using namespace Steinberg::Vst;
static unsigned draws=0;
static IMP originalDraw=nullptr;
static void countDraw(id self,SEL command,NSRect rect){++draws;((void(*)(id,SEL,NSRect))originalDraw)(self,command,rect);}
class Frames final:public EditController {
public:
 void* memory=MAP_FAILED;
 tresult PLUGIN_API notify(IMessage* message) override {
  if(!strcmp(message->getMessageID(),"BridgeLab.EmbedEditor")){
   const void* data=nullptr;uint32 size=0;
   if(message->getAttributes()->getBinary("payload",data,size)!=kResultOk)return kResultFalse;
   std::string path((const char*)data,size);if(path.rfind("Z:",0)!=0)return kResultFalse;
   int fd=open(path.c_str()+2,O_RDWR);if(fd<0)return kResultFalse;
   memory=mmap(nullptr,editorMapBytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);
   return memory==MAP_FAILED?kResultFalse:kResultOk;
  }
  if(!strcmp(message->getMessageID(),"BridgeLab.CloseEditor")){
   if(memory!=MAP_FAILED){munmap(memory,editorMapBytes);memory=MAP_FAILED;}
   return kResultOk;
  }
  return kResultOk;
 }
 void publish(uint32_t sequence){
  auto h=(uint32_t*)memory;__atomic_store_n(h+1,sequence-1,__ATOMIC_RELEASE);
  h[0]=editorFrameMagic;h[2]=64;h[3]=64;
  auto pixels=(uint32_t*)((char*)memory+16);
  for(int i=0;i<4096;++i)pixels[i]=0xff000000u|(sequence*23u<<8);
  __atomic_store_n(h+1,sequence,__ATOMIC_RELEASE);
 }
};
int main(int argc,char** argv){@autoreleasepool{
 if(argc!=2)return 2;[NSApplication sharedApplication];
 void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!library){std::cerr<<dlerror()<<"\n";return 1;}
 using Factory=IPlugView*(*)(EditController*);auto make=(Factory)dlsym(library,"BridgeLabMakeEditor");if(!make)return 1;
 auto method=class_getInstanceMethod(NSClassFromString(@"BridgeLabCanvas"),@selector(drawRect:));
 originalDraw=method_setImplementation(method,(IMP)countDraw);
 HostApplication host;auto owner=new EditController;auto frames=new Frames;
 owner->initialize(&host);owner->connect(frames);
 NSWindow* window=[[NSWindow alloc]initWithContentRect:NSMakeRect(40,40,160,120) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
 auto view=make(owner);if(view->attached((__bridge void*)window.contentView,kPlatformTypeNSView)!=kResultOk)return 1;
 NSView* canvas=window.contentView.subviews.firstObject;
 // Pump the host's CFRunLoop without NSApplication.run/updateWindows. Some
 // audio hosts own their event/display loop instead of AppKit's standard loop.
 for(int i=0;i<10&&frames->memory==MAP_FAILED;++i)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.03,false);
 if(frames->memory==MAP_FAILED)return 1;
 frames->publish(2);[window orderFrontRegardless];[window display];
 CFRunLoopRunInMode(kCFRunLoopDefaultMode,.15,false);
 unsigned initial=draws;bool passed=true;
 for(uint32_t sequence:{4u,6u,8u}){
  frames->publish(sequence);CFRunLoopRunInMode(kCFRunLoopDefaultMode,.20,false);
  auto received=[[canvas valueForKey:@"sequence"] unsignedIntValue];
  std::cout<<"frame="<<sequence<<" received="<<received<<" draws="<<draws<<" previous="<<initial<<"\n";
  passed=passed&&received==sequence&&draws>initial;initial=draws;
 }
 view->removed();view->release();owner->disconnect(frames);owner->terminate();owner->release();frames->release();[window orderOut:nil];
 method_setImplementation(method,originalDraw);
 std::cout<<(passed?"PASS":"FAIL")<<": live frame repaint without host updateWindows\n";return passed?0:1;
}}
