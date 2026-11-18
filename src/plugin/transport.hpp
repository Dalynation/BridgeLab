#pragma once
#include "../wire.hpp"
#include "config.hpp"
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
extern char** environ;
struct Packet {uint64_t seq=0;WireRequest request;WireResponse response;};
template<class T,int N> struct Queue {
 std::array<T,N> slots{};std::atomic<uint64_t> read{0},write{0};
 bool push(const T& value){auto w=write.load(std::memory_order_relaxed);if(w-read.load(std::memory_order_acquire)==N)return false;slots[w%N]=value;write.store(w+1,std::memory_order_release);return true;}
 bool pop(T& value){auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;value=slots[r%N];read.store(r+1,std::memory_order_release);return true;}
 void clear(){read=0;write=0;}
};
class Transport {
 int input=-1,output=-1;pid_t pid=-1;std::thread worker;std::atomic<bool> stopFlag{false};
 struct ControlJob {uint32_t command,program=0;std::vector<uint8_t> input,output;bool done=false,ok=false;};
 std::mutex controls,offlineIo;std::condition_variable completed;std::shared_ptr<ControlJob> pendingControl;
 bool offlineMode=false;std::atomic<bool> running{false};std::atomic<uint64_t> lastCompleted{0};
 bool performControl(ControlJob& job){
 WireRequest request;request.frames=0;request.command=job.command;request.program=job.program;request.payloadBytes=uint32_t(job.input.size());WireResponse response;
 if(!io(input,&request,sizeof(request),true)||(!job.input.empty()&&!io(input,job.input.data(),job.input.size(),true))||!io(output,&response,sizeof(response),false)||response.magic!=wireMagic||response.frames!=0||response.payloadBytes>maxStateBytes){failed=true;return false;}
 job.output.resize(response.payloadBytes);if(!job.output.empty()&&!io(output,job.output.data(),job.output.size(),false)){failed=true;return false;}
 return response.status==1;
 }
 bool io(int fd,void* data,size_t bytes,bool writing,int timeout=5000){
 auto* p=static_cast<char*>(data);
 while(bytes && !stopFlag){pollfd f{fd,short(writing?POLLOUT:POLLIN),0};int status=poll(&f,1,timeout);if(status<=0||!(f.revents&(writing?POLLOUT:POLLIN)))return false;
 auto n=writing?::write(fd,p,bytes): ::read(fd,p,bytes);if(n<=0)return false;p+=n;bytes-=n;}
 return bytes==0;
 }
public:
 Queue<Packet,128> requests,responses;std::atomic<bool> failed{false};uint32_t programs=1;
 ~Transport(){stop();}
 bool start(const std::string& path,int bits,double rate,bool offline){
 stop();offlineMode=offline;stopFlag=false;failed=false;lastCompleted=0;requests.clear();responses.clear();
 int toChild[2],fromChild[2];if(pipe(toChild))return false;if(pipe(fromChild)){close(toChild[0]);close(toChild[1]);return false;}
 // Avoid inherited descriptors holding another instance's pipes open.
 for(int* fd:{&toChild[0],&toChild[1],&fromChild[0],&fromChild[1]}){int duplicate=fcntl(*fd,F_DUPFD_CLOEXEC,3);if(duplicate<0)return false;close(*fd);*fd=duplicate;}
 std::string runtime=bridgeHome()+"/engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wine64";
 std::string runner=bridgeHome()+"/engine/bin/runner-"+(bits==32?"x86":"x64")+".exe";
 std::string dll="Z:"+path;std::string sampleRate=std::to_string(int(rate));
 std::array<char*,7> args{runtime.data(),runner.data(),dll.data(),const_cast<char*>("--serve"),sampleRate.data(),const_cast<char*>(offline?"offline":"realtime"),nullptr};
 std::vector<std::string> env;
 // DAWs may inject loader/runtime variables. Pass only the user runtime basics.
 for(const char* key:{"HOME","USER","LOGNAME","PATH","TMPDIR","LANG","LC_ALL"})if(const char* value=getenv(key))env.emplace_back(std::string(key)+"="+value);
 env.emplace_back(std::string("WINEPREFIX=")+bridgeHome()+(bits==32?"/prefixes/crossover-x86":"/prefixes/crossover-x64"));env.emplace_back("WINEDEBUG=-all");env.emplace_back("MVK_CONFIG_LOG_LEVEL=0");
 std::vector<char*> envp;for(auto& s:env)envp.push_back(s.data());envp.push_back(nullptr);
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
 posix_spawn_file_actions_adddup2(&actions,toChild[0],STDIN_FILENO);posix_spawn_file_actions_adddup2(&actions,fromChild[1],STDOUT_FILENO);
 std::string launchLog=bridgeHome()+"/logs/runner-launch.log";posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,launchLog.c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
 int err=posix_spawn(&pid,runtime.c_str(),&actions,nullptr,args.data(),envp.data());posix_spawn_file_actions_destroy(&actions);
 close(toChild[0]);close(fromChild[1]);input=toChild[1];output=fromChild[0];fcntl(input,F_SETNOSIGPIPE,1);
 if(err){pid=-1;stop();return false;}
 uint32_t hello[3]{};if(!io(output,hello,sizeof(hello),false,90000)||hello[0]!=wireMagic){stop();return false;}programs=hello[1];
 running=true;
 if(!offline)worker=std::thread([this]{
 pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
 Packet packet;while(!stopFlag){
 std::shared_ptr<ControlJob> job;{std::lock_guard<std::mutex> lock(controls);job=pendingControl;}
 if(job){bool ok=performControl(*job);{std::lock_guard<std::mutex> lock(controls);job->ok=ok;job->done=true;if(pendingControl==job)pendingControl.reset();}completed.notify_all();if(failed)break;}
 // A UI/state request can take several audio quanta in the Windows message
 // loop. Drain a bounded batch afterwards; one packet per control job lets
 // sustained mouse/state traffic starve audio and eventually fill the queue.
 bool processed=false;
 for(int batch=0;batch<8&&!stopFlag&&requests.pop(packet);++batch){
 processed=true;if(!exchange(packet)||!responses.push(packet)){failed=true;break;}
 {std::lock_guard<std::mutex> lock(controls);lastCompleted=packet.seq+1;}completed.notify_all();
 }
 if(failed)break;
 if(!processed)std::this_thread::sleep_for(std::chrono::microseconds(100));}
 running=false;completed.notify_all();});
 return true;
 }
 bool submit(const Packet& packet,bool prefetch){
 if(failed||!running||!requests.push(packet))return false;
 if(!prefetch)return true;
 // VST3 permits prefetch callbacks to run faster than wall clock and switch
 // from realtime without setupProcessing. Keep one worker/ordered pipe, but
 // wait here (only for prefetch) so a fast host cannot overflow the queue.
 std::unique_lock<std::mutex> lock(controls);
 return completed.wait_for(lock,std::chrono::seconds(6),[&]{return lastCompleted>packet.seq||failed||!running;})&&lastCompleted>packet.seq&&!failed;
 }
 bool control(uint32_t command,const std::vector<uint8_t>& bytes,std::vector<uint8_t>& result,uint32_t hostProgram=0){
 if(!running||failed||bytes.size()>maxStateBytes)return false;
 auto job=std::make_shared<ControlJob>();job->command=command;job->program=hostProgram;job->input=bytes;
 if(offlineMode){std::lock_guard<std::mutex> lock(offlineIo);bool ok=performControl(*job);result=std::move(job->output);return ok;}
 std::unique_lock<std::mutex> lock(controls);
 if(!completed.wait_for(lock,std::chrono::seconds(6),[&]{return !pendingControl||!running;} )||!running)return false;
 pendingControl=job;
 if(!completed.wait_for(lock,std::chrono::seconds(6),[&]{return job->done||!running;} )||!job->done)return false;
 result=std::move(job->output);return job->ok;
 }
 bool exchange(Packet& packet){std::unique_lock<std::mutex> lock(offlineIo,std::defer_lock);if(offlineMode)lock.lock();if(!io(input,&packet.request,sizeof(WireRequest),true)||!io(output,&packet.response,sizeof(WireResponse),false)||packet.response.magic!=wireMagic||packet.response.frames!=quantum){failed=true;return false;}return true;}
 void stop(){stopFlag=true;running=false;completed.notify_all();if(pid>0)kill(pid,SIGTERM);if(worker.joinable())worker.join();if(input>=0)close(input);if(output>=0)close(output);input=output=-1;if(pid>0){bool reaped=false;for(int attempt=0;attempt<50;++attempt){if(waitpid(pid,nullptr,WNOHANG)!=0){reaped=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}if(!reaped){kill(pid,SIGKILL);waitpid(pid,nullptr,0);}}pid=-1;{std::lock_guard<std::mutex> lock(controls);pendingControl.reset();}completed.notify_all();}
};
