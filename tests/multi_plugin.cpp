#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <stdexcept>
using namespace Steinberg;using namespace Steinberg::Vst;
struct Instance {
 VST3::Hosting::Module::Ptr module;IPtr<IComponent> component;FUnknownPtr<IAudioProcessor> processor;IPtr<IEditController> controller;IPtr<IPlugView> view;HostApplication host;std::string uid,name,path;
 Instance(const char* bundle,int preset=0,MemoryStream* restore=nullptr){
  std::string error;module=VST3::Hosting::Module::create(bundle,error);if(!module)throw std::runtime_error(error);
  auto factory=module->getFactory();auto info=factory.classInfos()[0];uid=info.ID().toString();name=info.name();component=factory.createInstance<IComponent>(info.ID());
  if(!component||component->initialize(&host)!=kResultOk)throw std::runtime_error("component initialize");
  controller=factory.createInstance<IEditController>(factory.classInfos()[1].ID());
  if(!controller||controller->initialize(&host)!=kResultOk)throw std::runtime_error("controller initialize");
  view=owned(controller->createView(ViewType::kEditor));if(!view||view->isPlatformTypeSupported(kPlatformTypeNSView)!=kResultTrue)throw std::runtime_error("shared editor creation");
  MemoryStream initial;if(component->getState(&initial)!=kResultOk)throw std::runtime_error("initial state");initial.seek(0,IBStream::kIBSeekSet,nullptr);IBStreamer in(&initial,kLittleEndian);int32 n;in.readInt32(n);path.resize(n);in.readRaw(path.data(),n);
  if(restore){restore->seek(0,IBStream::kIBSeekSet,nullptr);if(component->setState(restore)!=kResultOk)throw std::runtime_error("state restore");}
  else if(preset){MemoryStream state;IBStreamer out(&state,kLittleEndian);out.writeInt32(path.size());out.writeRaw(path.data(),path.size());out.writeInt32(preset);state.seek(0,IBStream::kIBSeekSet,nullptr);if(component->setState(&state)!=kResultOk)throw std::runtime_error("preset");}
  processor=FUnknownPtr<IAudioProcessor>(component);
  ProcessSetup setup{};setup.processMode=kOffline;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=256;setup.sampleRate=48000;
  if(processor->setupProcessing(setup)!=kResultOk||component->setActive(true)!=kResultOk)throw std::runtime_error("activation");
  component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);processor->setProcessing(true);
 }
 std::vector<float> render(){std::vector<float> samples;float left[256]{},right[256]{};float* channels[]{left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;EventList events;ProcessData data{};data.processMode=kOffline;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.numSamples=256;
  for(int b=0;b<128;b++){events.clear();if(b==0||b==96){Event e{};e.type=b==0?Event::kNoteOnEvent:Event::kNoteOffEvent;e.busIndex=0;e.sampleOffset=0;if(b==0){e.noteOn.pitch=60;e.noteOn.velocity=.8;e.noteOn.noteId=1;}else{e.noteOff.pitch=60;e.noteOff.noteId=1;}events.addEvent(e);}if(processor->process(data)!=kResultOk)throw std::runtime_error("process");for(int i=0;i<256;i++){if(!std::isfinite(left[i])||!std::isfinite(right[i]))throw std::runtime_error("nonfinite");samples.push_back(left[i]);samples.push_back(right[i]);}}
  float peak=0;for(float f:samples)peak=std::max(peak,std::abs(f));if(peak<.001)throw std::runtime_error("silent");return samples;
 }
 ~Instance(){view=nullptr;if(controller){controller->terminate();controller=nullptr;}if(processor)processor->setProcessing(false);if(component){component->setActive(false);component->terminate();}}
};
int main(int argc,char**argv){try{if(argc!=3)return 2;
 auto a=std::make_unique<Instance>(argv[1]);auto b=std::make_unique<Instance>(argv[2]);auto c=std::make_unique<Instance>(argv[1],7);
 if(a->uid==b->uid||a->path==b->path||a->uid!=c->uid)throw std::runtime_error("plugin identity collision");
 MemoryStream stateA,stateB,stateC;if(a->component->getState(&stateA)!=kResultOk||b->component->getState(&stateB)!=kResultOk||c->component->getState(&stateC)!=kResultOk)throw std::runtime_error("snapshot");
 auto audioA=a->render(),audioB=b->render(),audioC=c->render();if(audioA==audioB||audioA==audioC)throw std::runtime_error("instances not independent");
 std::cout<<"Distinct identities: "<<a->name<<" / "<<b->name<<"\nShared editors created, three active instances, independent sounds: PASS\n";
 a.reset();b.reset();c.reset();
 MemoryStream foreign;stateA.seek(0,IBStream::kIBSeekSet,nullptr);IBStreamer original(&stateA,kLittleEndian);int32 oldLength=0;original.readInt32(oldLength);std::string other="/Users/different-person/old-computer/SQ8L.dll";IBStreamer portable(&foreign,kLittleEndian);portable.writeInt32(other.size());portable.writeRaw(other.data(),other.size());portable.writeRaw(stateA.getData()+4+oldLength,stateA.getSize()-4-oldLength);
 Instance restoredA(argv[1],0,&foreign),restoredB(argv[2],0,&stateB),restoredC(argv[1],0,&stateC);
 if(restoredA.render()!=audioA||restoredB.render()!=audioB||restoredC.render()!=audioC)throw std::runtime_error("state/audio recall mismatch");
 std::cout<<"Three-instance save/recreate/audio recall, including foreign user path: PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
