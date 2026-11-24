#import <Cocoa/Cocoa.h>
#include "editor.hpp"
#include "../wire.hpp"
#include "public.sdk/source/common/pluginview.h"
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
using namespace Steinberg;using namespace Steinberg::Vst;

// The Windows window remains in the runner. Only its client pixels and bounded
// input events cross into this native NSView; Windows handles never cross ABI.
@interface BridgeLabCanvas:NSView
@property(nonatomic,assign) EditController* controller;
@property(nonatomic,strong) NSBitmapImageRep* bitmap;
@property(nonatomic,copy) void (^resizeEditor)(int,int);
@property(nonatomic,strong) NSTimer* timer;
@property(nonatomic,copy) NSString* mappingPath;
@property(nonatomic,assign) void* mapping;
@property(nonatomic,assign) uint32_t sequence;
@property(nonatomic,assign) BOOL connected;
@property(nonatomic,weak) NSResponder* keyboardHost;
@property(nonatomic,weak) NSWindow* keyboardWindow;
@property(nonatomic,strong) NSMutableIndexSet* editorKeys;
@property(nonatomic,strong) NSMutableIndexSet* responderKeys;
@property(nonatomic,assign) BOOL routingHostKey;
@property(nonatomic,strong) id keyboardMonitor;
@property(nonatomic,strong) NSMutableDictionary<NSNumber*,NSEvent*>* hostPresses;
@property(nonatomic,strong) NSMapTable<NSNumber*,NSResponder*>* hostResponders;
-(void)start;
-(void)stop;
-(NSEvent*)routeKeyboard:(NSEvent*)event;
@end
@implementation BridgeLabCanvas
-(BOOL)isFlipped{return YES;}
-(BOOL)acceptsFirstResponder{return YES;}
-(BOOL)acceptsFirstMouse:(NSEvent*)event{return YES;}
-(BOOL)send:(const char*)name bytes:(const void*)data length:(uint32_t)length {
 if(!self.controller)return NO;
 auto message=owned(self.controller->allocateMessage());if(!message)return NO;
 message->setMessageID(name);if(length)message->getAttributes()->setBinary("payload",data,length);
 return self.controller->sendMessage(message)==kResultOk;
}
-(void)start {
 NSString* prefix=[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"BridgeLab-editor-%d-XXXXXX",getpid()]];
 const char* representation=prefix.fileSystemRepresentation;
 if(!representation)return;
 std::vector<char> path(representation,representation+strlen(representation)+1);
 int fd=mkstemp(path.data());if(fd<0)return;
 self.mappingPath=[NSString stringWithUTF8String:path.data()];
 if(ftruncate(fd,editorMapBytes)==0){void* mem=mmap(nullptr,editorMapBytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(mem!=MAP_FAILED)self.mapping=mem;}close(fd);
 if(!self.mapping)return;
 // A host can focus the editor as soon as it opens, before the keyboard
 // monitor has seen a piano note. Preserve its owning/main window then.
 NSWindow* hostWindow=self.window.parentWindow;
 if(!hostWindow&&NSApp.mainWindow!=self.window)hostWindow=NSApp.mainWindow;
 if(hostWindow){self.keyboardWindow=hostWindow;self.keyboardHost=hostWindow.firstResponder;}
 self.hostPresses=[NSMutableDictionary dictionary];
 self.hostResponders=[NSMapTable strongToWeakObjectsMapTable];
 __weak BridgeLabCanvas* weakSelf=self;
 self.keyboardMonitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown|NSEventMaskKeyUp handler:^NSEvent*(NSEvent* event){BridgeLabCanvas* pane=weakSelf;return pane?[pane routeKeyboard:event]:event;}];
 self.timer=[NSTimer scheduledTimerWithTimeInterval:1.0/15.0 repeats:YES block:^(NSTimer*){[weakSelf refresh];}];
 [[NSRunLoop mainRunLoop]addTimer:self.timer forMode:NSRunLoopCommonModes];
 [self refresh];
}
-(void)refresh {
 if(!self.mapping)return;
 if(!self.connected){std::string path="Z:";path+=self.mappingPath.fileSystemRepresentation;self.connected=[self send:"BridgeLab.EmbedEditor" bytes:path.data() length:uint32_t(path.size())];if(!self.connected)return;}
 auto* header=(uint32_t*)self.mapping;
 uint32_t seq=__atomic_load_n(header+1,__ATOMIC_ACQUIRE),magic=header[0],w=header[2],h=header[3];
 if(seq==self.sequence||(seq&1)||magic!=editorFrameMagic||w<1||h<1||w>editorMaxWidth||h>editorMaxHeight)return;
 NSBitmapImageRep* image=[[NSBitmapImageRep alloc]initWithBitmapDataPlanes:nullptr pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:w*4 bitsPerPixel:32];
 if(!image)return;
 auto* src=(const uint8_t*)self.mapping+16;auto* dst=image.bitmapData;
 for(size_t i=0;i<size_t(w)*h;i++){dst[i*4]=src[i*4+2];dst[i*4+1]=src[i*4+1];dst[i*4+2]=src[i*4];dst[i*4+3]=255;}
 if(seq!=__atomic_load_n(header+1,__ATOMIC_ACQUIRE))return;
 BOOL resize=!self.bitmap||self.bitmap.pixelsWide!=w||self.bitmap.pixelsHigh!=h;
 self.sequence=seq;self.bitmap=image;
 if(resize&&self.resizeEditor)self.resizeEditor(w,h);
 [self setNeedsDisplay:YES];
 // Hosts may own their event loop and never call AppKit updateWindows.
 [self displayIfNeeded];
}
-(void)drawRect:(NSRect)rect {
 [[NSColor colorWithWhite:0.12 alpha:1]setFill];NSRectFill(self.bounds);
 if(self.bitmap){NSGraphicsContext.currentContext.imageInterpolation=NSImageInterpolationNone;[NSGraphicsContext saveGraphicsState];NSAffineTransform* flip=[NSAffineTransform transform];[flip translateXBy:0 yBy:self.bounds.size.height];[flip scaleXBy:1 yBy:-1];[flip concat];[self.bitmap drawInRect:self.bounds];[NSGraphicsContext restoreGraphicsState];}
 else{NSDictionary* attrs=@{NSForegroundColorAttributeName:NSColor.whiteColor,NSFontAttributeName:[NSFont systemFontOfSize:15]};[@"Opening Windows plugin editor…" drawAtPoint:NSMakePoint(20,24) withAttributes:attrs];}
}
-(void)input:(NSEvent*)event message:(uint32_t)message value:(uint32_t)value {
 if(!self.bitmap)return;NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];
 EditorInputEvent input{message,(int32_t)(p.x*self.bitmap.pixelsWide/self.bounds.size.width),(int32_t)(p.y*self.bitmap.pixelsHigh/self.bounds.size.height),value};
 [self send:"BridgeLab.EditorInput" bytes:&input length:sizeof(input)];[self refresh];
}
-(void)mouseDown:(NSEvent*)event{
 // Preserve the recipient of a typing-piano press begun before this click.
 NSResponder* previous=self.window.firstResponder;
 if(previous&&previous!=self){self.keyboardHost=previous;self.keyboardWindow=self.window;}
 [self.window makeFirstResponder:self];[self input:event message:event.clickCount==2?0x203:0x201 value:1];}
