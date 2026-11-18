#pragma once
#ifdef _WIN32
// Offscreen editors still query the Win32 cursor and button state during their
// control handlers. Virtualize these imports in the plugin, without moving the
// user's Mac pointer or injecting input into another application.
struct EditorInputHooks {
 inline static bool enabled=false;
 inline static POINT cursor{};
 inline static uint32_t buttons=0;
 inline static HWND captured=nullptr,root=nullptr,focused=nullptr,active=nullptr;
 // The backing window is offscreen, but its editor still needs a display for
 // refresh/renderer initialization. Only virtualize our own window hierarchy.
 static HMONITOR WINAPI monitorFromWindow(HWND hwnd,DWORD flags){if(enabled&&root&&flags==MONITOR_DEFAULTTONULL&&(hwnd==root||IsChild(root,hwnd)))flags=MONITOR_DEFAULTTONEAREST;return ::MonitorFromWindow(hwnd,flags);}
 static HWND WINAPI windowFromPoint(POINT p){if(!enabled||!root)return ::WindowFromPoint(p);HWND target=root;for(int i=0;i<32;i++){POINT local=p;ScreenToClient(target,&local);HWND child=ChildWindowFromPointEx(target,local,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED);if(!child||child==target)break;target=child;}return target;}
 static BOOL WINAPI cursorPos(LPPOINT p){if(enabled){*p=cursor;return TRUE;}return ::GetCursorPos(p);}
 static BOOL WINAPI setCursorPos(int x,int y){if(enabled){cursor={x,y};return TRUE;}return ::SetCursorPos(x,y);}
 static SHORT WINAPI keyState(int key){if(enabled&&(key==VK_LBUTTON||key==VK_RBUTTON))return (buttons&(key==VK_LBUTTON?1:2))?SHORT(0x8000):0;return ::GetKeyState(key);}
 static BOOL WINAPI keyboardState(PBYTE keys){BOOL ok=::GetKeyboardState(keys);if(enabled){keys[VK_LBUTTON]=(buttons&1)?0x80:0;keys[VK_RBUTTON]=(buttons&2)?0x80:0;}return ok;}
 static SHORT WINAPI asyncKeyState(int key){if(enabled&&(key==VK_LBUTTON||key==VK_RBUTTON))return keyState(key);return ::GetAsyncKeyState(key);}
 static HWND WINAPI setCapture(HWND hwnd){if(!enabled)return ::SetCapture(hwnd);HWND old=captured;captured=hwnd;return old;}
 static HWND WINAPI getCapture(){return enabled?captured: ::GetCapture();}
 static BOOL WINAPI releaseCapture(){if(!enabled)return ::ReleaseCapture();captured=nullptr;return TRUE;}
 static HWND WINAPI getFocus(){return enabled?focused: ::GetFocus();}
 static HWND WINAPI setFocus(HWND hwnd){if(!enabled)return ::SetFocus(hwnd);HWND old=focused;focused=hwnd;return old;}
 static HWND WINAPI getActive(){return enabled?(active?active:root): ::GetActiveWindow();}
 static HWND WINAPI setActive(HWND hwnd){if(!enabled)return ::SetActiveWindow(hwnd);HWND old=active;active=hwnd;return old;}
 static HWND WINAPI getForeground(){return enabled?root: ::GetForegroundWindow();}
 static BOOL WINAPI setForeground(HWND hwnd){return enabled?TRUE: ::SetForegroundWindow(hwnd);}
 static BOOL WINAPI bringTop(HWND hwnd){return enabled?TRUE: ::BringWindowToTop(hwnd);}
 static int patch(HMODULE module){int count=0;
 auto* base=(uint8_t*)module;auto* dos=(IMAGE_DOS_HEADER*)base;if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 0;
 auto* nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!dir.VirtualAddress)return 0;
 for(auto* imp=(IMAGE_IMPORT_DESCRIPTOR*)(base+dir.VirtualAddress);imp->Name;++imp){

 auto* addresses=(IMAGE_THUNK_DATA*)(base+imp->FirstThunk);HMODULE library=GetModuleHandleA((const char*)(base+imp->Name));
 auto* names=imp->OriginalFirstThunk?(IMAGE_THUNK_DATA*)(base+imp->OriginalFirstThunk):nullptr;
 for(size_t i=0;addresses[i].u1.Function;++i){const char* imported=nullptr;if(names&&!IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal))imported=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+names[i].u1.AddressOfData))->Name;uintptr_t replacement=0;
 #define BRIDGE_HOOK(api,fn) if((imported&&!std::strcmp(imported,api))||(!imported&&library&&addresses[i].u1.Function==(uintptr_t)GetProcAddress(library,api)))replacement=(uintptr_t)&fn;
 BRIDGE_HOOK("MonitorFromWindow",monitorFromWindow) BRIDGE_HOOK("GetFocus",getFocus) BRIDGE_HOOK("SetFocus",setFocus) BRIDGE_HOOK("GetActiveWindow",getActive) BRIDGE_HOOK("SetActiveWindow",setActive) BRIDGE_HOOK("GetForegroundWindow",getForeground) BRIDGE_HOOK("SetForegroundWindow",setForeground) BRIDGE_HOOK("BringWindowToTop",bringTop) BRIDGE_HOOK("GetKeyboardState",keyboardState) BRIDGE_HOOK("WindowFromPoint",windowFromPoint) BRIDGE_HOOK("GetCursorPos",cursorPos) BRIDGE_HOOK("SetCursorPos",setCursorPos) BRIDGE_HOOK("GetKeyState",keyState) BRIDGE_HOOK("GetAsyncKeyState",asyncKeyState) BRIDGE_HOOK("SetCapture",setCapture) BRIDGE_HOOK("GetCapture",getCapture) BRIDGE_HOOK("ReleaseCapture",releaseCapture)
 #undef BRIDGE_HOOK
 if(replacement){++count;DWORD old;if(VirtualProtect(&addresses[i].u1.Function,sizeof(addresses[i].u1.Function),PAGE_READWRITE,&old)){addresses[i].u1.Function=replacement;DWORD unused;VirtualProtect(&addresses[i].u1.Function,sizeof(addresses[i].u1.Function),old,&unused);}}
 }

 }
 return count;}
};
#endif
