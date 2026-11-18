#pragma once
#include "../wire.hpp"
#include "config.hpp"
#include "timing.hpp"
#include "../shared_audio.hpp"
#include <sys/mman.h>
#include <new>
#include <array>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstring>
#include <spawn.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <cerrno>
extern char** environ;
struct Packet {uint64_t seq=0;WireRequest request;WireResponse response;};
template<class T,int N> struct Queue {
 std::array<T,N> slots{};std::atomic<uint64_t> read{0},write{0};
 bool push(const T& value){auto w=write.load(std::memory_order_relaxed);if(w-read.load(std::memory_order_acquire)==N)return false;slots[w%N]=value;write.store(w+1,std::memory_order_release);return true;}
 bool pop(T& value){auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;value=slots[r%N];read.store(r+1,std::memory_order_release);return true;}
 void clear(){read=0;write=0;}
};
class Transport {
 SharedAudio* shared=nullptr;std::string sharedPath;
 int input=-1,output=-1;pid_t pid=-1;std::thread worker;std::atomic<bool> stopFlag{false};
 struct ControlJob {uint32_t command,program=0;std::vector<uint8_t> input,output;bool done=false,ok=false;};
 std::mutex controls,offlineIo;std::condition_variable completed;std::shared_ptr<ControlJob> pendingControl;
 bool offlineMode=false;std::atomic<bool> running{false};std::atomic<uint64_t> lastCompleted{0};
 std::atomic<bool> workerReady{false},pipeScheduler{false};
 static bool enablePipeRealtime(){
  mach_timebase_info_data_t timebase{};if(mach_timebase_info(&timebase)!=KERN_SUCCESS)return false;
  auto ticks=[&](uint64_t nanos){return uint32_t(nanos*timebase.denom/timebase.numer);};
  // Only the small pipe I/O worker opts in. GUI/control-only shared workers
  // retain QoS scheduling, and no process-wide policy is changed.
  thread_time_constraint_policy_data_t policy{};policy.period=ticks(10000000);policy.computation=ticks(1000000);policy.constraint=ticks(10000000);policy.preemptible=TRUE;
  auto thread=mach_thread_self();auto status=thread_policy_set(thread,THREAD_TIME_CONSTRAINT_POLICY,reinterpret_cast<thread_policy_t>(&policy),THREAD_TIME_CONSTRAINT_POLICY_COUNT);
  if(status==KERN_SUCCESS){boolean_t isDefault=FALSE;mach_msg_type_number_t count=THREAD_TIME_CONSTRAINT_POLICY_COUNT;status=thread_policy_get(thread,THREAD_TIME_CONSTRAINT_POLICY,reinterpret_cast<thread_policy_t>(&policy),&count,&isDefault);if(isDefault)status=KERN_FAILURE;}
  mach_port_deallocate(mach_task_self(),thread);return status==KERN_SUCCESS;
 }
 bool createShared(){
 std::string pattern=(getenv("TMPDIR")?getenv("TMPDIR"):"/tmp/");if(pattern.empty())pattern="/tmp/";if(pattern.back()!='/')pattern+='/';pattern+="BridgeLab-audio-XXXXXX";
 std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0)return false;
 bool ok=ftruncate(fd,sizeof(SharedAudio))==0;void* data=ok?mmap(nullptr,sizeof(SharedAudio),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0):MAP_FAILED;close(fd);
 if(data==MAP_FAILED){unlink(name.data());return false;}
 sharedPath=name.data();shared=new(data)SharedAudio();return true;
 }
 template<class Predicate> bool waitShared(Predicate done){
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(32);
 while(!done()){if(stopFlag||shared->error.load(std::memory_order_acquire)||std::chrono::steady_clock::now()>deadline)return false;std::this_thread::sleep_for(std::chrono::microseconds(100));}return true;
 }
 bool sharedControlBarrier(){
 uint32_t target=shared->requestWrite.load(std::memory_order_acquire);
 if(!waitShared([&]{return int32_t(shared->completed.load(std::memory_order_acquire)-target)>=0;}))return false;
 shared->pause.store(1,std::memory_order_release);
 if(!waitShared([&]{return shared->paused.load(std::memory_order_acquire)!=0;})){shared->pause.store(0,std::memory_order_release);return false;}
 return true;
 }
 bool performControl(ControlJob& job){
 // State replacement must fence future DSP. Read-only snapshots and UI
 // requests use the plugin mutex in the runner, without pausing audio while
 // the Windows message thread waits for its next dispatch.
 if(shared&&(job.command==GetState||job.command==GetStateWithProgram)){
  uint32_t target=shared->requestWrite.load(std::memory_order_acquire);
  if(!waitShared([&]{return int32_t(shared->completed.load(std::memory_order_acquire)-target)>=0;}))return false;
 }
 bool pauseAudio=shared&&(job.command==SetState||job.command==SelectProgram);
 if(pauseAudio&&!sharedControlBarrier())return false;
 struct Resume{Transport* owner;bool paused;~Resume(){if(paused){owner->shared->pause.store(0,std::memory_order_release);owner->waitShared([&]{return owner->shared->paused.load(std::memory_order_acquire)==0;});}}} resume{this,pauseAudio};
 WireRequest request;request.frames=0;request.command=job.command;request.program=job.program;request.payloadBytes=uint32_t(job.input.size());WireResponse response;
 if(!io(input,&request,sizeof(request),true)||(!job.input.empty()&&!io(input,job.input.data(),job.input.size(),true))||!io(output,&response,sizeof(response),false,30000)||response.magic!=wireMagic||response.frames!=0||response.payloadBytes>maxStateBytes){failed=true;return false;}
 job.output.resize(response.payloadBytes);if(!job.output.empty()&&!io(output,job.output.data(),job.output.size(),false,30000)){failed=true;return false;}
 return response.status==1;
 }
 bool io(int fd,void* data,size_t bytes,bool writing,int timeout=5000){
 auto* p=static_cast<char*>(data);
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
 while(bytes && !stopFlag){
 auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();if(remaining<=0)return false;
 pollfd f{fd,short(writing?POLLOUT:POLLIN),0};int ready=poll(&f,1,int(std::min<int64_t>(remaining,100)));
 if(ready<0&&errno==EINTR)continue;if(ready==0)continue;if(ready<0||!(f.revents&(writing?POLLOUT:POLLIN)))return false;
 auto n=writing?::write(fd,p,bytes): ::read(fd,p,bytes);if(n<0&&(errno==EINTR||errno==EAGAIN))continue;if(n<=0)return false;p+=n;bytes-=n;}
 return bytes==0;
 }