-(void)mouseUp:(NSEvent*)event{[self input:event message:0x202 value:0];}
-(void)mouseDragged:(NSEvent*)event{[self input:event message:0x200 value:1];}
-(void)mouseMoved:(NSEvent*)event{[self input:event message:0x200 value:0];}
-(void)rightMouseDown:(NSEvent*)event{[self input:event message:0x204 value:2];}
-(void)rightMouseUp:(NSEvent*)event{[self input:event message:0x205 value:0];}
-(void)rightMouseDragged:(NSEvent*)event{[self input:event message:0x200 value:2];}
-(void)scrollWheel:(NSEvent*)event{[self input:event message:0x20a value:(uint32_t)(int32_t)(event.scrollingDeltaY>0?120:-120)];}
-(uint32_t)virtualKey:(NSEvent*)event {
 switch(event.keyCode){case 123:return 0x25;case 124:return 0x27;case 125:return 0x28;case 126:return 0x26;case 36:return 13;case 48:return 9;case 51:return 8;case 53:return 27;case 117:return 0x2e;default:break;}
 NSString* s=event.charactersIgnoringModifiers.uppercaseString;return s.length?[s characterAtIndex:0]:0;
}
// Remember where a typing-piano press began. A plugin may live in a different
// NSWindow from the Channel Rack; its first responder cannot release that note.
-(NSEvent*)routeKeyboard:(NSEvent*)event {
 if(self.routingHostKey)return event;
 NSNumber* key=@(event.keyCode);
 if(event.type==NSEventTypeKeyDown){
  if(event.window.firstResponder==self&&!event.isARepeat){[self.hostPresses removeObjectForKey:key];[self.hostResponders removeObjectForKey:key];}
  else if(!event.isARepeat&&![event.window.firstResponder isKindOfClass:[BridgeLabCanvas class]]){self.hostPresses[key]=event;[self.hostResponders setObject:event.window.firstResponder forKey:key];self.keyboardHost=event.window.firstResponder;self.keyboardWindow=event.window;}
  return event;
 }
 NSEvent* press=self.hostPresses[key];
 if(!press)return event;
 NSResponder* responder=[self.hostResponders objectForKey:key];
 [self.hostPresses removeObjectForKey:key];[self.hostResponders removeObjectForKey:key];
 NSWindow* window=press.window;
 if(!window||(event.window==window&&(!responder||window.firstResponder==responder)))return event;
 NSEvent* release=[NSEvent keyEventWithType:NSEventTypeKeyUp location:press.locationInWindow modifierFlags:event.modifierFlags timestamp:event.timestamp windowNumber:window.windowNumber context:nil characters:event.characters charactersIgnoringModifiers:event.charactersIgnoringModifiers isARepeat:NO keyCode:event.keyCode];
 NSResponder* current=window.firstResponder;
 self.routingHostKey=YES;
 @try{if(responder)[window makeFirstResponder:responder];[window sendEvent:release];}
 @finally{if(window.firstResponder==responder&&current)[window makeFirstResponder:current];self.routingHostKey=NO;}
 return nil;
}
-(void)keyDown:(NSEvent*)event{
 uint32_t flags=self.mapping?__atomic_load_n((uint32_t*)((uint8_t*)self.mapping+editorKeyboardOffset),__ATOMIC_ACQUIRE):editorTextInput;
 NSNumber* eventKey=@(event.keyCode);NSEvent* heldPress=self.hostPresses[eventKey];
 NSWindow* window=heldPress?heldPress.window:self.keyboardWindow;
 NSResponder* host=heldPress?[self.hostResponders objectForKey:eventKey]:self.keyboardHost;
 BOOL editorOwned=event.isARepeat&&[self.editorKeys containsIndex:event.keyCode];
 BOOL printable=event.charactersIgnoringModifiers.length&&[event.charactersIgnoringModifiers characterAtIndex:0]>=32&&[event.charactersIgnoringModifiers characterAtIndex:0]<0xf700;
 BOOL menuSpace=(flags&editorPopupInput)&&[event.charactersIgnoringModifiers isEqualToString:@" "];
 if(!self.routingHostKey&&!editorOwned&&(heldPress||(!(flags&editorTextInput)&&printable&&!menuSpace))&&window&&host&&host!=self){
  NSEvent* press=[NSEvent keyEventWithType:NSEventTypeKeyDown location:event.locationInWindow modifierFlags:event.modifierFlags timestamp:event.timestamp windowNumber:window.windowNumber context:nil characters:event.characters charactersIgnoringModifiers:event.charactersIgnoringModifiers isARepeat:event.isARepeat keyCode:event.keyCode];
  NSNumber* key=@(event.keyCode);if(!event.isARepeat){self.hostPresses[key]=press;[self.hostResponders setObject:host forKey:key];}
  NSResponder* current=window.firstResponder;self.routingHostKey=YES;
  @try{[window makeFirstResponder:host];[window sendEvent:press];}
  @finally{if(window.firstResponder==host&&current)[window makeFirstResponder:current];self.routingHostKey=NO;}
  return;
 }
 if(!editorOwned&&!(flags&editorTextInput)&&printable&&!menuSpace){
  if(!self.responderKeys)self.responderKeys=[NSMutableIndexSet indexSet];
  [self.responderKeys addIndex:event.keyCode];[super keyDown:event];return;
 }
 if(!self.editorKeys)self.editorKeys=[NSMutableIndexSet indexSet];[self.editorKeys addIndex:event.keyCode];uint32_t key=[self virtualKey:event];[self input:event message:0x100 value:key];for(NSUInteger i=0;i<event.characters.length;i++)[self input:event message:0x102 value:[event.characters characterAtIndex:i]];
}
-(void)keyUp:(NSEvent*)event{
 if(self.routingHostKey){[super keyUp:event];return;}
 if(self.hostPresses[@(event.keyCode)]&&![self routeKeyboard:event])return;
 if([self.responderKeys containsIndex:event.keyCode]){[self.responderKeys removeIndex:event.keyCode];[super keyUp:event];return;}
 if([self.editorKeys containsIndex:event.keyCode]){
  [self.editorKeys removeIndex:event.keyCode];[self input:event message:0x101 value:[self virtualKey:event]];
 }else if(self.keyboardHost&&self.keyboardHost!=self){
  // Hosts can handle their piano keys in NSWindow.sendEvent, before responder
  // dispatch. Calling the previous responder directly skips that host path.
  NSWindow* window=self.window;NSResponder* host=self.keyboardHost;
  NSResponder* current=window.firstResponder;
  self.routingHostKey=YES;
  @try{if([window makeFirstResponder:host])[window sendEvent:event];else [super keyUp:event];}
  @finally{if(window.firstResponder==host)[window makeFirstResponder:current];self.routingHostKey=NO;}
 }
 else{[super keyUp:event];}
}
-(void)stop {
 if(self.keyboardMonitor){[NSEvent removeMonitor:self.keyboardMonitor];self.keyboardMonitor=nil;}
 [self.hostPresses removeAllObjects];[self.hostResponders removeAllObjects];
 [self.timer invalidate];self.timer=nil;
 if(self.controller)[self send:"BridgeLab.CloseEditor" bytes:nullptr length:0];
 self.controller=nullptr;self.resizeEditor=nil;
 if(self.mapping){munmap(self.mapping,editorMapBytes);self.mapping=nullptr;}
 if(self.mappingPath){unlink(self.mappingPath.fileSystemRepresentation);unlink([self.mappingPath stringByAppendingString:@".health.json"].fileSystemRepresentation);self.mappingPath=nil;}
}
@end
class BridgeView final:public CPluginView {
 EditController* controller;BridgeLabCanvas* pane=nil;
public:
 explicit BridgeView(EditController* owner):controller(owner){controller->addRef();ViewRect size(0,0,640,420);setRect(size);}
 ~BridgeView() override{removed();controller->release();}
 tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override{return type&&!std::strcmp(type,kPlatformTypeNSView)?kResultTrue:kResultFalse;}
 tresult PLUGIN_API attached(void* parent,FIDString type) override {
 if(!parent||isPlatformTypeSupported(type)!=kResultTrue)return kResultFalse;
 CPluginView::attached(parent,type);
 pane=[[BridgeLabCanvas alloc]initWithFrame:NSMakeRect(0,0,rect.getWidth(),rect.getHeight())];pane.controller=controller;
 pane.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
 [pane setAccessibilityElement:YES];[pane setAccessibilityRole:NSAccessibilityGroupRole];[pane setAccessibilityLabel:@"Embedded Windows plugin editor"];
 pane.resizeEditor=^(int w,int h){ViewRect size(0,0,w,h);if(plugFrame&&plugFrame->resizeView(this,&size)==kResultOk)setRect(size);};
 [(__bridge NSView*)parent addSubview:pane];[pane start];return kResultOk;
 }
 tresult PLUGIN_API onSize(ViewRect* size) override {if(!size)return kResultFalse;setRect(*size);if(pane)pane.frame=NSMakeRect(0,0,size->getWidth(),size->getHeight());return kResultOk;}
 tresult PLUGIN_API removed() override {
 if(pane){[pane stop];[pane removeFromSuperview];pane=nil;}
 return CPluginView::removed();
 }
};
extern "C" __attribute__((visibility("default"))) IPlugView* BridgeLabMakeEditor(EditController* controller){return new BridgeView(controller);}
