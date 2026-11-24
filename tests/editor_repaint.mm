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
@interface NSView (BridgeLabKeyboardTest)
-(NSEvent*)routeKeyboard:(NSEvent*)event;
@end
static unsigned draws=0;
static IMP originalDraw=nullptr;
static void countDraw(id self,SEL command,NSRect rect){++draws;((void(*)(id,SEL,NSRect))originalDraw)(self,command,rect);}
@interface KeyboardHost:NSView
@property(nonatomic) BOOL held;
@end
@implementation KeyboardHost
-(BOOL)acceptsFirstResponder{return YES;}
-(void)keyDown:(NSEvent*)event{self.held=YES;}
-(void)keyUp:(NSEvent*)event{self.held=NO;}
@end
@interface KeyboardWindow:NSWindow
@property(nonatomic) BOOL held;
@end
@implementation KeyboardWindow
-(void)sendEvent:(NSEvent*)event{
 if(event.type==NSEventTypeKeyUp)self.held=NO;
 [super sendEvent:event];
}
@end
class Frames final:public EditController {
public:
 void* memory=MAP_FAILED;
 unsigned keyPresses=0,keyReleases=0;
 tresult PLUGIN_API notify(IMessage* message) override {
  if(!strcmp(message->getMessageID(),"BridgeLab.EditorInput")){
   const void* data=nullptr;uint32 size=0;
   if(message->getAttributes()->getBinary("payload",data,size)==kResultOk&&size==sizeof(EditorInputEvent)){
    auto input=(const EditorInputEvent*)data;
    if(input->message==0x100)++keyPresses;
    if(input->message==0x101)++keyReleases;
   }
  }
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
 KeyboardWindow* window=[[KeyboardWindow alloc]initWithContentRect:NSMakeRect(40,40,160,120) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
 auto view=make(owner);if(view->attached((__bridge void*)window.contentView,kPlatformTypeNSView)!=kResultOk)return 1;
 NSView* canvas=window.contentView.subviews.firstObject;
 KeyboardHost* keyboardHost=[[KeyboardHost alloc]initWithFrame:NSZeroRect];
 [window.contentView addSubview:keyboardHost];
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
 // Open a second editor without first observing a host note. Its first
 // printable key must reach the owning DAW window immediately.
 KeyboardWindow* startupHostWindow=[[KeyboardWindow alloc]initWithContentRect:NSMakeRect(420,40,160,120) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
 KeyboardHost* startupHost=[[KeyboardHost alloc]initWithFrame:startupHostWindow.contentView.bounds];[startupHostWindow.contentView addSubview:startupHost];[startupHostWindow makeFirstResponder:startupHost];
 KeyboardWindow* startupWindow=[[KeyboardWindow alloc]initWithContentRect:NSMakeRect(600,40,160,120) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
 [startupHostWindow addChildWindow:startupWindow ordered:NSWindowAbove];
 auto startupOwner=new EditController;auto startupFrames=new Frames;startupOwner->initialize(&host);startupOwner->connect(startupFrames);auto startupView=make(startupOwner);startupView->attached((__bridge void*)startupWindow.contentView,kPlatformTypeNSView);
 NSView* startupCanvas=startupWindow.contentView.subviews.firstObject;
 for(int i=0;i<10&&startupFrames->memory==MAP_FAILED;++i)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.03,false);
 startupFrames->publish(2);CFRunLoopRunInMode(kCFRunLoopDefaultMode,.15,false);[startupWindow makeFirstResponder:startupCanvas];
 NSEvent* firstDown=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:startupWindow.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
 NSEvent* firstUp=[NSEvent keyEventWithType:NSEventTypeKeyUp location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:startupWindow.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
 [startupCanvas keyDown:firstDown];bool initialNote= startupHost.held&&startupFrames->keyPresses==0;[startupCanvas keyUp:firstUp];initialNote=initialNote&&!startupHost.held;
 passed=passed&&initialNote;std::cout<<(initialNote?"PASS":"FAIL")<<": first note after editor opens reaches owning host without prior keyboard input\n";
 startupView->removed();startupView->release();startupOwner->disconnect(startupFrames);startupOwner->terminate();startupOwner->release();startupFrames->release();[startupHostWindow removeChildWindow:startupWindow];[startupWindow orderOut:nil];[startupHostWindow orderOut:nil];
 // A typing-piano note starts in the host, then clicking a knob changes focus.
 // Its release must still reach the host that received the press.
 NSEvent* down=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
 NSEvent* up=[NSEvent keyEventWithType:NSEventTypeKeyUp location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
 KeyboardWindow* mainWindow=[[KeyboardWindow alloc]initWithContentRect:NSMakeRect(240,40,160,120) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
 KeyboardHost* mainHost=[[KeyboardHost alloc]initWithFrame:NSZeroRect];[mainWindow.contentView addSubview:mainHost];[mainWindow orderFrontRegardless];[mainWindow makeFirstResponder:mainHost];
 NSEvent* mainDown=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:mainWindow.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
 bool crossWindowPassed=false;
 if([canvas respondsToSelector:@selector(routeKeyboard:)]){
  [canvas routeKeyboard:mainDown];[mainHost keyDown:mainDown];mainWindow.held=YES;
  [window makeFirstResponder:canvas];NSEvent* forwarded=[canvas routeKeyboard:up];
  crossWindowPassed=!forwarded&&!mainWindow.held&&!mainHost.held;

 }
 passed=passed&&crossWindowPassed;std::cout<<(crossWindowPassed?"PASS":"FAIL")<<": key release returns to original host window\n";
 [mainWindow orderOut:nil];
 [window makeFirstResponder:keyboardHost];[keyboardHost keyDown:down];
 NSEvent* click=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(20,20) modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil eventNumber:1 clickCount:1 pressure:1];
 [canvas mouseDown:click];[canvas keyUp:up];
 bool releasePassed=!keyboardHost.held;passed=passed&&releasePassed;
 std::cout<<(releasePassed?"PASS":"FAIL")<<": host key release after editor takes focus\n";
 unsigned presses=frames->keyPresses,releases=frames->keyReleases;
 __atomic_store_n((uint32_t*)((char*)frames->memory+editorKeyboardOffset),editorTextInput,__ATOMIC_RELEASE);
 keyboardHost.held=YES;[canvas keyDown:down];[canvas keyUp:up];
 bool editorKeysPassed=frames->keyPresses==presses+1&&frames->keyReleases==releases+1&&keyboardHost.held;
 passed=passed&&editorKeysPassed;
 std::cout<<(editorKeysPassed?"PASS":"FAIL")<<": editor-owned keys stay with Windows editor\n";
 // A fresh note after a knob click must reach the DAW without moving its window.
 __atomic_store_n((uint32_t*)((char*)frames->memory+editorKeyboardOffset),0u,__ATOMIC_RELEASE);
 [canvas setValue:mainHost forKey:@"keyboardHost"];[canvas setValue:mainWindow forKey:@"keyboardWindow"];
 mainHost.held=NO;presses=frames->keyPresses;[canvas keyDown:down];
 bool freshHostPress=mainHost.held&&frames->keyPresses==presses;
 [canvas keyUp:up];bool freshHostRelease=!mainHost.held;
 passed=passed&&freshHostPress&&freshHostRelease;
 std::cout<<((freshHostPress&&freshHostRelease)?"PASS":"FAIL")<<": new typing note after plugin click reaches host and releases\n";
 [canvas setValue:nil forKey:@"keyboardWindow"];
 // DAWs may handle typing-piano keys in NSWindow.sendEvent rather than in
 // their first responder. Calling the responder directly bypasses that path.
 [window makeFirstResponder:keyboardHost];window.held=YES;
 [canvas mouseDown:click];[canvas keyUp:up];
 bool windowReleasePassed=!window.held;passed=passed&&windowReleasePassed;
 std::cout<<(windowReleasePassed?"PASS":"FAIL")<<": host window receives release after editor focus\n";
 if([canvas respondsToSelector:@selector(routeKeyboard:)]){
  [canvas routeKeyboard:mainDown];[window makeFirstResponder:canvas];
  [canvas routeKeyboard:down];
  bool freshEditorKey=[canvas routeKeyboard:up]==up;
  passed=passed&&freshEditorKey;std::cout<<(freshEditorKey?"PASS":"FAIL")<<": editor key cannot inherit a stale host press\n";
 }
 // Ownership stays with the press when a control changes focus during repeat.
 NSEvent* repeat=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:YES keyCode:0];
 [canvas setValue:mainHost forKey:@"keyboardHost"];[canvas setValue:mainWindow forKey:@"keyboardWindow"];[window makeFirstResponder:canvas];
 __atomic_store_n((uint32_t*)((char*)frames->memory+editorKeyboardOffset),0u,__ATOMIC_RELEASE);
 presses=frames->keyPresses;[canvas keyDown:down];
 __atomic_store_n((uint32_t*)((char*)frames->memory+editorKeyboardOffset),editorTextInput,__ATOMIC_RELEASE);[canvas keyDown:repeat];[canvas keyUp:up];
 bool heldHost=frames->keyPresses==presses&&!mainHost.held;
 [canvas keyDown:down];
 __atomic_store_n((uint32_t*)((char*)frames->memory+editorKeyboardOffset),0u,__ATOMIC_RELEASE);[canvas keyDown:repeat];[canvas keyUp:up];
 bool heldText=frames->keyPresses==presses+2&&!mainHost.held;
 passed=passed&&heldHost&&heldText;std::cout<<((heldHost&&heldText)?"PASS":"FAIL")<<": key repeats retain host/text ownership across focus changes\n";
 view->removed();
 bool monitorRemoved=![canvas valueForKey:@"keyboardMonitor"];passed=passed&&monitorRemoved;
 std::cout<<(monitorRemoved?"PASS":"FAIL")<<": keyboard monitor removed with editor\n";
 view->release();owner->disconnect(frames);owner->terminate();owner->release();frames->release();[window orderOut:nil];
 method_setImplementation(method,originalDraw);
 std::cout<<(passed?"PASS":"FAIL")<<": editor repaint and keyboard routing\n";return passed?0:1;
}}
