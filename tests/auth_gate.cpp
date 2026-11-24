#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include <algorithm>
#include <iostream>
using namespace Steinberg;using namespace Steinberg::Vst;
int main(int argc,char**argv){
 if(argc!=2)return 2;std::string error;
 auto module=VST3::Hosting::Module::create(argv[1],error);if(!module){std::cerr<<error;return 1;}
 auto factory=module->getFactory();auto classes=factory.classInfos();HostApplication host;
 auto component=factory.createInstance<IComponent>(classes[0].ID());
 if(!component||component->initialize(&host)!=kResultOk)return 1;
 FUnknownPtr<IAudioProcessor> processor(component);ProcessSetup setup{};setup.sampleRate=48000;setup.maxSamplesPerBlock=256;setup.symbolicSampleSize=kSample32;setup.processMode=kRealtime;
 if(!processor||processor->setupProcessing(setup)!=kResultOk)return 1;
 // Run before provisioning a native session. Metadata remains scannable while locked.
 if(component->setActive(true)==kResultOk){std::cerr<<"Expected logged-out activation denial";component->setActive(false);component->terminate();return 1;}
 float left[256],right[256];std::fill_n(left,256,1.f);std::fill_n(right,256,1.f);float* channels[]={left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
 ProcessData data{};data.symbolicSampleSize=kSample32;data.numSamples=256;data.numOutputs=1;data.outputs=&bus;
 if(processor->process(data)!=kResultOk)return 1;
 if(std::any_of(left,left+256,[](float x){return x!=0;})||std::any_of(right,right+256,[](float x){return x!=0;}))return 1;
 TUID controllerID;component->getControllerClassId(controllerID);auto controller=factory.createInstance<IEditController>(VST3::UID(controllerID));
 if(!controller||controller->initialize(&host)!=kResultOk)return 1;
 auto* view=controller->createView(ViewType::kEditor);if(!view)return 1;ViewRect rect{};view->getSize(&rect);view->release();controller->terminate();
 component->terminate();
 if(rect.right!=480||rect.bottom!=180)return 1;
 std::cout<<"PASS: logged-out scan metadata, activation denial, silent processing and login-required editor\n";
}
