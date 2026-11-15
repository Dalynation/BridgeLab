#pragma once
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mutex>
#include <functional>
#include <fstream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "plugin_state.hpp"
#include "editor_input_hooks.hpp"
struct EditorHost {
 Effect* effect;std::recursive_mutex mutex;HWND window=nullptr;bool opened=false,standalone=false,embedded=false;
 HANDLE frameFile=INVALID_HANDLE_VALUE,frameMapping=nullptr;uint8_t* frameMemory=nullptr;DWORD lastFrame=0;HWND dragTarget=nullptr;std::atomic<bool> quit{false};std::atomic<int> note{0};std::string statePath;
 explicit EditorHost(Effect* e):effect(e){}
 std::mutex jobMutex;std::condition_variable jobDone;
 struct Control {uint32_t command;std::vector<uint8_t> input,output;bool ok=false,done=false;uint32_t program=0;bool syncProgram=false;};
 Control* pending=nullptr;
 static constexpr UINT controlMessage=WM_APP+11;
 void save(){if(statePath.empty())return;auto bytes=captureState(effect);if(bytes.empty())return;std::string tmp=statePath+".tmp";std::ofstream out(tmp,std::ios::binary);out.write((char*)bytes.data(),bytes.size());out.close();MoveFileExA(tmp.c_str(),statePath.c_str(),MOVEFILE_REPLACE_EXISTING);}
 void log(const char* message){char exe[MAX_PATH]{};GetModuleFileNameA(nullptr,exe,MAX_PATH);std::string p=exe;auto slash=p.find_last_of("\\/");if(slash!=std::string::npos)p.resize(slash+1);std::ofstream out(p+"editor-diagnostics.log",std::ios::app);out<<GetCurrentProcessId()<<" "<<message<<"\n";}
 void open(){
 log("open editor requested");
 if(!(effect->flags&1))return;
 if(!opened){effect->dispatch(effect,14,0,0,window,0);opened=true;}
 struct Rect{int16_t top,left,bottom,right;};Rect* rect=nullptr;effect->dispatch(effect,13,0,0,&rect,0);
 int width=rect?rect->right-rect->left:640,height=rect?rect->bottom-rect->top:400;width=std::clamp(width,100,4096);height=std::clamp(height,100,4096);
 RECT bounds{0,0,width,height+(standalone?44:0)};AdjustWindowRect(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
 SetWindowPos(window,HWND_TOP,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE);
 if(standalone&&!GetDlgItem(window,100))CreateWindowW(L"BUTTON",L"Play C4 test note",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,12,height+8,150,28,window,(HMENU)100,GetModuleHandle(nullptr),nullptr);
 if(embedded){SetWindowPos(window,HWND_BOTTOM,-12000,-12000,0,0,SWP_NOSIZE|SWP_NOACTIVATE);ShowWindow(window,SW_SHOWNOACTIVATE);SetWindowPos(window,HWND_BOTTOM,-12000,-12000,0,0,SWP_NOSIZE|SWP_NOACTIVATE);}else{ShowWindow(window,SW_SHOW);SetForegroundWindow(window);}log(GetWindow(window,GW_CHILD)?"editor child exists":"NO editor child");
 }
 static LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 auto* s=(EditorHost*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
 if(msg==WM_NCCREATE){s=(EditorHost*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)s);}
 if(!s)return DefWindowProcA(hwnd,msg,wp,lp);
 std::lock_guard<std::recursive_mutex> guard(s->mutex);
 if(s->embedded && msg==WM_WINDOWPOSCHANGING){auto* pos=(WINDOWPOS*)lp;pos->x=-12000;pos->y=-12000;pos->flags=(pos->flags&~SWP_NOMOVE)|SWP_NOACTIVATE;}
 if(msg==WM_CLOSE){s->save();ShowWindow(hwnd,SW_HIDE);if(s->standalone)s->quit=true;return 0;}
 if(msg==WM_COMMAND&&LOWORD(wp)==100){s->note=1;return 0;}
 return DefWindowProcA(hwnd,msg,wp,lp);
 }

