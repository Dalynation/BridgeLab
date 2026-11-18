#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include <AudioUnit/AudioUnit.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include "device_editor.hpp"
using namespace Steinberg;using namespace Steinberg::Vst;
struct RenderState {
 IAudioProcessor* processor;int total,block,pos=0,maxSlice=0;uint64_t credit=0;float left[4096]{},right[4096]{};float* channels[2]{left,right};
 AudioBusBuffers bus{};EventList events;ProcessData data{};std::vector<float> recording;std::atomic<bool> done{false};bool error=false,realtimePolicy=false,checkedPolicy=false;
 explicit RenderState(IAudioProcessor* p,int frames,int blockSize):processor(p),total(frames),block(blockSize),recording(frames*2){bus.numChannels=2;bus.channelBuffers32=channels;data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;}
 static OSStatus callback(void* context,AudioUnitRenderActionFlags*,const AudioTimeStamp*,UInt32,UInt32 frames,AudioBufferList* output){
  auto& s=*static_cast<RenderState*>(context);for(UInt32 b=0;b<output->mNumberBuffers;++b)if(output->mBuffers[b].mData)std::fill_n(static_cast<float*>(output->mBuffers[b].mData),output->mBuffers[b].mDataByteSize/4,0.f);
  if(s.done)return noErr;s.maxSlice=std::max(s.maxSlice,int(frames));
  if(!s.checkedPolicy){thread_time_constraint_policy_data_t policy{};mach_msg_type_number_t count=THREAD_TIME_CONSTRAINT_POLICY_COUNT;boolean_t isDefault=FALSE;auto thread=mach_thread_self();s.realtimePolicy=thread_policy_get(thread,THREAD_TIME_CONSTRAINT_POLICY,reinterpret_cast<thread_policy_t>(&policy),&count,&isDefault)==KERN_SUCCESS&&!isDefault;mach_port_deallocate(mach_task_self(),thread);s.checkedPolicy=true;}
  s.credit+=frames;
  while(s.pos<s.total&&s.credit>=uint64_t(std::min(s.block,s.total-s.pos))){
   int n=std::min(s.block,s.total-s.pos);s.data.numSamples=n;s.events.clear();int off=s.total/2;
   if(s.pos==0||(s.pos<=off&&s.pos+n>off)){Event e{};bool on=s.pos==0;e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;e.busIndex=0;e.sampleOffset=on?0:off-s.pos;if(on){e.noteOn.pitch=60;e.noteOn.velocity=.8;e.noteOn.noteId=1;}else{e.noteOff.pitch=60;e.noteOff.noteId=1;}s.events.addEvent(e);}
   if(s.processor->process(s.data)!=kResultOk){s.error=true;s.done=true;break;}
   for(int i=0;i<n;++i){s.recording[(s.pos+i)*2]=s.left[i];s.recording[(s.pos+i)*2+1]=s.right[i];if(!std::isfinite(s.left[i])||!std::isfinite(s.right[i]))s.error=true;}
   s.pos+=n;s.credit-=n;
  }
  if(s.pos==s.total)s.done=true;return noErr;
 }
};
int main(int argc,char** argv){try{
 if(argc!=3)return 2;std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);if(!module){std::cerr<<error;return 1;}
 auto factory=module->getFactory();auto component=factory.createInstance<IComponent>(factory.classInfos()[0].ID());HostApplication host;if(!component||component->initialize(&host)!=kResultOk)return 1;
 int block=getenv("BRIDGELAB_TEST_BLOCK")?std::atoi(getenv("BRIDGELAB_TEST_BLOCK")):64;int rate=getenv("BRIDGELAB_TEST_RATE")?std::atoi(getenv("BRIDGELAB_TEST_RATE")):48000;if(block<1||block>4096||rate<44100||rate>192000)return 2;
 FUnknownPtr<IAudioProcessor> processor(component);ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=block;setup.sampleRate=rate;
 if(processor->setupProcessing(setup)!=kResultOk||component->setActive(true)!=kResultOk)return 1;component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);processor->setProcessing(true);
 int frames=getenv("BRIDGELAB_TEST_FRAMES")?std::atoi(getenv("BRIDGELAB_TEST_FRAMES")):960000;if(frames<4096||frames>48000*120)return 2;RenderState state(processor,frames,block);
 std::unique_ptr<DeviceEditor> editor;if(const char* name=getenv("BRIDGELAB_TEST_EDITOR"))editor=std::make_unique<DeviceEditor>(component,name);
 AudioComponentDescription description{};description.componentType=kAudioUnitType_Output;description.componentSubType=kAudioUnitSubType_DefaultOutput;description.componentManufacturer=kAudioUnitManufacturer_Apple;AudioComponent output=AudioComponentFindNext(nullptr,&description);AudioUnit unit=nullptr;
 OSStatus status=output?AudioComponentInstanceNew(output,&unit):-1;
 AudioStreamBasicDescription format{};format.mSampleRate=rate;format.mFormatID=kAudioFormatLinearPCM;format.mFormatFlags=kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved;format.mBytesPerPacket=4;format.mFramesPerPacket=1;format.mBytesPerFrame=4;format.mChannelsPerFrame=2;format.mBitsPerChannel=32;
 AURenderCallbackStruct callback{RenderState::callback,&state};
 if(!status)status=AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format));
 if(!status)status=AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback));
 if(!status)status=AudioUnitInitialize(unit);if(!status)status=AudioOutputUnitStart(unit);
 auto started=std::chrono::steady_clock::now();auto deadline=started+std::chrono::seconds(frames/rate+15);while(!status&&!state.done&&std::chrono::steady_clock::now()<deadline){if(editor&&!editor->changed&&std::chrono::steady_clock::now()-started>std::chrono::seconds(2)){try{editor->edit();}catch(const std::exception& e){std::cerr<<e.what()<<"\n";status=-1;break;}}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 if(unit){AudioOutputUnitStop(unit);AudioUnitUninitialize(unit);AudioComponentInstanceDispose(unit);}
 bool editorChanged=editor&&editor->changed;editor.reset();processor->setProcessing(false);component->setActive(false);component->terminate();
 if(status||!state.done||state.error){std::cerr<<"Core Audio render failed: "<<status<<" frames="<<state.pos<<"\n";return 1;}
 std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<char*>(state.recording.data()),state.recording.size()*sizeof(float));
 std::cout<<"Core Audio callback: frames="<<state.pos<<" max_slice="<<state.maxSlice<<" VST_block="<<block<<" rate="<<rate<<" realtime_policy="<<state.realtimePolicy<<" latency="<<processor->getLatencySamples()<<" editor_changed="<<editorChanged<<" device_output=silence\n";return file&&state.realtimePolicy?0:1;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
