#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
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
#include <vector>
#include <pthread.h>
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
 ParameterChanges changes;
 const char* requestedProgram=std::getenv("BRIDGELAB_TEST_PROGRAM");
 if(requestedProgram){int32 index=0;ParamID id=1;auto* queue=changes.addParameterData(id,index);queue->addPoint(0,std::atoi(requestedProgram)/511.0,index);}
 EventList events;ProcessData data{};data.processMode=setup.processMode;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.numSamples=256;data.inputParameterChanges=requestedProgram?&changes:nullptr;
 const int total=std::getenv("BRIDGELAB_TEST_FRAMES")?std::atoi(std::getenv("BRIDGELAB_TEST_FRAMES")):153600;
 if(total<4096||total>48000*120)return 2;const int noteOff=std::getenv("BRIDGELAB_TEST_HELD_NOTE")?total:total/2;
 // Preallocate and touch the complete recording before timed processing.
 // Disk I/O in this loop can stall the fake host and manufacture dropouts.
 std::vector<float> recording(total*2);double peak=0,tail=0;int nonzero=0;
 int qosStatus=0;if(live&&!std::getenv("BRIDGELAB_TEST_NORMAL_PRIORITY"))qosStatus=pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
 auto start=std::chrono::steady_clock::now();long long maxLateUs=0;
 for(int pos=0;pos<total;pos+=block){
 if(live)maxLateUs=std::max(maxLateUs,(long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()-pos*1000000LL/rate);
 data.numSamples=std::min(block,total-pos);events.clear();
 if(pos==0||(pos<=noteOff&&pos+data.numSamples>noteOff)){Event e{};bool on=pos==0;e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;e.busIndex=0;e.sampleOffset=on?0:noteOff-pos;if(on){e.noteOn.pitch=60;e.noteOn.velocity=.8;e.noteOn.noteId=1;}else {e.noteOff.pitch=60;e.noteOff.noteId=1;}events.addEvent(e);}
 if(processor->process(data)!=kResultOk)return 1;
 for(int i=0;i<data.numSamples;i++){if(!std::isfinite(left[i])||!std::isfinite(right[i]))return 1;peak=std::max(peak,double(std::abs(left[i])));if(pos>140000)tail=std::max(tail,double(std::abs(left[i])));if(left[i]!=0)nonzero++;recording[(pos+i)*2]=left[i];recording[(pos+i)*2+1]=right[i];}
 if(live)std::this_thread::sleep_until(start+std::chrono::microseconds((pos+data.numSamples)*1000000LL/rate));
 }
 if(requestedProgram){
  MemoryStream state;if(component->getState(&state)!=kResultOk)return 1;
  state.seek(0,IBStream::kIBSeekSet,nullptr);IBStreamer reader(&state,kLittleEndian);int32 length=0,preset=-1;
  if(!reader.readInt32(length)||length<0||length>32768)return 1;
  std::vector<char> path(length);if(reader.readRaw(path.data(),length)!=length||!reader.readInt32(preset)||preset!=std::atoi(requestedProgram)){std::cerr<<"Preset selection was not retained\n";return 1;}
  state.seek(0,IBStream::kIBSeekSet,nullptr);if(component->setState(&state)!=kResultOk)return 1;
  std::cout<<"Program-list selection and state restore: "<<preset<<" PASS\n";
 }
 processor->setProcessing(false);component->setActive(false);component->terminate();
 std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(recording.data()),recording.size()*sizeof(float));if(!output)return 1;
 std::cout<<(live?"Realtime":"Offline")<<" VST3 → Windows plugin: peak="<<peak<<" nonzero="<<nonzero<<" tail="<<tail<<" rate="<<rate<<" block="<<block<<" latency="<<processor->getLatencySamples()<<" qos_status="<<qosStatus<<" max_timer_lateness_us="<<maxLateUs<<"\n";
 return peak>.001&&nonzero>1000?0:1;
}
