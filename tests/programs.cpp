#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "base/source/fstreamer.h"
#include <iostream>
using namespace Steinberg;using namespace Steinberg::Vst;
int main(int argc,char** argv){
 if(argc!=2)return 2;std::string error;
 auto module=VST3::Hosting::Module::create(argv[1],error);if(!module){std::cerr<<error;return 1;}
 HostApplication host;IPtr<IEditController> controller;
 for(const auto& info:module->getFactory().classInfos())if(info.category()==kVstComponentControllerClass)controller=module->getFactory().createInstance<IEditController>(info.ID());
 if(!controller||controller->initialize(&host)!=kResultOk)return 1;
 FUnknownPtr<IUnitInfo> units(controller);ProgramListInfo list{};String128 name{};ParameterInfo parameter{};
 bool ok=units&&units->getProgramListCount()==1&&units->getProgramListInfo(0,list)==kResultOk&&list.id==1&&list.programCount==3;
 ok=ok&&units->getProgramName(1,1,name)==kResultOk&&std::u16string(name)==u"Second preset";
 ok=ok&&controller->getParameterCount()==2&&controller->getParameterInfo(1,parameter)==kResultOk&&parameter.id==1&&parameter.stepCount==2&&(parameter.flags&ParameterInfo::kIsProgramChange);
 UnitInfo unit{};ok=ok&&units->getUnitInfo(0,unit)==kResultOk&&unit.id==kRootUnitId&&unit.programListId==1;
 UnitID busUnit=-1;
 ok=ok&&units->getUnitByBus(kEvent,kInput,0,0,busUnit)==kResultOk&&busUnit==kRootUnitId;
 ok=ok&&units->getUnitByBus(kEvent,kInput,0,15,busUnit)==kResultOk&&units->getUnitByBus(kAudio,kInput,0,0,busUnit)!=kResultOk;
 MemoryStream state;IBStreamer stream(&state,kLittleEndian);stream.writeInt32(4);stream.writeRaw("test",4);stream.writeInt32(2);state.seek(0,IBStream::kIBSeekSet,nullptr);
 ok=ok&&controller->setComponentState(&state)==kResultOk&&controller->getParamNormalized(1)==1&&controller->getParamNormalized(0)==2.0/511;
 controller->terminate();std::cout<<(ok?"PASS":"FAIL")<<": original preset names, VST3 program list, selection scaling and legacy state\n";return ok?0:1;
}
