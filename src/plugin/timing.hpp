#pragma once
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
// Opt-in diagnostics: no allocation or file writes in the audio callback.
class TimingTrace {
 struct Event {char kind=0;uint64_t seq=0,start=0,duration=0,a=0,b=0;};
 static constexpr size_t capacity=16384;
 std::unique_ptr<Event[]> events;std::atomic<size_t> count{0};std::string path;
public:
 static uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 bool enabled()const{return bool(events);}
 void begin(){if(const char* prefix=getenv("BRIDGELAB_TIMING_TRACE")){events=std::make_unique<Event[]>(capacity);count=0;path=std::string(prefix)+"-"+std::to_string(getpid())+"-"+std::to_string(reinterpret_cast<uintptr_t>(this))+".csv";}}
 void record(char kind,uint64_t seq,uint64_t start,uint64_t duration=0,uint64_t a=0,uint64_t b=0){if(!events)return;auto i=count.fetch_add(1,std::memory_order_relaxed);if(i<capacity)events[i]={kind,seq,start,duration,a,b};}
 void finish(){if(!events)return;std::ofstream out(path);out<<"kind,seq,start_ns,duration_ns,a,b\n";for(size_t i=0,n=std::min(count.load(),capacity);i<n;++i){auto& e=events[i];out<<e.kind<<','<<e.seq<<','<<e.start<<','<<e.duration<<','<<e.a<<','<<e.b<<'\n';}events.reset();count=0;}
};
