#include <windows.h>
#include <cstring>
#include <iostream>
#include "../src/editor_input_hooks.hpp"
static int notifications=0;
static HWND replacement=nullptr,observed=nullptr;
static LRESULT CALLBACK proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_CAPTURECHANGED){++notifications;replacement=(HWND)lp;observed=EditorInputHooks::getCapture();}
 return DefWindowProcW(window,message,wp,lp);
}
int main(){
 WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"BridgeLabCaptureContract";RegisterClassW(&cls);
 HWND first=CreateWindowW(cls.lpszClassName,L"first",WS_POPUP,0,0,50,50,nullptr,nullptr,cls.hInstance,nullptr);
 HWND second=CreateWindowW(cls.lpszClassName,L"second",WS_POPUP,0,0,50,50,nullptr,nullptr,cls.hInstance,nullptr);
 EditorInputHooks::enabled=true;
 bool passed=EditorInputHooks::setCapture(first)==nullptr&&notifications==0;
 passed=passed&&EditorInputHooks::setCapture(second)==first&&notifications==1&&replacement==second&&observed==second;
 EditorInputHooks::setCapture(second);passed=passed&&notifications==1;
 passed=passed&&EditorInputHooks::releaseCapture()&&notifications==2&&replacement==nullptr&&observed==nullptr;
 EditorInputHooks::setCapture(first);passed=passed&&EditorInputHooks::setCapture((HWND)1)==nullptr&&EditorInputHooks::getCapture()==first;
 EditorInputHooks::releaseCapture();EditorInputHooks::enabled=false;DestroyWindow(first);DestroyWindow(second);
 std::cout<<(passed?"PASS":"FAIL")<<": virtual capture notifies the old control after transfer/release, preserves same-window capture and rejects invalid targets\n";
 return passed?0:1;
}
