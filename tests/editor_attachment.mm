#import <Cocoa/Cocoa.h>
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include <dlfcn.h>
#include <iostream>
using namespace Steinberg;using namespace Steinberg::Vst;
int main(int argc,char** argv){
 @autoreleasepool {
  if(argc!=2)return 2;
  [NSApplication sharedApplication];
  void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
  if(!library){std::cerr<<dlerror()<<"\n";return 1;}
  using Factory=IPlugView*(*)(EditController*);
  auto make=(Factory)dlsym(library,"BridgeLabMakeEditor");if(!make)return 1;
  NSString* temporary=NSTemporaryDirectory();
  auto count=[&](){NSUInteger n=0;for(NSString* name in [[NSFileManager defaultManager]contentsOfDirectoryAtPath:temporary error:nil])if([name hasPrefix:@"BridgeLab-editor-"])++n;return n;};
  NSUInteger before=count();
  try{
   for(int iteration=0;iteration<200;++iteration){
    @autoreleasepool {
     NSView* parents[2]={[[NSView alloc]initWithFrame:NSMakeRect(0,0,640,420)],[[NSView alloc]initWithFrame:NSMakeRect(0,0,1270,695)]};
     EditController* owners[2]={new EditController,new EditController};IPlugView* views[2]={make(owners[0]),make(owners[1])};
     for(int i=0;i<2;++i)if(views[i]->attached((__bridge void*)parents[i],kPlatformTypeNSView)!=kResultOk)return 1;
     for(int i=0;i<2;++i){views[i]->removed();views[i]->release();owners[i]->release();}
    }
   }
  }catch(const std::exception& error){std::cerr<<"Editor attachment failed: "<<error.what()<<"\n";return 1;}
  if(count()!=before){std::cerr<<"Editor mapping files leaked\n";return 1;}
  std::cout<<"400 editor attachments, paired views, teardown and temporary-file cleanup: PASS\n";
 }
}
