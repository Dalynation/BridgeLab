#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>
#include "public.sdk/source/common/memorystream.h"
using namespace Steinberg;using namespace Steinberg::Vst;
int main(int argc,char** argv){try{
 if(argc!=3)return 2;std::string error,scenario=argv[2];
 auto module=VST3::Hosting::Module::create(argv[1],error);if(!module)throw std::runtime_error(error);
 auto factory=module->getFactory();auto component=factory.createInstance<IComponent>(factory.classInfos()[0].ID());HostApplication host;
 if(!component||component->initialize(&host)!=kResultOk)throw std::runtime_error("initialize");
 FUnknownPtr<IAudioProcessor> processor(component);ProcessSetup setup{};setup.processMode=scenario=="prefetch"?kPrefetch:scenario=="control-pressure"?kRealtime:kOffline;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=256;setup.sampleRate=48000;
 if(processor->setupProcessing(setup)!=kResultOk||component->setActive(true)!=kResultOk)throw std::runtime_error("activate");
 component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);processor->setProcessing(true);
 float left[256]{},right[256]{};float* channels[]{left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;EventList events;ProcessData data{};data.processMode=setup.processMode;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.numSamples=256;
 auto note=[&]{events.clear();Event e{};e.type=Event::kNoteOnEvent;e.noteOn.pitch=60;e.noteOn.velocity=.8;e.noteOn.noteId=1;events.addEvent(e);};
 auto render=[&](int blocks){float peak=0;for(int b=0;b<blocks;b++){if(processor->process(data)!=kResultOk)throw std::runtime_error("process");events.clear();for(float v:left){if(!std::isfinite(v))throw std::runtime_error("nonfinite");peak=std::max(peak,std::abs(v));}}return peak;};
 float peak=0;
 if(scenario=="zero-sample-note"){note();data.numSamples=0;data.numOutputs=0;processor->process(data);events.clear();data.numSamples=256;data.numOutputs=1;peak=render(128);}
 else if(scenario=="repeated-activation"){note();if(render(16)<.001)throw std::runtime_error("initial note silent");if(component->setActive(true)!=kResultOk)throw std::runtime_error("repeated activation");peak=render(16);}
 else if(scenario=="prefetch"){note();render(512);peak=render(64);}
 else if(scenario=="control-pressure"){
  note();auto start=std::chrono::steady_clock::now();std::atomic<bool> done{false};std::atomic<int> captures{0};
  std::thread controls([&]{while(!done){MemoryStream state;if(component->getState(&state)!=kResultOk)break;++captures;}});
  int audible=0;for(int b=0;b<512;++b){float value=render(1);if(b>32&&value>.0001)++audible;std::this_thread::sleep_until(start+std::chrono::microseconds((b+1)*256*1000000LL/48000));}
  done=true;controls.join();std::cout<<"captures="<<captures<<" audible_blocks="<<audible<<"/479 ";peak=audible==479&&captures>0?.1f:0.f;
 }
 else return 2;
 processor->setProcessing(false);component->setActive(false);component->terminate();
 std::cout<<scenario<<" peak="<<peak<<" latency="<<processor->getLatencySamples()<<"\n";return peak>.001?0:1;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
