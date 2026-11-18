#include "vst2_abi.hpp"
#include "wire.hpp"
#include "shared_audio.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <ole2.h>
#else
#include <dlfcn.h>
#endif
#include "plugin_state.hpp"
#include "editor_gpu.hpp"
#include "windows_editor.hpp"
static std::string pluginDirectory;
struct HostTime {double samplePos=0,sampleRate=48000,nanoSeconds=0,ppqPos=0,tempo=120,barStartPos=0,cycleStartPos=0,cycleEndPos=0;int32_t numerator=4,denominator=4,smpteOffset=0,smpteRate=0,clock=0,flags=(1<<10)|(1<<13);};
static HostTime hostTime;
static int processLevel=4;static int rate=48000;static constexpr int block=256;
static intptr_t BCALL host(Effect*,int32_t op,int32_t,intptr_t,void* ptr,float) {
 switch(op){case 7:hostTime.sampleRate=rate;return reinterpret_cast<intptr_t>(&hostTime);case 41:return pluginDirectory.empty()?0:reinterpret_cast<intptr_t>(pluginDirectory.c_str());case 1:return 2400;case 23:return processLevel;case 16:return rate;case 17:return block;
 case 32: if(ptr)std::strcpy((char*)ptr,"BridgeLab");return 1;
 case 33: if(ptr)std::strcpy((char*)ptr,"BridgeLab Offline");return 1;
 case 34:return 1;
 case 37:return ptr && (!std::strcmp((char*)ptr,"sendVstEvents") || !std::strcmp((char*)ptr,"sendVstMidiEvent"));
 default:return 0;}
}
static void u16(std::ostream& o,uint16_t v){o.put(v&255);o.put(v>>8);}
static void u32(std::ostream& o,uint32_t v){u16(o,v&65535);u16(o,v>>16);}
int main(int argc,char** argv){
 if(argc!=3 && argc!=5 && argc!=6){std::cerr<<"Usage: runner plugin-path output.wav\n";return 2;}
 const bool serve=(argc==5||argc==6) && std::string(argv[2])=="--serve";
 if((argc==5||argc==6) && !serve)return 2;
 if(serve){processLevel=std::string(argv[4])=="offline"?4:2;rate=std::atoi(argv[3]);if(rate<44100||rate>192000)return 2;}
 try{
#ifdef _WIN32
 struct OleScope{HRESULT result=OleInitialize(nullptr);~OleScope(){if(SUCCEEDED(result))OleUninitialize();}} ole;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 char fullPath[MAX_PATH]{};char* filePart=nullptr;
 if(GetFullPathNameA(argv[1],MAX_PATH,fullPath,&filePart)&&filePart){std::string directory(fullPath,filePart-fullPath);SetCurrentDirectoryA(directory.c_str());pluginDirectory=directory;}
 auto lib=LoadLibraryA(argv[1]);
 if(lib&&(!std::getenv("BRIDGELAB_GPU_BRIDGE")||std::string(std::getenv("BRIDGELAB_GPU_BRIDGE"))!="0"))EditorGPU::patch(lib);
 auto entry=lib?reinterpret_cast<Effect*(BCALL *)(Callback)>(GetProcAddress(lib,"VSTPluginMain")):nullptr;
 if(!entry && lib)entry=reinterpret_cast<Effect*(BCALL *)(Callback)>(GetProcAddress(lib,"main"));
#else
 auto lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 auto entry=lib?reinterpret_cast<Effect*(*)(Callback)>(dlsym(lib,"VSTPluginMain")):nullptr;
#endif
 if(!entry)throw std::runtime_error("Cannot load plugin or find VSTPluginMain (check architecture and dependencies)");
 Effect* e=entry(host);
 if(!e || e->magic!=0x56737450 || !e->dispatch)throw std::runtime_error("Invalid VST2 effect");
 auto call=[&](int op,intptr_t value=0,void* ptr=nullptr,float opt=0){return e->dispatch(e,op,0,value,ptr,opt);};
 call(0);
 if(argc==3&&std::string(argv[2])=="--probe"){
 char name[128]{};e->dispatch(e,45,0,0,name,0);name[127]=0;
 std::cout<<"VST2 probe ok; id="<<e->id<<" programs="<<e->programs<<" inputs="<<e->inputs<<" outputs="<<e->outputs<<" flags="<<e->flags<<" editor="<<((e->flags&1)?1:0)<<" name="<<name<<"\n";call(1);return ((e->flags&256)||e->inputs==0)&&e->inputs>=0&&e->inputs<=32&&e->outputs>0&&e->outputs<=32&&e->replacing?0:4;
 }

 if(e->outputs<1||e->outputs>32||e->inputs<0||e->inputs>32||!e->replacing)throw std::runtime_error("Unsupported channel count or no processReplacing");
 call(10,0,nullptr,rate);call(11,block);call(2,0);call(12,1);call(71);
 std::vector<std::vector<float>> in(e->inputs,std::vector<float>(block)),out(e->outputs,std::vector<float>(block));
 std::vector<float*> ip,op;for(auto& c:in)ip.push_back(c.data());for(auto& c:out)op.push_back(c.data());
 // VST2 wrappers can defer DSP preparation until their first process call.
 // Give that call an empty MIDI block so the first real key is not discarded.
 auto primeAudio=[&]{for(auto& c:out)std::fill(c.begin(),c.end(),0);e->replacing(e,ip.data(),op.data(),quantum);};
 if(serve){
#ifdef _WIN32
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
 uint32_t hello[3]={wireMagic,uint32_t(e->programs),uint32_t(e->outputs)};
 std::atomic<uint32_t> program{0};std::atomic<bool> needsPrime{true};
#ifdef _WIN32
 EditorHost editor(e);
#endif
 SharedAudio* shared=nullptr;
#ifdef _WIN32
 HANDLE audioFile=INVALID_HANDLE_VALUE,audioMapping=nullptr;
 if(argc==6){
  audioFile=CreateFileA(argv[5],GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
  LARGE_INTEGER size{};if(audioFile!=INVALID_HANDLE_VALUE&&GetFileSizeEx(audioFile,&size)&&size.QuadPart==sizeof(SharedAudio))audioMapping=CreateFileMappingA(audioFile,nullptr,PAGE_READWRITE,0,0,nullptr);
  if(audioMapping)shared=(SharedAudio*)MapViewOfFile(audioMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedAudio));
  if(!shared||shared->magic!=sharedAudioMagic||shared->version!=1||shared->frames!=quantum||shared->slots!=sharedAudioSlots)throw std::runtime_error("Cannot map shared audio transport");
 }
#endif
 if(!shared){fwrite(hello,sizeof(hello),1,stdout);fflush(stdout);}
 std::atomic<bool> done{false};
 const bool measure=std::getenv("BRIDGELAB_TIMING")!=nullptr;
 auto renderAudio=[&](const WireRequest& request){
 if(request.magic!=wireMagic||request.count>maxMidi)throw std::runtime_error("Invalid audio request");
 if(request.frames!=quantum||request.payloadBytes)throw std::runtime_error("Invalid audio packet");
 auto renderStart=measure?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
 WireResponse response;
 {
#ifdef _WIN32
 if(request.program!=program&&request.program<uint32_t(e->programs)){EditorHost::Control select{SelectProgram,{}, {},false};select.program=request.program;editor.request(select);if(select.ok){program=request.program;needsPrime=true;}}
 std::lock_guard<std::recursive_mutex> pluginLock(editor.mutex);
#else
 if(request.program!=program && request.program<uint32_t(e->programs)){program=request.program;call(2,program);needsPrime=true;}
#endif
 if(needsPrime.exchange(false,std::memory_order_acq_rel))primeAudio();
 struct ManyEvents {int32_t count;intptr_t reserved;Midi* items[maxMidi];} events{};
 Midi messages[maxMidi];events.count=request.count;
 for(uint32_t i=0;i<request.count;i++){messages[i].delta=std::min(request.midi[i].offset,uint32_t(quantum-1));messages[i].data[0]=request.midi[i].status;messages[i].data[1]=request.midi[i].note;messages[i].data[2]=request.midi[i].velocity;events.items[i]=&messages[i];}
 if(events.count)call(25,0,&events);
 for(auto& c:out)std::fill(c.begin(),c.end(),0);
 e->replacing(e,ip.data(),op.data(),quantum);
 if(measure){auto micros=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-renderStart).count();response.status=1|(uint32_t(std::min<int64_t>(micros,0x7fffffff))<<1);}
 for(int i=0;i<quantum;i++){response.left[i]=std::isfinite(out[0][i])?out[0][i]:0;response.right[i]=std::isfinite(out[std::min(1,e->outputs-1)][i])?out[std::min(1,e->outputs-1)][i]:0;}
 } // Release the plugin mutex before pipe I/O can block.
 return response;
 };
 auto serveLoop=[&]{
#ifdef _WIN32
 if(processLevel==2&&!shared){
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
  using SetDescription=HRESULT(WINAPI*)(HANDLE,PCWSTR);auto nameThread=(SetDescription)GetProcAddress(GetModuleHandleA("kernel32.dll"),"SetThreadDescription");if(nameThread)nameThread(GetCurrentThread(),L"BridgeLab DSP");
 }
#endif
 try {
 WireRequest request;
 while(fread(&request,sizeof(request),1,stdin)==1){
 if(request.magic!=wireMagic||request.count>maxMidi||request.payloadBytes>maxStateBytes)throw std::runtime_error("Invalid bridge packet");
 if(request.command!=Audio){
 std::vector<uint8_t> payload(request.payloadBytes);if(!payload.empty()&&fread(payload.data(),payload.size(),1,stdin)!=1)break;
 WireResponse response;response.frames=0;response.status=0;
#ifdef _WIN32
 EditorHost::Control control{request.command,std::move(payload),{},false};
 control.program=request.program;control.syncProgram=request.command==GetStateWithProgram&&request.program!=program&&request.program<uint32_t(e->programs);
 const bool asynchronousUI=request.command==OpenEditor||request.command==OpenEmbeddedEditor||request.command==CloseEditor||request.command==EditorInput;
 if(asynchronousUI)control.ok=editor.post(EditorHost::Control{control.command,std::move(control.input),{},false});else editor.request(control);
 if(control.ok&&(request.command==SetState||request.command==SelectProgram||control.syncProgram))needsPrime=true;
 if((request.command==SetState||request.command==GetStateWithProgram||request.command==SelectProgram)&&control.ok)program=request.program;
 response.status=control.ok;response.payloadBytes=uint32_t(control.output.size());
#else
 std::vector<uint8_t> controlOutput;
 if(request.command==GetState){controlOutput=captureState(e);response.status=!controlOutput.empty();}
 if(request.command==SetState)response.status=restoreState(e,payload);
 response.payloadBytes=uint32_t(controlOutput.size());
#endif
 if(fwrite(&response,sizeof(response),1,stdout)!=1)break;
#ifdef _WIN32
 if(response.payloadBytes&&fwrite(control.output.data(),control.output.size(),1,stdout)!=1)break;
#else
 if(response.payloadBytes&&fwrite(controlOutput.data(),controlOutput.size(),1,stdout)!=1)break;
#endif
 fflush(stdout);continue;
 }
 if(shared)throw std::runtime_error("Shared audio must use its mapped queue");
 WireResponse response=renderAudio(request);
 if(fwrite(&response,sizeof(response),1,stdout)!=1)break;fflush(stdout);
 }
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";}
 done=true;
 };
#ifdef _WIN32
 std::thread audio(serveLoop);std::thread mapped;
 if(shared){mapped=std::thread([&]{
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);timeBeginPeriod(1);
  using SetDescription=HRESULT(WINAPI*)(HANDLE,PCWSTR);auto nameThread=(SetDescription)GetProcAddress(GetModuleHandleA("kernel32.dll"),"SetThreadDescription");if(nameThread)nameThread(GetCurrentThread(),L"BridgeLab DSP");
  shared->ready.store(1,std::memory_order_release);
  auto burstStart=std::chrono::steady_clock::now();unsigned burstCount=0;
  try{while(!done&&!shared->stop.load(std::memory_order_acquire)){
   if(shared->pause.load(std::memory_order_acquire)){shared->paused.store(1,std::memory_order_release);Sleep(1);continue;}
   shared->paused.store(0,std::memory_order_release);
   auto r=shared->requestRead.load(std::memory_order_relaxed),w=shared->requestWrite.load(std::memory_order_acquire);
   if(r==w){Sleep(1);burstStart=std::chrono::steady_clock::now();burstCount=0;continue;}
   auto request=shared->requests[r%sharedAudioSlots];shared->requestRead.store(r+1,std::memory_order_release);
   auto response=renderAudio(request.request);
   auto write=shared->responseWrite.load(std::memory_order_relaxed);
   while(write-shared->responseRead.load(std::memory_order_acquire)>=sharedAudioSlots&&!done&&!shared->stop.load(std::memory_order_acquire))Sleep(1);
   if(done||shared->stop.load(std::memory_order_acquire))break;
   auto& slot=shared->responses[write%sharedAudioSlots];slot.seq=request.seq;slot.response=response;
   shared->responseWrite.store(write+1,std::memory_order_release);shared->completed.store(r+1,std::memory_order_release);
   // A mapped queue can remain busy without ever blocking. Bound catch-up
   // work so an overloaded host cannot turn this realtime thread into a CPU
   // hog and trigger macOS's failsafe demotion. Publish audio before yielding.
   if(shared->requestWrite.load(std::memory_order_acquire)-(r+1)>1){
    if(++burstCount>=4||std::chrono::steady_clock::now()-burstStart>=std::chrono::milliseconds(4)){Sleep(1);burstStart=std::chrono::steady_clock::now();burstCount=0;}
   }else{burstStart=std::chrono::steady_clock::now();burstCount=0;}
  }}catch(const std::exception& error){std::cerr<<error.what()<<"\n";shared->error.store(1,std::memory_order_release);done=true;}
  timeEndPeriod(1);
 });
 while(!shared->ready.load(std::memory_order_acquire))Sleep(1);
 fwrite(hello,sizeof(hello),1,stdout);fflush(stdout);
 }
 while(!done){editor.pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,8,QS_ALLINPUT);}
 if(shared)shared->stop.store(1,std::memory_order_release);
 audio.join();if(mapped.joinable())mapped.join();editor.close();
 if(shared)UnmapViewOfFile(shared);if(audioMapping)CloseHandle(audioMapping);if(audioFile!=INVALID_HANDLE_VALUE)CloseHandle(audioFile);
#else
 serveLoop();
#endif
 call(72);call(12,0);call(1);return 0;
 }
 std::vector<int16_t> pcm;pcm.reserve(rate*3*2);double peak=0;bool finite=true;
 primeAudio();
 for(int pos=0;pos<rate*3;pos+=block){int n=std::min(block,rate*3-pos);
 Midi m;Events events; events.items[0]=&m;
 if(pos==0 || (pos<=rate*2 && pos+n>rate*2)){
 m.delta=pos==0?0:rate*2-pos;m.data[0]=pos==0?0x90:0x80;m.data[1]=60;m.data[2]=pos==0?100:0;call(25,0,&events);}
 for(auto& c:out)std::fill(c.begin(),c.end(),0);
 e->replacing(e,ip.data(),op.data(),n);
 for(int i=0;i<n;i++)for(int c=0;c<2;c++){float x=out[std::min(c,e->outputs-1)][i];if(!std::isfinite(x)){finite=false;x=0;}peak=std::max(peak,std::abs(double(x)));pcm.push_back(int16_t(std::max(-1.f,std::min(1.f,x))*32767));}
 }
 call(72);call(12,0);call(1);
 if(!finite)throw std::runtime_error("Plugin generated non-finite audio");
 std::ofstream f(argv[2],std::ios::binary);if(!f)throw std::runtime_error("Cannot create output");
 f.write("RIFF",4);u32(f,36+pcm.size()*2);f.write("WAVEfmt ",8);u32(f,16);u16(f,1);u16(f,2);u32(f,rate);u32(f,rate*4);u16(f,4);u16(f,16);f.write("data",4);u32(f,pcm.size()*2);for(auto s:pcm)u16(f,uint16_t(s));
 if(!f)throw std::runtime_error("Output write failed");
 std::cout<<"Rendered 3 seconds; peak="<<peak<<"; "<<(peak>0?"audio detected":"SILENT: plugin may need a preset or additional host support")<<"\n";
 return peak>0?0:3;
 }catch(const std::exception& ex){std::cerr<<ex.what()<<"\n";return 1;}
}
