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
#include <deque>
#include <memory>
#include "plugin_state.hpp"
#include "editor_input_hooks.hpp"
#include "editor_popups.hpp"
#include "audio_health.hpp"
struct EditorHost {
 EditorPopups popups;std::string healthPath;DWORD lastHealthWrite=0,lastFramePublished=0;uint32_t framesPublished=0,inputDelivered=0;std::atomic<uint32_t> inputQueued{0},inputCoalesced{0},inputRejected{0};
 std::string effectName;
 AudioHealth* audioHealth=nullptr;uint32_t lastAudioBlocks=0;DWORD lastAudioActivity=0;
 Effect* effect;std::recursive_mutex mutex;HWND window=nullptr;bool opened=false,standalone=false,embedded=false;
 HANDLE frameFile=INVALID_HANDLE_VALUE,frameMapping=nullptr;uint8_t* frameMemory=nullptr;DWORD lastFrame=0;uint32_t idleCalls=0;HWND dragTarget=nullptr;std::atomic<bool> quit{false};std::atomic<int> note{0};std::string statePath;
 const DWORD messageThread=GetCurrentThreadId();
 explicit EditorHost(Effect* e,AudioHealth* health=nullptr):audioHealth(health),effect(e){char name[128]{};effect->dispatch(effect,45,0,0,name,0);name[127]=0;effectName=name;MSG pending{};PeekMessageW(&pending,nullptr,0,0,PM_NOREMOVE);}
 std::mutex requestMutex;std::mutex jobMutex;std::condition_variable jobDone;
 struct Control {uint32_t command;std::vector<uint8_t> input,output;bool ok=false,done=false;uint32_t program=0;bool syncProgram=false;};
 struct Job {Control* control=nullptr;std::unique_ptr<Control> owned;};
 std::deque<Job> jobs;
 static constexpr UINT frameTimer=0x424c;
 static constexpr UINT controlMessage=WM_APP+11;
 void save(){if(statePath.empty())return;auto bytes=captureState(effect);if(bytes.empty())return;std::string tmp=statePath+".tmp";std::ofstream out(tmp,std::ios::binary);out.write((char*)bytes.data(),bytes.size());out.close();MoveFileExA(tmp.c_str(),statePath.c_str(),MOVEFILE_REPLACE_EXISTING);}
 void log(const char* message){char exe[MAX_PATH]{};GetModuleFileNameA(nullptr,exe,MAX_PATH);std::string p=exe;auto slash=p.find_last_of("\\/");if(slash!=std::string::npos)p.resize(slash+1);std::ofstream out(p+"editor-diagnostics.log",std::ios::app);out<<GetCurrentProcessId()<<" "<<message<<"\n";}
 void open(){
 log("open editor requested");
 if(!(effect->flags&1))return;
 if(!window&&!create("BridgeLab - Original Windows Plugin Editor"))return;
 if(!opened){effect->dispatch(effect,14,0,0,window,0);opened=true;}
 struct Rect{int16_t top,left,bottom,right;};Rect* rect=nullptr;effect->dispatch(effect,13,0,0,&rect,0);
 int width=rect?rect->right-rect->left:640,height=rect?rect->bottom-rect->top:400;width=std::clamp(width,100,4096);height=std::clamp(height,100,4096);
 RECT bounds{0,0,width,height+(standalone?44:0)};AdjustWindowRectEx(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE)));
 SetWindowPos(window,HWND_TOP,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE);
 log((std::string("editor requested client ")+std::to_string(width)+"x"+std::to_string(height)).c_str());
 if(standalone&&!GetDlgItem(window,100))CreateWindowW(L"BUTTON",L"Play C4 test note",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,12,height+8,150,28,window,(HMENU)100,GetModuleHandle(nullptr),nullptr);
 if(embedded){SetWindowPos(window,HWND_BOTTOM,-12000,-12000,0,0,SWP_NOSIZE|SWP_NOACTIVATE);ShowWindow(window,SW_SHOWNOACTIVATE);SetWindowPos(window,HWND_BOTTOM,-12000,-12000,0,0,SWP_NOSIZE|SWP_NOACTIVATE);}else{ShowWindow(window,SW_SHOW);SetForegroundWindow(window);}log(GetWindow(window,GW_CHILD)?"editor child exists":"NO editor child");
 }
 static LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 auto* s=(EditorHost*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
 if(msg==WM_NCCREATE){s=(EditorHost*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)s);}
 if(!s)return DefWindowProcA(hwnd,msg,wp,lp);
 if(msg==controlMessage){s->dispatchJobs();return 0;}
 if(msg==WM_TIMER&&wp==frameTimer){s->dispatchJobs();s->frame();return 0;}
 if(s->embedded && msg==WM_WINDOWPOSCHANGING){auto* pos=(WINDOWPOS*)lp;pos->x=-12000;pos->y=-12000;pos->flags=(pos->flags&~SWP_NOMOVE)|SWP_NOACTIVATE;}
 if(msg==WM_CLOSE){{std::lock_guard<std::recursive_mutex> guard(s->mutex);s->save();}ShowWindow(hwnd,SW_HIDE);if(s->standalone)s->quit=true;return 0;}
 if(msg==WM_COMMAND&&LOWORD(wp)==100){s->note=1;return 0;}
 return DefWindowProcA(hwnd,msg,wp,lp);
 }

 void unmapFrame(){if(!healthPath.empty()){DeleteFileA(healthPath.c_str());DeleteFileA((healthPath+".tmp").c_str());healthPath.clear();lastHealthWrite=0;}if(frameMemory)UnmapViewOfFile(frameMemory);if(frameMapping)CloseHandle(frameMapping);if(frameFile!=INVALID_HANDLE_VALUE)CloseHandle(frameFile);frameMemory=nullptr;frameMapping=nullptr;frameFile=INVALID_HANDLE_VALUE;}
 bool embed(const std::vector<uint8_t>& path){
 EditorGPU::enabled=!std::getenv("BRIDGELAB_GPU_BRIDGE")||std::string(std::getenv("BRIDGELAB_GPU_BRIDGE"))!="0";EditorGPU::reset();
 embedded=true;
 if(!window&&!create("BridgeLab - Original Windows Plugin Editor"))return false;
 SetWindowLongPtrW(window,GWL_EXSTYLE,(GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_APPWINDOW)|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE);
 unmapFrame();if(path.empty()||path.size()>32768||std::find(path.begin(),path.end(),0)!=path.end())return false;
 std::string name(path.begin(),path.end());healthPath=name+".health.json";frameFile=CreateFileA(name.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
 if(frameFile==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};if(!GetFileSizeEx(frameFile,&size)||size.QuadPart!=editorMapBytes){unmapFrame();return false;}
 frameMapping=CreateFileMappingA(frameFile,nullptr,PAGE_READWRITE,0,0,nullptr);if(frameMapping)frameMemory=(uint8_t*)MapViewOfFile(frameMapping,FILE_MAP_ALL_ACCESS,0,0,editorMapBytes);
 if(!frameMemory){unmapFrame();return false;}
 HMODULE plugin=nullptr;GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)effect->dispatch,&plugin);if(plugin)log((std::string("input imports patched ")+std::to_string(EditorInputHooks::patch(plugin))).c_str());else log("plugin module lookup FAILED");EditorInputHooks::root=window;EditorInputHooks::enabled=true;embedded=true;open();lastFrame=0;return opened;
 }
 uint32_t unsupportedPopups()const{return (EditorGPU::liveChains.load()&&!popups.windows.empty()?1u:0u)|(popups.unsupported?2u:0u);}
 static std::string jsonName(const std::string& name){std::string result;for(unsigned char c:name){if(c=='"'||c=='\\'){result+='\\';result+=char(c);}else if(c<32)result+=' ';else if(c>=128){const char* hex="0123456789abcdef";result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}else result+=char(c);}return result;}
 void reportHealth(bool force=false){
  if(healthPath.empty()||(!force&&GetTickCount()-lastHealthWrite<1000))return;
  lastHealthWrite=GetTickCount();
  std::string temporary=healthPath+".tmp";std::ofstream out(temporary,std::ios::trunc);
  out<<"{\"instrument_name\":\""<<jsonName(effectName)<<"\",\"format_version\":1,\"popup_windows_detected\":"<<popups.discovered<<",\"original_popup_pixels_forwarded\":"<<uint32_t(popups.copied!=0)<<",\"popup_input_forwarded\":"<<uint32_t(popups.routed!=0)<<",\"capture_failure\":"<<uint32_t(popups.captureFailures!=0)<<",\"visible_popup_windows\":"<<popups.windows.size()<<",\"unsupported_popup_modes\":"<<unsupportedPopups()<<",\"input_queued\":"<<inputQueued.load()<<",\"input_delivered\":"<<inputDelivered<<",\"input_coalesced\":"<<inputCoalesced.load()<<",\"input_rejected\":"<<inputRejected.load()<<",\"frames_published\":"<<framesPublished<<",\"frame_stalled\":"<<uint32_t(lastFramePublished&&GetTickCount()-lastFramePublished>2000)<<",\"popup_backing_offscreen\":"<<uint32_t(popups.backingOffscreen());
  if(audioHealth){auto a=audioHealth->snapshot();if(a[AudioHealth::Blocks]!=lastAudioBlocks){lastAudioBlocks=a[AudioHealth::Blocks];lastAudioActivity=GetTickCount();}out<<",\"audio_blocks_observed\":"<<a[AudioHealth::Blocks]<<",\"note_ons_observed\":"<<a[AudioHealth::NoteOns]<<",\"held_note_keys\":"<<a[AudioHealth::HeldKeys]<<",\"audio_rms\":"<<std::sqrt(AudioHealth::number(a[AudioHealth::MeanSquare]))<<",\"audio_peak\":"<<AudioHealth::number(a[AudioHealth::Peak])<<",\"quiet_while_notes\":"<<a[AudioHealth::Quiet]<<",\"nonfinite_audio_samples\":"<<a[AudioHealth::Invalid]<<",\"above_full_scale_recent\":"<<a[AudioHealth::AboveRecent]<<",\"audio_observation_stale\":"<<uint32_t(lastAudioActivity&&GetTickCount()-lastAudioActivity>2000);}
  out<<"}";out.close();
  if(out)MoveFileExA(temporary.c_str(),healthPath.c_str(),MOVEFILE_REPLACE_EXISTING);
 }
 void frame(){
 if(!embedded||!frameMemory||!opened||GetTickCount()-lastFrame<66)return;lastFrame=GetTickCount();
 RECT rect{};GetClientRect(window,&rect);int w=rect.right,h=rect.bottom;if(w<1||h<1||w>int(editorMaxWidth)||h>int(editorMaxHeight))return;
 popups.refresh(window,!EditorGPU::liveChains.load());
 if(EditorGPU::enabled&&EditorGPU::liveChains.load()){if(EditorGPU::publish(frameMemory,w,h)){++framesPublished;lastFramePublished=GetTickCount();}reportHealth();return;}
 HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=nullptr;
 HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
 if(bitmap){HGDIOBJ old=SelectObject(dc,bitmap);PatBlt(dc,0,0,w,h,BLACKNESS);
 BitBlt(dc,0,0,w,h,screen,0,0,SRCCOPY);
 popups.compose(dc);
 GdiFlush();auto* header=(uint32_t*)frameMemory;InterlockedIncrement((volatile LONG*)&header[1]);MemoryBarrier();header[0]=editorFrameMagic;header[2]=w;header[3]=h;std::memcpy(frameMemory+16,pixels,size_t(w)*h*4);MemoryBarrier();InterlockedIncrement((volatile LONG*)&header[1]);++framesPublished;lastFramePublished=GetTickCount();SelectObject(dc,old);DeleteObject(bitmap);}
 DeleteDC(dc);ReleaseDC(window,screen);reportHealth();
 }
 bool input(const std::vector<uint8_t>& bytes){
 if(!opened||bytes.size()!=sizeof(EditorInputEvent))return false;EditorInputEvent e{};std::memcpy(&e,bytes.data(),sizeof(e));
 switch(e.message){case WM_MOUSEMOVE:case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_LBUTTONDBLCLK:case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_MOUSEWHEEL:case WM_KEYDOWN:case WM_KEYUP:case WM_CHAR:break;default:return false;}
 HWND target=nullptr;
 if(e.message==WM_KEYDOWN||e.message==WM_KEYUP||e.message==WM_CHAR){if(embedded&&!EditorGPU::liveChains.load())popups.refresh(window);target=embedded?EditorInputHooks::focused:GetFocus();if(embedded&&!EditorGPU::liveChains.load()&&!popups.windows.empty())target=popups.windows.front().window;if(!target||(GetWindowThreadProcessId(target,nullptr)!=messageThread))target=GetWindow(window,GW_CHILD);if(!target)target=window;SendMessageA(target,e.message,e.value,1);++inputDelivered;return true;}
 POINT point{e.x,e.y};ClientToScreen(window,&point);HWND popup=nullptr;
 if(embedded&&!EditorGPU::liveChains.load()){popups.refresh(window);popup=popups.target(POINT{e.x,e.y},point);}
 EditorInputHooks::cursor=point;EditorInputHooks::buttons=e.value&3;target=popup?EditorInputHooks::windowFromPoint(point):((embedded&&EditorInputHooks::captured)?EditorInputHooks::captured:dragTarget);
 if(target&&(!IsWindow(target)||GetWindowThreadProcessId(target,nullptr)!=messageThread)){target=nullptr;dragTarget=nullptr;EditorInputHooks::captured=nullptr;}
 if(!target){target=window;for(int depth=0;depth<32;++depth){POINT local=point;ScreenToClient(target,&local);HWND child=ChildWindowFromPointEx(target,local,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED);if(!child||child==target)break;target=child;}}
 POINT local=point;ScreenToClient(target,&local);

 if(e.message==WM_LBUTTONDOWN||e.message==WM_RBUTTONDOWN){dragTarget=target;SendMessageA(GetWindow(window,GW_CHILD),WM_ACTIVATE,WA_ACTIVE,0);SendMessageA(target,WM_SETCURSOR,(WPARAM)target,MAKELPARAM(HTCLIENT,e.message));if(embedded)EditorInputHooks::focused=target;else{SendMessageA(target,WM_MOUSEACTIVATE,(WPARAM)window,MAKELPARAM(HTCLIENT,e.message));SetFocus(target);}}
 LPARAM coordinates=MAKELPARAM(local.x,local.y);WPARAM flags=e.value;
 if(e.message==WM_MOUSEWHEEL){coordinates=MAKELPARAM(point.x,point.y);flags=MAKEWPARAM(0,(short)e.value);}
 SendMessageA(target,e.message,flags,coordinates);++inputDelivered;
 if(e.message==WM_LBUTTONUP||e.message==WM_RBUTTONUP)dragTarget=nullptr;return true;
 }
 void execute(Control* c){std::unique_lock<std::recursive_mutex> guard(mutex,std::defer_lock);
 // UI/message callbacks must remain independent of the audio callback. Some
 // plugins synchronize initialization with their message thread internally;
 // an outer audio lock around that thread creates a lock inversion.
 if(c->command==GetState||c->command==GetStateWithProgram||c->command==SetState||c->command==SelectProgram||c->command==SetParameter)guard.lock();auto* s=this;
switch(c->command){
 case OpenEditor:EditorInputHooks::enabled=false;s->embedded=false;s->open();c->ok=s->opened;break;
 case OpenEmbeddedEditor:c->ok=s->embed(c->input);break;
 case EditorInput:c->ok=s->input(c->input);break;
 case CloseEditor:
  // effEditClose releases the plugin GUI and stops its idle work. Hiding only
  // the host window kept every closed editor's UI resources alive.
  if(s->opened)s->effect->dispatch(s->effect,15,0,0,nullptr,0);
  EditorGPU::enabled=false;EditorGPU::reset();s->opened=false;s->embedded=false;s->unmapFrame();if(window)DestroyWindow(window);window=nullptr;
  popups.clear();EditorInputHooks::enabled=false;EditorInputHooks::captured=nullptr;EditorInputHooks::focused=nullptr;EditorInputHooks::root=nullptr;EditorInputHooks::active=nullptr;
  c->ok=true;break;
 case EditorStatus:{RECT r{};GetWindowRect(window,&r);int32_t status[4]={r.left,r.top,int32_t(IsWindowVisible(window)),int32_t(GetForegroundWindow()==window)};c->output.resize(sizeof(status));std::memcpy(c->output.data(),status,sizeof(status));c->ok=true;break;}
 case EditorLifecycle:{uint32_t status[4]={uint32_t(opened),uint32_t(frameMemory!=nullptr),idleCalls,uint32_t(GetWindow(window,GW_CHILD)!=nullptr)};c->output.resize(sizeof(status));std::memcpy(c->output.data(),status,sizeof(status));c->ok=true;break;}
 case AudioHealthStatus:{if(audioHealth){auto values=audioHealth->snapshot();c->output.resize(values.size()*4);std::memcpy(c->output.data(),values.data(),c->output.size());c->ok=true;}break;}
 case EditorHealth:{
  popups.refresh(window,!EditorGPU::liveChains.load());reportHealth(true);std::vector<uint32_t> status{1,popups.discovered,popups.copied,popups.routed,popups.captureFailures,uint32_t(popups.windows.size()),uint32_t((EditorGPU::liveChains.load()&&!popups.windows.empty()?1:0)|(popups.unsupported?2:0)),0};
  for(auto& popup:popups.windows){auto r=popup.destination;status.insert(status.end(),{uint32_t(r.left),uint32_t(r.top),uint32_t(r.right),uint32_t(r.bottom)});}
  c->output.resize(status.size()*4);std::memcpy(c->output.data(),status.data(),c->output.size());c->ok=true;break;
 }
 case EditorGraphics:{auto status=EditorGPU::status();c->output.resize(sizeof(uint32_t)*status.size());std::memcpy(c->output.data(),status.data(),c->output.size());c->ok=true;break;}
 case EditorResources:{uint32_t status[4]={uint32_t(window!=nullptr),uint32_t(opened),frameMemory?uint32_t(editorMapBytes):0,idleCalls};c->output.resize(sizeof(status));std::memcpy(c->output.data(),status,sizeof(status));c->ok=true;break;}
 case SelectProgram:if(c->program<uint32_t(s->effect->programs)){s->effect->dispatch(s->effect,2,0,c->program,nullptr,0);c->ok=true;}break;
 case GetState:case GetStateWithProgram:if(c->syncProgram)s->effect->dispatch(s->effect,2,0,c->program,nullptr,0);c->output=captureState(s->effect);c->ok=!c->output.empty();break;
 case SetState:c->ok=restoreState(s->effect,c->input);break;
 case SetParameter:if(c->input.size()==8&&s->effect->setParameter){uint32_t index;float value;std::memcpy(&index,c->input.data(),4);std::memcpy(&value,c->input.data()+4,4);if(index<uint32_t(s->effect->parameters)&&std::isfinite(value)){s->effect->setParameter(s->effect,index,value);c->ok=true;}}break;
 }
 }
 // UI events acknowledge bounded queue ownership, not execution. A slow
 // editor must not hold the DAW's UI thread or poison a live audio connection.
 bool post(Control control){
  if(control.command==EditorInput){if(control.input.size()!=sizeof(EditorInputEvent))return false;EditorInputEvent event{};std::memcpy(&event,control.input.data(),sizeof(event));switch(event.message){case WM_MOUSEMOVE:case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_LBUTTONDBLCLK:case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_MOUSEWHEEL:case WM_KEYDOWN:case WM_KEYUP:case WM_CHAR:break;default:return false;}}
  if(control.command==OpenEmbeddedEditor&&(control.input.empty()||control.input.size()>32768||std::find(control.input.begin(),control.input.end(),0)!=control.input.end()))return false;
  std::lock_guard<std::mutex> lock(jobMutex);
  if(control.command==EditorInput&&control.input.size()==sizeof(EditorInputEvent)&&!jobs.empty()&&jobs.back().owned&&jobs.back().control->command==EditorInput){EditorInputEvent a{},b{};std::memcpy(&a,control.input.data(),sizeof(a));auto& previous=jobs.back().control->input;if(previous.size()==sizeof(b)){std::memcpy(&b,previous.data(),sizeof(b));if(a.message==WM_MOUSEMOVE&&b.message==WM_MOUSEMOVE&&a.value==b.value){previous=std::move(control.input);++inputCoalesced;return true;}}}
  if(jobs.size()>=128){if(control.command==EditorInput)++inputRejected;return false;}if(control.command==EditorInput)++inputQueued;auto owned=std::make_unique<Control>(std::move(control));auto* pointer=owned.get();jobs.push_back({pointer,std::move(owned)});PostThreadMessageW(messageThread,controlMessage,0,0);return true;
 }
 void request(Control& control){std::lock_guard<std::mutex> serialize(requestMutex);std::unique_lock<std::mutex> lock(jobMutex);jobs.push_back({&control,{}});PostThreadMessageW(messageThread,controlMessage,0,0);jobDone.wait(lock,[&]{return control.done;});}
 void dispatchJobs(){for(unsigned count=0;count<64;++count){Job job;{std::lock_guard<std::mutex> lock(jobMutex);if(jobs.empty())break;job=std::move(jobs.front());jobs.pop_front();}execute(job.control);if(!job.owned){{std::lock_guard<std::mutex> lock(jobMutex);job.control->done=true;}jobDone.notify_all();}}}
 bool create(const char* title){WNDCLASSA cls{};cls.lpfnWndProc=wndproc;cls.hInstance=GetModuleHandle(nullptr);cls.lpszClassName="BridgeLabEditorHost";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&cls);window=CreateWindowExA(embedded?(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE):0,cls.lpszClassName,"BridgeLab - Windows Plugin Editor",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,640,480,nullptr,nullptr,cls.hInstance,this);if(window){SetWindowTextA(window,title);SetTimer(window,frameTimer,66,nullptr);}return window;}
 void pump(){
 dispatchJobs();
 bool trace=opened&&idleCalls==0&&std::getenv("BRIDGELAB_EDITOR_TRACE");if(trace)log("first pump messages");
 // A continuously repainting plugin must not starve controls, idle or capture.
 MSG msg;for(int count=0;count<64&&PeekMessageA(&msg,nullptr,0,0,PM_REMOVE);++count){if(trace)log((std::string("dispatch message ")+std::to_string(msg.message)).c_str());if(msg.message==controlMessage&&!msg.hwnd)dispatchJobs();else{TranslateMessage(&msg);DispatchMessageA(&msg);}if(trace)log("dispatch complete");}if(opened){if(trace)log("first pump idle");effect->dispatch(effect,19,0,0,nullptr,0);++idleCalls;if(trace)log("first pump frame");frame();if(trace)log("first pump complete");}}
 void close(){std::lock_guard<std::recursive_mutex> guard(mutex);save();EditorGPU::enabled=false;EditorGPU::reset();unmapFrame();if(opened)effect->dispatch(effect,15,0,0,nullptr,0);opened=false;if(window)DestroyWindow(window);window=nullptr;}
};
#endif