public:
 bool usesSharedAudio()const{return shared!=nullptr;}
 bool usesRealtimeScheduler()const{return shared&&shared->scheduler.load(std::memory_order_acquire)==1;}
 bool usesRealtimeIO()const{return !shared&&pipeScheduler.load(std::memory_order_acquire);}
 TimingTrace timing;
 Queue<Packet,128> requests,responses;std::atomic<bool> failed{false};uint32_t programs=1;
 ~Transport(){stop();}
 bool start(const std::string& path,int bits,double rate,bool offline,bool allowShared=true){
 stop();timing.begin();offlineMode=offline;stopFlag=false;failed=false;lastCompleted=0;workerReady=false;pipeScheduler=false;requests.clear();responses.clear();
 int toChild[2],fromChild[2];if(pipe(toChild))return false;if(pipe(fromChild)){close(toChild[0]);close(toChild[1]);return false;}
 // Avoid inherited descriptors holding another instance's pipes open.
 for(int* fd:{&toChild[0],&toChild[1],&fromChild[0],&fromChild[1]}){int duplicate=fcntl(*fd,F_DUPFD_CLOEXEC,3);if(duplicate<0)return false;close(*fd);*fd=duplicate;}
 std::string runtime=bridgeHome()+"/engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64";
 std::string runner=bridgeHome()+"/engine/bin/runner-"+(bits==32?"x86":"x64")+".exe";
 std::string dll="Z:"+path;std::string sampleRate=std::to_string(int(rate));
 if(!offline&&allowShared&&!getenv("BRIDGELAB_PIPE_AUDIO"))createShared();
 std::string mapWindows=shared?"Z:"+sharedPath:"";
 std::array<char*,8> args{runtime.data(),runner.data(),dll.data(),const_cast<char*>("--serve"),sampleRate.data(),const_cast<char*>(offline?"offline":"realtime"),shared?mapWindows.data():nullptr,nullptr};
 std::vector<std::string> env;
 // DAWs may inject loader/runtime variables. Pass only the user runtime basics.
 for(const char* key:{"HOME","USER","LOGNAME","PATH","TMPDIR","LANG","LC_ALL"})if(const char* value=getenv(key))env.emplace_back(std::string(key)+"="+value);
 env.emplace_back(std::string("WINEPREFIX=")+bridgeHome()+"/prefixes/"+bridgeRuntimeProfile(bits));env.emplace_back("WINEDEBUG=-all");if(timing.enabled())env.emplace_back("BRIDGELAB_TIMING=1");env.emplace_back("MVK_CONFIG_LOG_LEVEL=0");env.emplace_back("WINE_D3D_CONFIG=renderer=gl,shader_backend=glsl");
 if(!offline){std::string helper=bridgeHome()+"/engine/libBridgeLabRealtime.dylib";if(std::filesystem::exists(helper)){env.emplace_back("DYLD_INSERT_LIBRARIES="+helper);if(shared)env.emplace_back("BRIDGELAB_AUDIO_MAPPING="+sharedPath);else env.emplace_back("BRIDGELAB_REALTIME_PIPE=1");}}
 std::vector<char*> envp;for(auto& s:env)envp.push_back(s.data());envp.push_back(nullptr);
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
 posix_spawn_file_actions_adddup2(&actions,toChild[0],STDIN_FILENO);posix_spawn_file_actions_adddup2(&actions,fromChild[1],STDOUT_FILENO);
 std::string launchLog=bridgeHome()+"/logs/runner-launch.log";posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,launchLog.c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
 int err=posix_spawn(&pid,runtime.c_str(),&actions,nullptr,args.data(),envp.data());posix_spawn_file_actions_destroy(&actions);
 close(toChild[0]);close(fromChild[1]);input=toChild[1];output=fromChild[0];fcntl(input,F_SETNOSIGPIPE,1);
 fcntl(input,F_SETFL,fcntl(input,F_GETFL)|O_NONBLOCK);fcntl(output,F_SETFL,fcntl(output,F_GETFL)|O_NONBLOCK);
 if(err){pid=-1;stop();return false;}
 uint32_t hello[3]{};if(!io(output,hello,sizeof(hello),false,90000)||hello[0]!=wireMagic||(shared&&!shared->ready.load(std::memory_order_acquire))){bool fallback=shared!=nullptr;stop();return fallback&&allowShared?start(path,bits,rate,offline,false):false;}programs=hello[1];
 running=true;
 if(!offline)worker=std::thread([this]{
 pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
 if(!shared)pipeScheduler.store(enablePipeRealtime(),std::memory_order_release);
 workerReady.store(true,std::memory_order_release);completed.notify_all();
 std::array<Packet,8> batch;
 while(!stopFlag){
 std::shared_ptr<ControlJob> job;{std::unique_lock<std::mutex> lock(controls);if(shared)completed.wait(lock,[&]{return stopFlag||bool(pendingControl);});if(stopFlag)break;job=pendingControl;}
 if(job){bool ok=performControl(*job);{std::lock_guard<std::mutex> lock(controls);job->ok=ok;job->done=true;if(pendingControl==job)pendingControl.reset();}completed.notify_all();if(failed)break;}
 if(shared)continue;
 // A UI/state request can take several audio quanta in the Windows message
 // loop. Drain a bounded batch afterwards; one packet per control job lets
 // sustained mouse/state traffic starve audio and eventually fill the queue.
 size_t count=0;
 while(count<batch.size()&&requests.pop(batch[count]))++count;
 if(count&&!exchangeBatch(batch.data(),count)){failed=true;break;}
 if(!count)std::this_thread::sleep_for(std::chrono::microseconds(100));}
 running=false;completed.notify_all();});
 if(!offline&&!shared){std::unique_lock<std::mutex> lock(controls);completed.wait_for(lock,std::chrono::seconds(1),[&]{return workerReady.load(std::memory_order_acquire)||!running;});}
 return true;
 }
 bool exchangeBatch(Packet* batch,size_t count){
 // Full-duplex, bounded pipelining: neither pipe's capacity is assumed.
 // A response may be drained while a later request is only partially written.
 // Control jobs execute only after every response in this batch is collected.
 size_t sent=0,received=0,writeOffset=0,readOffset=0;
 std::array<uint64_t,8> started{};
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(received<count&&!stopFlag){
  auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
  if(remaining<=0)return false;
  pollfd fds[2]{{input,short(sent<count?POLLOUT:0),0},{output,POLLIN,0}};
  int status=poll(fds,2,int(std::min<int64_t>(remaining,100)));if(status<0&&errno==EINTR)continue;if(status==0)continue;if(status<0)return false;
  if(fds[0].revents&POLLOUT){
   if(!writeOffset&&timing.enabled())started[sent]=TimingTrace::now();
   auto n=::write(input,reinterpret_cast<char*>(&batch[sent].request)+writeOffset,sizeof(WireRequest)-writeOffset);
   if(n>0){writeOffset+=size_t(n);if(writeOffset==sizeof(WireRequest)){++sent;writeOffset=0;}}
   else if(n==0||(errno!=EINTR&&errno!=EAGAIN))return false;
  }
  if(fds[1].revents&POLLIN){
   auto n=::read(output,reinterpret_cast<char*>(&batch[received].response)+readOffset,sizeof(WireResponse)-readOffset);
   if(n>0){readOffset+=size_t(n);if(readOffset==sizeof(WireResponse)){
    auto& packet=batch[received];if(packet.response.magic!=wireMagic||packet.response.frames!=quantum)return false;
    // A paused host can leave this queue full. Apply backpressure on this
    // worker instead of declaring a healthy Windows instrument dead.
    auto blocked=std::chrono::steady_clock::now();
    while(!stopFlag&&!responses.push(packet))std::this_thread::sleep_for(std::chrono::microseconds(100));
    if(stopFlag)return false;deadline+=std::chrono::steady_clock::now()-blocked;
    if(timing.enabled())timing.record('R',packet.seq,started[received],TimingTrace::now()-started[received],count,packet.response.status>>1);
    {std::lock_guard<std::mutex> lock(controls);lastCompleted=packet.seq+1;}completed.notify_all();
    ++received;readOffset=0;
   }}else if(n==0||(errno!=EINTR&&errno!=EAGAIN))return false;
  }
  if((fds[0].revents&(POLLERR|POLLNVAL))||(fds[1].revents&(POLLERR|POLLNVAL)))return false;
  if((fds[1].revents&POLLHUP)&&!(fds[1].revents&POLLIN))return false;
 }
 return received==count;
 }
 bool receive(Packet& packet){
 if(!shared)return responses.pop(packet);
 auto r=shared->responseRead.load(std::memory_order_relaxed),w=shared->responseWrite.load(std::memory_order_acquire);if(r==w)return false;
 const auto& slot=shared->responses[r%sharedAudioSlots];packet.seq=slot.seq;packet.response=slot.response;
 shared->responseRead.store(r+1,std::memory_order_release);
 if(timing.enabled())timing.record('R',packet.seq,TimingTrace::now(),0,w-r,packet.response.status>>1);
 return true;
 }
 bool submit(const Packet& packet,bool prefetch){
 if(shared){
  if(failed||!running||shared->error.load(std::memory_order_acquire)){failed=true;return false;}
  auto w=shared->requestWrite.load(std::memory_order_relaxed),r=shared->requestRead.load(std::memory_order_acquire);if(w-r>=sharedAudioSlots)return false;
  auto& slot=shared->requests[w%sharedAudioSlots];slot.seq=packet.seq;slot.request=packet.request;shared->requestWrite.store(w+1,std::memory_order_release);
  if(timing.enabled())timing.record('S',packet.seq,TimingTrace::now(),0,w-r+1,1);
  return !prefetch||waitShared([&]{return int32_t(shared->completed.load(std::memory_order_acquire)-(w+1))>=0;});
 }
 if(failed||!running){failed=true;return false;}
 if(!requests.push(packet))return false;
 if(timing.enabled())timing.record('S',packet.seq,TimingTrace::now(),0,requests.write.load()-requests.read.load(),0);
 if(!prefetch)return true;
 // VST3 permits prefetch callbacks to run faster than wall clock and switch
 // from realtime without setupProcessing. Keep one worker/ordered pipe, but
 // wait here (only for prefetch) so a fast host cannot overflow the queue.
 std::unique_lock<std::mutex> lock(controls);
 return completed.wait_for(lock,std::chrono::seconds(32),[&]{return lastCompleted>packet.seq||failed||!running;})&&lastCompleted>packet.seq&&!failed;
 }
 bool control(uint32_t command,const std::vector<uint8_t>& bytes,std::vector<uint8_t>& result,uint32_t hostProgram=0){
 if(!running||failed||bytes.size()>maxStateBytes)return false;
 auto job=std::make_shared<ControlJob>();job->command=command;job->program=hostProgram;job->input=bytes;
 if(offlineMode){std::lock_guard<std::mutex> lock(offlineIo);bool ok=performControl(*job);result=std::move(job->output);return ok;}
 std::unique_lock<std::mutex> lock(controls);
 if(!completed.wait_for(lock,std::chrono::seconds(32),[&]{return !pendingControl||!running;} )||!running)return false;
 pendingControl=job;completed.notify_all();
 if(!completed.wait_for(lock,std::chrono::seconds(32),[&]{return job->done||!running;} )||!job->done)return false;
 result=std::move(job->output);return job->ok;
 }
 bool exchange(Packet& packet){auto started=timing.enabled()?TimingTrace::now():0;std::unique_lock<std::mutex> lock(offlineIo,std::defer_lock);if(offlineMode)lock.lock();if(!io(input,&packet.request,sizeof(WireRequest),true)||!io(output,&packet.response,sizeof(WireResponse),false)||packet.response.magic!=wireMagic||packet.response.frames!=quantum){failed=true;return false;}if(timing.enabled())timing.record('R',packet.seq,started,TimingTrace::now()-started);return true;}
 // Close stdin and allow the runner to tear down Wine normally before forced exit.
 void stop(){stopFlag=true;if(shared)shared->stop.store(1,std::memory_order_release);running=false;completed.notify_all();if(worker.joinable())worker.join();if(input>=0)close(input);if(output>=0)close(output);input=output=-1;if(pid>0){bool reaped=false;for(int attempt=0;attempt<100;++attempt){if(waitpid(pid,nullptr,WNOHANG)!=0){reaped=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}if(!reaped){kill(pid,SIGKILL);waitpid(pid,nullptr,0);}}pid=-1;{std::lock_guard<std::mutex> lock(controls);pendingControl.reset();}completed.notify_all();if(shared){munmap(shared,sizeof(SharedAudio));shared=nullptr;unlink(sharedPath.c_str());sharedPath.clear();}timing.finish();}
};
