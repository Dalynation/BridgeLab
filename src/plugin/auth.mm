#import <Cocoa/Cocoa.h>
#include "auth.hpp"
#include "public.sdk/source/common/pluginview.h"
#include <signal.h>
#include <sys/sysctl.h>
#include <mach/machine.h>

bool zoneSixOnlineCheck(const std::atomic<bool>* cancelled){
 @autoreleasepool {
  NSString* location=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/ZoneSixStudioHub/hub-location.plist"];
  NSDictionary* record=[NSDictionary dictionaryWithContentsOfFile:location];
  NSString* path=record[@"applicationPath"];if(![path isKindOfClass:[NSString class]])return false;
  NSBundle* bundle=[NSBundle bundleWithPath:path];
  if(![bundle.bundleIdentifier isEqualToString:@"studio.tools.hub.preview"]||[bundle.infoDictionary[@"ZoneSixAuthenticationVersion"] intValue]!=1||!bundle.executableURL)return false;
  NSTask* task=[[NSTask alloc] init];task.executableURL=bundle.executableURL;task.arguments=@[@"--authorize-product",@"bridgelab"];
  // A translated host must use the same native Hub identity that saved the
  // session. Ad-hoc Keychain trust can differ between universal binary slices.
  int arm64=0;size_t size=sizeof(arm64);
  if(sysctlbyname("hw.optional.arm64",&arm64,&size,nullptr,0)==0&&arm64&&[bundle.executableArchitectures containsObject:@(CPU_TYPE_ARM64)]){
   task.executableURL=[NSURL fileURLWithPath:@"/usr/bin/arch"];
   task.arguments=@[@"-arm64",bundle.executableURL.path,@"--authorize-product",@"bridgelab"];
  }
  task.standardOutput=[NSFileHandle fileHandleWithNullDevice];task.standardError=[NSFileHandle fileHandleWithNullDevice];task.standardInput=[NSFileHandle fileHandleWithNullDevice];
  NSError* error=nil;@try{if(![task launchAndReturnError:&error])return false;}@catch(NSException* exception){return false;}
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(14);
  while(task.running){
   if((cancelled&&cancelled->load())||std::chrono::steady_clock::now()>deadline){
    [task terminate];for(int i=0;i<10&&task.running;++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if(task.running)kill(task.processIdentifier,SIGKILL);[task waitUntilExit];return false;
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return task.terminationStatus==0;
 }
}

class ZoneSixLockedView final:public Steinberg::CPluginView {
 __strong NSView* view=nil;
public:
 ZoneSixLockedView():CPluginView(nullptr){rect={0,0,480,180};}
 Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type)override{return type&&!strcmp(type,Steinberg::kPlatformTypeNSView)?Steinberg::kResultTrue:Steinberg::kResultFalse;}
 Steinberg::tresult PLUGIN_API attached(void* parent,Steinberg::FIDString type)override{
  if(!parent||isPlatformTypeSupported(type)!=Steinberg::kResultTrue)return Steinberg::kResultFalse;
  view=[[NSView alloc]initWithFrame:NSMakeRect(0,0,480,180)];
  NSTextField* text=[NSTextField wrappingLabelWithString:@"Sign in to Zone Six Studio Hub, then reload this instrument. An internet connection is required to verify access."];
  text.frame=NSMakeRect(24,80,432,70);[view addSubview:text];
  NSTextView* link=[[NSTextView alloc]initWithFrame:NSMakeRect(24,28,400,32)];link.editable=NO;link.selectable=YES;link.drawsBackground=NO;
  [link.textStorage setAttributedString:[[NSAttributedString alloc]initWithString:@"Open Zone Six Studio Hub" attributes:@{NSLinkAttributeName:[NSURL URLWithString:@"studiohub://library"],NSFontAttributeName:[NSFont systemFontOfSize:14]}]];[view addSubview:link];
  [(__bridge NSView*)parent addSubview:view];return Steinberg::kResultOk;
 }
 Steinberg::tresult PLUGIN_API removed()override{[view removeFromSuperview];view=nil;return Steinberg::kResultOk;}
};
Steinberg::IPlugView* makeZoneSixLockedView(){return new ZoneSixLockedView;}
