#pragma once
#ifdef _WIN32
#include <windows.h>
#include <deque>
#include <algorithm>

// Standard popup menus consume queued input inside TrackPopupMenu, before
// window dispatch. Keep their original tracking, commands and pixels intact.
struct EditorMenu {
 struct Posted {HWND window;UINT message;WPARAM flags;LPARAM coordinates;POINT screen;};
 inline static HHOOK hook=nullptr;
 inline static std::deque<Posted> pending;
 static bool native(HWND window){char name[32]{};return IsWindow(window)&&GetClassNameA(window,name,sizeof(name))&&!lstrcmpA(name,"#32768");}
 static BOOL CALLBACK findActive(HWND window,LPARAM result){
  if(IsWindowVisible(window)&&native(window)){*reinterpret_cast<HWND*>(result)=window;return FALSE;}return TRUE;
 }
 static HWND active(){HWND result=nullptr;EnumThreadWindows(GetCurrentThreadId(),findActive,reinterpret_cast<LPARAM>(&result));return result;}
 static LRESULT CALLBACK observe(int code,WPARAM removal,LPARAM data){
  if(code>=0){
   auto* message=reinterpret_cast<MSG*>(data);
   auto it=std::find_if(pending.begin(),pending.end(),[&](const Posted& p){return p.window==message->hwnd&&p.message==message->message&&p.flags==message->wParam&&p.coordinates==message->lParam;});
   if(it!=pending.end()){
    // Older Wine menu loops use MSG.pt; newer ones use screen lParam.
    // Amend only our posted messages, without moving the OS cursor.
    message->pt=it->screen;if(removal==PM_REMOVE)pending.erase(it);
   }
  }
  return CallNextHookEx(hook,code,removal,data);
 }
 static bool mouse(HWND window,UINT message,WPARAM flags,POINT screen){
  if(!native(window)||GetWindowThreadProcessId(window,nullptr)!=GetCurrentThreadId())return false;
  pending.erase(std::remove_if(pending.begin(),pending.end(),[](const Posted& p){return !IsWindow(p.window);}),pending.end());
  if(!hook)hook=SetWindowsHookExA(WH_GETMESSAGE,observe,nullptr,GetCurrentThreadId());
  if(!hook||pending.size()>=128)return false;
  LPARAM coordinates=MAKELPARAM(screen.x,screen.y);
  pending.push_back({window,message,flags,coordinates,screen});
  if(PostMessageA(window,message,flags,coordinates))return true;
  pending.pop_back();return false;
 }
 static bool key(HWND window,UINT message,WPARAM value){
  // The menu loop translates keydown itself. Forwarding the canvas's WM_CHAR
  // as well would execute the same mnemonic/Enter twice.
  return message==WM_CHAR||PostMessageA(window,message,value,1);
 }
 static void reset(){if(hook)UnhookWindowsHookEx(hook);hook=nullptr;pending.clear();}
};
#endif
