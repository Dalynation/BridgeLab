#include "shared_audio.hpp"
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
// Wine 8 does not provide the newer Mac thread-priority mapping. Only the
// explicitly named bridge DSP worker opts in; UI/control threads are untouched.
static int bridge_setname(const char* name){
 int result=pthread_setname_np(name);
 if(!name||std::strcmp(name,"BridgeLab DSP"))return result;
 SharedAudio* shared=nullptr;
 const char* path=std::getenv("BRIDGELAB_AUDIO_MAPPING");
 if(path){
  int fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return result;
  struct stat info{};if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()||info.st_size!=sizeof(SharedAudio)){close(fd);return result;}
  void* data=mmap(nullptr,sizeof(SharedAudio),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);if(data==MAP_FAILED)return result;
  shared=static_cast<SharedAudio*>(data);
  if(shared->magic!=sharedAudioMagic||shared->version!=1){munmap(data,sizeof(SharedAudio));return result;}
 }else{
  const char* pipe=std::getenv("BRIDGELAB_REALTIME_PIPE");if(!pipe||std::strcmp(pipe,"1"))return result;
 }
 mach_timebase_info_data_t timebase{};kern_return_t status=mach_timebase_info(&timebase);
 if(status==KERN_SUCCESS){
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
  auto ticks=[&](uint64_t nanos){return uint32_t(nanos*timebase.denom/timebase.numer);};
  thread_time_constraint_policy_data_t policy{};policy.period=ticks(10000000);policy.computation=ticks(5000000);policy.constraint=ticks(10000000);policy.preemptible=TRUE;
  auto thread=mach_thread_self();status=thread_policy_set(thread,THREAD_TIME_CONSTRAINT_POLICY,reinterpret_cast<thread_policy_t>(&policy),THREAD_TIME_CONSTRAINT_POLICY_COUNT);
  if(status==KERN_SUCCESS){boolean_t getDefault=FALSE;mach_msg_type_number_t count=THREAD_TIME_CONSTRAINT_POLICY_COUNT;status=thread_policy_get(thread,THREAD_TIME_CONSTRAINT_POLICY,reinterpret_cast<thread_policy_t>(&policy),&count,&getDefault);if(getDefault)status=KERN_FAILURE;}
  mach_port_deallocate(mach_task_self(),thread);
 }
 if(shared){shared->scheduler.store(status==KERN_SUCCESS?1:2,std::memory_order_release);munmap(shared,sizeof(SharedAudio));}
 else std::fprintf(stderr,"BridgeLab pipe DSP pid=%d realtime_policy=%d\n",getpid(),status==KERN_SUCCESS?1:0);
 return result;
}
__attribute__((used)) static struct {const void* replacement;const void* original;} bridge_interpose
 __attribute__((section("__DATA,__interpose")))={reinterpret_cast<const void*>(bridge_setname),reinterpret_cast<const void*>(pthread_setname_np)};
