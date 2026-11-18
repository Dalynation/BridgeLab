#include "../src/vst2_abi.hpp"
#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstring>
static std::atomic<float> gain{.5f};static bool active=false;static double phase=0;static float rate=48000;static HWND child=nullptr;
#ifdef BRIDGELAB_QUEUE_FIXTURE
static unsigned processCalls=0;
#endif
static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_LBUTTONUP){gain=.2f;InvalidateRect(window,nullptr,TRUE);return 0;}
 if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);RECT rect{};GetClientRect(window,&rect);HBRUSH brush=CreateSolidBrush(RGB(34,70,130));FillRect(dc,&rect,brush);DeleteObject(brush);SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);TextOutW(dc,10,10,L"Slow editor ready",17);EndPaint(window,&paint);return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
static intptr_t BCALL dispatch(Effect*,int32_t op,int32_t,intptr_t,void* pointer,float value){
 if(op==45&&pointer){
#ifdef BRIDGELAB_QUEUE_FIXTURE
  std::strcpy((char*)pointer,"Queue loss QA fixture");
#else
  std::strcpy((char*)pointer,"Slow editor QA fixture");
#endif
  return 1;
 }
 if(op==10)rate=value;
 if(op==13){static int16_t rect[4]={0,0,120,220};*(void**)pointer=rect;return 1;}
 if(op==14){Sleep(8000);WNDCLASSW cls{};cls.lpfnWndProc=windowProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"BridgeLabSlowFixture";RegisterClassW(&cls);child=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,10,10,200,100,(HWND)pointer,nullptr,cls.hInstance,nullptr);return 1;}
 if(op==15){if(child)DestroyWindow(child);child=nullptr;return 1;}
 if(op==25&&pointer){auto* events=(Events*)pointer;for(int i=0;i<events->count;++i){auto* m=events->items[i];int command=m->data[0]&0xf0;if(command==0x90)active=m->data[2]>0;else if(command==0x80)active=false;
#ifndef BRIDGELAB_QUEUE_FIXTURE
  else if(command==0xb0&&(m->data[1]==120||m->data[1]==123))active=false;
#endif
 }}
 return 0;
}
static void BCALL process(Effect*,float**,float** outputs,int32_t n){
#ifdef BRIDGELAB_QUEUE_FIXTURE
 // Test-only fault injection inside the instrument, never the host: a single
 // stall guarantees its input queue fills during the deliberate host burst.
 // Ignore panic controllers so recovery must deliver an actual note-off.
 if(++processCalls==34)Sleep(500);
#endif
 for(int i=0;i<n;++i){float v=active?float(std::sin(phase)*.2*gain.load()):0;phase+=2*3.141592653589793*261.625565/rate;outputs[0][i]=outputs[1][i]=v;}}
static float BCALL get(Effect*,int32_t){return gain.load();}
static void BCALL set(Effect*,int32_t,float value){gain=value;}
extern "C" __declspec(dllexport) Effect* VSTPluginMain(Callback host){static Effect e{};e.magic=0x56737450;e.dispatch=dispatch;e.outputs=2;e.programs=1;e.parameters=1;e.flags=1|16|256;e.id=0x534c4f57;e.replacing=process;e.getParameter=get;e.setParameter=set;return host(nullptr,1,0,0,nullptr,0)==2400?&e:nullptr;}
