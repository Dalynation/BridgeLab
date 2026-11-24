#include <windows.h>
#include <thread>
#include <atomic>
#include <iostream>
#include "../src/editor_popups.hpp"
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM p,LPARAM l){return DefWindowProcW(w,m,p,l);}
int main(){
 WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"BridgeLabPopupContract";RegisterClassW(&cls);
 HWND root=CreateWindowW(cls.lpszClassName,L"root",WS_OVERLAPPEDWINDOW,-12000,-12000,300,260,nullptr,nullptr,cls.hInstance,nullptr);
 HWND popup=CreateWindowW(cls.lpszClassName,L"popup",WS_POPUP|WS_VISIBLE,-8000,-8000,100,80,nullptr,nullptr,cls.hInstance,nullptr);
 std::atomic<bool> ready{false},stop{false};
 std::thread other([&]{HWND unrelated=CreateWindowW(cls.lpszClassName,L"other thread",WS_POPUP|WS_VISIBLE,-9000,-9000,50,50,nullptr,nullptr,cls.hInstance,nullptr);ready=true;while(!stop){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE))DispatchMessageW(&m);Sleep(1);}DestroyWindow(unrelated);});
 while(!ready)Sleep(1);
 EditorPopups bridge;bridge.anchor={40,50};bridge.refresh(root);bool passed=bridge.windows.size()==1&&bridge.windows[0].window==popup;
 POINT mapped{};HWND target=bridge.target(POINT{50,60},mapped);POINT expected{10,10};ClientToScreen(popup,&expected);passed=passed&&target==popup&&mapped.x==expected.x&&mapped.y==expected.y&&bridge.backingOffscreen();
 HWND list=CreateWindowW(L"LISTBOX",L"",WS_CHILD|WS_VISIBLE,0,0,50,50,popup,nullptr,cls.hInstance,nullptr);
 HWND stale=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE,0,0,30,20,root,nullptr,cls.hInstance,nullptr);
 SendMessageW(list,LB_ADDSTRING,0,(LPARAM)L"one");SendMessageW(list,LB_ADDSTRING,0,(LPARAM)L"two");SendMessageW(list,LB_SETCURSEL,0,0);
 HWND keyTarget=bridge.keyboardTarget(list);SendMessageW(keyTarget,WM_KEYDOWN,VK_DOWN,1);
 HWND fallback=bridge.keyboardTarget(stale);
 bool childKeys=keyTarget==list&&SendMessageW(list,LB_GETCURSEL,0,0)==1&&(fallback==popup||IsChild(popup,fallback));
 passed=passed&&childKeys;std::cout<<(childKeys?"PASS":"FAIL")<<": popup child list receives navigation; stale root focus stays outside popup\n";
 POINT outside{};passed=passed&&bridge.target(POINT{0,0},outside)==nullptr;
 SetWindowPos(popup,nullptr,-8000,-8000,1000,1000,SWP_NOACTIVATE|SWP_NOZORDER);bridge.refresh(root);passed=passed&&bridge.windows.empty()&&bridge.unsupported==1;
 DestroyWindow(popup);bridge.refresh(root);passed=passed&&bridge.windows.empty()&&bridge.unsupported==0;
 stop=true;other.join();DestroyWindow(root);
 std::cout<<(passed?"PASS":"FAIL")<<": popup thread isolation, coordinate forwarding, size-limit detection and destroyed-window cleanup\n";return passed?0:1;
}
