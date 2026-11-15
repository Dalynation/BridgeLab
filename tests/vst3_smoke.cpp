#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cmath>
#include <thread>
#include <chrono>
using namespace Steinberg;using namespace Steinberg::Vst;
int main(int argc,char**argv){
 if(argc<3)return 2;bool live=argc>3;std::string error;
 auto module=VST3::Hosting::Module::create(argv[1],error);if(!module){std::cerr<<error;return 1;}
 auto factory=module->getFactory();auto classes=factory.classInfos();if(classes.empty())return 1;
 auto component=factory.createInstance<IComponent>(classes[0].ID());HostApplication host;
 if(!component||component->initialize(&host)!=kResultOk){std::cerr<<"initialize failed";return 1;}
 if(const char* dll=std::getenv("BRIDGELAB_TEST_DLL")){
 MemoryStream state;IBStreamer writer(&state,kLittleEndian);std::string path=dll;
 writer.writeInt32(path.size());writer.writeRaw(path.data(),path.size());writer.writeInt32(0);state.seek(0,IBStream::kIBSeekSet,nullptr);
 if(component->setState(&state)!=kResultOk){std::cerr<<"state load failed";return 1;}
 MemoryStream saved;if(component->getState(&saved)!=kResultOk){std::cerr<<"state save failed";return 1;}
 saved.seek(0,IBStream::kIBSeekSet,nullptr);if(component->setState(&saved)!=kResultOk){std::cerr<<"state roundtrip failed";return 1;}
 }
 const int block=std::getenv("BRIDGELAB_TEST_BLOCK")?std::atoi(std::getenv("BRIDGELAB_TEST_BLOCK")):256;
 const int rate=std::getenv("BRIDGELAB_TEST_RATE")?std::atoi(std::getenv("BRIDGELAB_TEST_RATE")):48000;
 if(block<1||block>4096)return 2;
 FUnknownPtr<IAudioProcessor> processor(component);
 ProcessSetup setup{};setup.processMode=live?kRealtime:kOffline;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=block;setup.sampleRate=rate;
 if(processor->setupProcessing(setup)!=kResultOk||component->setActive(true)!=kResultOk){std::cerr<<"activation failed";component->terminate();return 1;}
 component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);processor->setProcessing(true);
 float left[4096]{},right[4096]{};float* channels[2]{left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
 EventList events;ProcessData data{};data.processMode=setup.processMode;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.numSamples=256;
 std::ofstream output(argv[2],std::ios::binary);double peak=0,tail=0;int nonzero=0;auto start=std::chrono::steady_clock::now();
 const int total=153600,noteOff=76800;
 for(int pos=0;pos<total;pos+=block){
 data.numSamples=std::min(block,total-pos);events.clear();
 if(pos==0||(pos<=noteOff&&pos+data.numSamples>noteOff)){Event e{};bool on=pos==0;e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;e.busIndex=0;e.sampleOffset=on?0:noteOff-pos;if(on){e.noteOn.pitch=60;e.noteOn.velocity=.8;e.noteOn.noteId=1;}else {e.noteOff.pitch=60;e.noteOff.noteId=1;}events.addEvent(e);}
 if(processor->process(data)!=kResultOk)return 1;
 for(int i=0;i<data.numSamples;i++){if(!std::isfinite(left[i])||!std::isfinite(right[i]))return 1;peak=std::max(peak,double(std::abs(left[i])));if(pos>140000)tail=std::max(tail,double(std::abs(left[i])));if(left[i]!=0)nonzero++;output.write((char*)&left[i],4);output.write((char*)&right[i],4);}
 if(live)std::this_thread::sleep_until(start+std::chrono::microseconds((pos+data.numSamples)*1000000LL/rate));
 }
 processor->setProcessing(false);component->setActive(false);component->terminate();
 std::cout<<(live?"Realtime":"Offline")<<" VST3 → Windows plugin: peak="<<peak<<" nonzero="<<nonzero<<" tail="<<tail<<" rate="<<rate<<" block="<<block<<" latency="<<processor->getLatencySamples()<<"\n";
 return peak>.001&&nonzero>1000?0:1;
}