 void unmapFrame(){if(frameMemory)UnmapViewOfFile(frameMemory);if(frameMapping)CloseHandle(frameMapping);if(frameFile!=INVALID_HANDLE_VALUE)CloseHandle(frameFile);frameMemory=nullptr;frameMapping=nullptr;frameFile=INVALID_HANDLE_VALUE;}
 bool embed(const std::vector<uint8_t>& path){
 unmapFrame();if(path.empty()||path.size()>32768||std::find(path.begin(),path.end(),0)!=path.end())return false;
 std::string name(path.begin(),path.end());frameFile=CreateFileA(name.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
 if(frameFile==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};if(!GetFileSizeEx(frameFile,&size)||size.QuadPart!=editorMapBytes){unmapFrame();return false;}
 frameMapping=CreateFileMappingA(frameFile,nullptr,PAGE_READWRITE,0,0,nullptr);if(frameMapping)frameMemory=(uint8_t*)MapViewOfFile(frameMapping,FILE_MAP_ALL_ACCESS,0,0,editorMapBytes);
 if(!frameMemory){unmapFrame();return false;}
 HMODULE plugin=nullptr;GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)effect->dispatch,&plugin);if(plugin)log((std::string("input imports patched ")+std::to_string(EditorInputHooks::patch(plugin))).c_str());else log("plugin module lookup FAILED");EditorInputHooks::root=window;EditorInputHooks::enabled=true;embedded=true;open();lastFrame=0;return opened;
 }
 void frame(){
 if(!embedded||!frameMemory||!opened||GetTickCount()-lastFrame<66)return;lastFrame=GetTickCount();
 RECT rect{};GetClientRect(window,&rect);int w=rect.right,h=rect.bottom;if(w<1||h<1||w>int(editorMaxWidth)||h>int(editorMaxHeight))return;
 HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=nullptr;
 HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
 if(bitmap){HGDIOBJ old=SelectObject(dc,bitmap);PatBlt(dc,0,0,w,h,BLACKNESS);
 BitBlt(dc,0,0,w,h,screen,0,0,SRCCOPY);
 GdiFlush();auto* header=(uint32_t*)frameMemory;InterlockedIncrement((volatile LONG*)&header[1]);MemoryBarrier();header[0]=editorFrameMagic;header[2]=w;header[3]=h;std::memcpy(frameMemory+16,pixels,size_t(w)*h*4);MemoryBarrier();InterlockedIncrement((volatile LONG*)&header[1]);SelectObject(dc,old);DeleteObject(bitmap);}
 DeleteDC(dc);ReleaseDC(window,screen);
 }
 bool input(const std::vector<uint8_t>& bytes){
 if(!opened||bytes.size()!=sizeof(EditorInputEvent))return false;EditorInputEvent e{};std::memcpy(&e,bytes.data(),sizeof(e));
 switch(e.message){case WM_MOUSEMOVE:case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_LBUTTONDBLCLK:case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_MOUSEWHEEL:case WM_KEYDOWN:case WM_KEYUP:case WM_CHAR:break;default:return false;}
 HWND target=nullptr;
 if(e.message==WM_KEYDOWN||e.message==WM_KEYUP||e.message==WM_CHAR){target=embedded?EditorInputHooks::focused:GetFocus();if(!target||(!IsChild(window,target)&&target!=window))target=GetWindow(window,GW_CHILD);if(!target)target=window;SendMessageA(target,e.message,e.value,1);return true;}
 POINT point{e.x,e.y};ClientToScreen(window,&point);EditorInputHooks::cursor=point;EditorInputHooks::buttons=e.value&3;target=(embedded&&EditorInputHooks::captured)?EditorInputHooks::captured:dragTarget;
 if(!target){target=window;for(int depth=0;depth<32;++depth){POINT local=point;ScreenToClient(target,&local);HWND child=ChildWindowFromPointEx(target,local,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED);if(!child||child==target)break;target=child;}}
 POINT local=point;ScreenToClient(target,&local);

 if(e.message==WM_LBUTTONDOWN||e.message==WM_RBUTTONDOWN){dragTarget=target;SendMessageA(GetWindow(window,GW_CHILD),WM_ACTIVATE,WA_ACTIVE,0);SendMessageA(target,WM_SETCURSOR,(WPARAM)target,MAKELPARAM(HTCLIENT,e.message));if(embedded)EditorInputHooks::focused=target;else{SendMessageA(target,WM_MOUSEACTIVATE,(WPARAM)window,MAKELPARAM(HTCLIENT,e.message));SetFocus(target);}}
 LPARAM coordinates=MAKELPARAM(local.x,local.y);WPARAM flags=e.value;
 if(e.message==WM_MOUSEWHEEL){coordinates=MAKELPARAM(point.x,point.y);flags=MAKEWPARAM(0,(short)e.value);}
 SendMessageA(target,e.message,flags,coordinates);
 if(e.message==WM_LBUTTONUP||e.message==WM_RBUTTONUP)dragTarget=nullptr;return true;
 }
 void execute(Control* c){std::lock_guard<std::recursive_mutex> guard(mutex);auto* s=this;
switch(c->command){
 case OpenEditor:EditorInputHooks::enabled=false;s->embedded=false;s->open();c->ok=s->opened;break;
 case OpenEmbeddedEditor:c->ok=s->embed(c->input);break;
 case EditorInput:c->ok=s->input(c->input);break;
 case CloseEditor:s->embedded=false;s->unmapFrame();ShowWindow(window,SW_HIDE);c->ok=true;break;
 case EditorStatus:{RECT r{};GetWindowRect(window,&r);int32_t status[4]={r.left,r.top,int32_t(IsWindowVisible(window)),int32_t(GetForegroundWindow()==window)};c->output.resize(sizeof(status));std::memcpy(c->output.data(),status,sizeof(status));c->ok=true;break;}
 case SelectProgram:if(c->program<uint32_t(s->effect->programs)){s->effect->dispatch(s->effect,2,0,c->program,nullptr,0);c->ok=true;}break;
 case GetState:case GetStateWithProgram:if(c->syncProgram)s->effect->dispatch(s->effect,2,0,c->program,nullptr,0);c->output=captureState(s->effect);c->ok=!c->output.empty();break;
 case SetState:c->ok=restoreState(s->effect,c->input);break;
 case SetParameter:if(c->input.size()==8&&s->effect->setParameter){uint32_t index;float value;std::memcpy(&index,c->input.data(),4);std::memcpy(&value,c->input.data()+4,4);if(index<uint32_t(s->effect->parameters)&&std::isfinite(value)){s->effect->setParameter(s->effect,index,value);c->ok=true;}}break;
 }
 }
 void request(Control& control){std::unique_lock<std::mutex> lock(jobMutex);pending=&control;jobDone.wait(lock,[&]{return control.done;});}
 bool create(const char* title){WNDCLASSA cls{};cls.lpfnWndProc=wndproc;cls.hInstance=GetModuleHandle(nullptr);cls.lpszClassName="BridgeLabEditorHost";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&cls);window=CreateWindowExA(0,cls.lpszClassName,"BridgeLab - Windows Plugin Editor",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,640,480,nullptr,nullptr,cls.hInstance,this);if(window)SetWindowTextA(window,title);return window;}
 void pump(){
 Control* job=nullptr;{std::lock_guard<std::mutex> lock(jobMutex);job=pending;}
 if(job){execute(job);{std::lock_guard<std::mutex> lock(jobMutex);job->done=true;pending=nullptr;}jobDone.notify_all();}
 MSG msg;while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)){std::lock_guard<std::recursive_mutex> guard(mutex);TranslateMessage(&msg);DispatchMessageA(&msg);}if(opened){std::lock_guard<std::recursive_mutex> guard(mutex);effect->dispatch(effect,19,0,0,nullptr,0);frame();}}
 void close(){std::lock_guard<std::recursive_mutex> guard(mutex);save();unmapFrame();if(opened)effect->dispatch(effect,15,0,0,nullptr,0);opened=false;if(window)DestroyWindow(window);window=nullptr;}
};
#endif
