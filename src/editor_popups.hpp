#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
// Forward original same-thread popup pixels and coordinates without changing
// the user's pointer or injecting events into other apps/processes.
struct EditorPopups {
 struct Popup {HWND window;RECT source,destination;};
 HWND root=nullptr;bool relocateWindows=true;POINT anchor{};std::vector<Popup> windows;
 uint32_t discovered=0,copied=0,routed=0,captureFailures=0,unsupported=0;
 static BOOL CALLBACK enumerate(HWND window,LPARAM context){
  auto* self=(EditorPopups*)context;
  if(window==self->root||!IsWindowVisible(window)||IsChild(self->root,window))return TRUE;
  if(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TRANSPARENT)return TRUE;
  RECT source{};GetClientRect(window,&source);POINT origin{};ClientToScreen(window,&origin);OffsetRect(&source,origin.x,origin.y);
  int w=source.right-source.left,h=source.bottom-source.top;
  RECT bounds{};GetClientRect(self->root,&bounds);
  if(w<8||h<8)return TRUE;
  if(w>bounds.right||h>bounds.bottom){++self->unsupported;return TRUE;}
  if(self->windows.size()>=16){++self->unsupported;return FALSE;}
  RECT destination{};bool existing=false;
  for(auto& old:self->previous)if(old.window==window){destination=old.destination;existing=true;break;}
  if(!existing){
   ++self->discovered;int x=self->anchor.x,y=self->anchor.y;
   for(auto& old:self->windows){
    int dx=source.left-old.source.left,dy=source.top-old.source.top;
    if(std::abs(dx)<bounds.right&&std::abs(dy)<bounds.bottom){x=old.destination.left+dx;y=old.destination.top+dy;break;}
   }
   x=std::clamp(x,0,int(bounds.right)-w);y=std::clamp(y,0,int(bounds.bottom)-h);destination={x,y,x+w,y+h};
  }else{int x=std::clamp(int(destination.left),0,int(bounds.right)-w),y=std::clamp(int(destination.top),0,int(bounds.bottom)-h);destination={x,y,x+w,y+h};}
  // Keep real backing popups off the Mac desktop, just like the root
  // editor. Their original pixels are shown only through the native view.
  POINT rootOrigin{};ClientToScreen(self->root,&rootOrigin);
  const int screenX=rootOrigin.x+destination.left,screenY=rootOrigin.y+destination.top;
  if(self->relocateWindows&&(source.left!=screenX||source.top!=screenY)){
   SetWindowPos(window,HWND_BOTTOM,screenX,screenY,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
   POINT moved{};ClientToScreen(window,&moved);source={moved.x,moved.y,moved.x+w,moved.y+h};
  }
  self->windows.push_back({window,source,destination});return TRUE;
 }
 std::vector<Popup> previous;
 void refresh(HWND owner,bool relocate=true){root=owner;relocateWindows=relocate;previous=std::move(windows);windows.clear();unsupported=0;if(root)EnumThreadWindows(GetWindowThreadProcessId(root,nullptr),enumerate,(LPARAM)this);previous.clear();}
 void clear(){windows.clear();previous.clear();root=nullptr;}
 HWND target(POINT local,POINT& screen){
  for(auto& popup:windows)if(PtInRect(&popup.destination,local)){
   screen={popup.source.left+local.x-popup.destination.left,popup.source.top+local.y-popup.destination.top};++routed;return popup.window;
  }
  anchor=local;return nullptr;
 }
 bool backingOffscreen()const{for(auto& popup:windows){RECT r{};GetWindowRect(popup.window,&r);if(::MonitorFromRect(&r,MONITOR_DEFAULTTONULL))return false;}return true;}
 void compose(HDC destination){
  // Enumeration is front-to-back, whereas compositing is back-to-front.
  for(auto it=windows.rbegin();it!=windows.rend();++it){HDC source=GetDC(it->window);
   if(source&&BitBlt(destination,it->destination.left,it->destination.top,it->source.right-it->source.left,it->source.bottom-it->source.top,source,0,0,SRCCOPY))++copied;else ++captureFailures;
   if(source)ReleaseDC(it->window,source);
  }
 }
};
#endif
