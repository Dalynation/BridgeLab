#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"
#include "base/source/fstring.h"
#include "config.hpp"
#include "transport.hpp"
#include "editor.hpp"
#include "midi_recovery.hpp"
#include "auth.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>
using namespace Steinberg;
using namespace Steinberg::Vst;
static const FUID uid=bridgeClassID(0);
static const FUID controllerUid=bridgeClassID(1);
static void trace(const char* message){std::ofstream out(bridgeHome()+"/logs/host.log",std::ios::app);out<<message<<"\n";}
class Bridge final:public AudioEffect {
 Transport transport;
 ZoneSixAuthorization authorization;
 std::vector<uint8_t> savedState,editorMapping;bool editorWanted=false;
 std::string path;std::atomic<uint32_t> program{0};bool active=false,offline=false;int bits=32;
 uint64_t underruns=0;uint64_t seq=0;int position=0;Packet pending,playing,waiting;bool hasWaiting=false;
 int delay=8;uint32_t programSteps=511;
 MidiRecovery midiRecovery;uint64_t overloadDrops=0;
 int architecture(){std::ifstream file(path,std::ios::binary);uint32_t off=0;uint16_t machine=0;file.seekg(60);file.read((char*)&off,4);if(!file||off>16*1024*1024)return 0;file.seekg(off+4);file.read((char*)&machine,2);return machine==0x14c?32:machine==0x8664?64:0;}
public:
 Bridge(){setControllerClass(controllerUid);trace("processor constructed");}
 static FUnknown* create(void*){trace("create processor");return static_cast<IAudioProcessor*>(new Bridge);}
 tresult PLUGIN_API initialize(FUnknown* context) override {
 trace("processor initialize");auto result=AudioEffect::initialize(context);if(result!=kResultOk)return result;
 addAudioOutput(STR16("Stereo"),SpeakerArr::kStereo);addEventInput(STR16("MIDI"));

 path=bridgePluginPath();programSteps=bridgeProgramSteps();
 return kResultOk;
 }
 tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32 numInputs,SpeakerArrangement* outputs,int32 numOutputs) override {
 return numInputs==0&&numOutputs==1&&outputs&&outputs[0]==SpeakerArr::kStereo?kResultTrue:kResultFalse;
 }
 tresult PLUGIN_API canProcessSampleSize(int32 size) override{return size==kSample32?kResultTrue:kResultFalse;}
 uint32 PLUGIN_API getLatencySamples() override{return delay*quantum;}
 uint32 PLUGIN_API getTailSamples() override{return kInfiniteTail;}
 tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
 if(active)return kResultFalse;
 if(setup.sampleRate<44100||setup.sampleRate>192000||setup.symbolicSampleSize!=kSample32||setup.maxSamplesPerBlock<1||setup.maxSamplesPerBlock>8192)return kResultFalse;
 // Cover an entire host callback plus worker scheduling headroom. Small
 // buffers retain scheduling safety; larger callbacks need more headroom.
 // Keep the scheduling allowance in time: a fixed sample count loses
 // headroom at higher sample rates. This does not depend on a host version.
 double allowance=2048.0/48000.0;
 int schedulingQuanta=int(std::ceil(setup.sampleRate*allowance/quantum));
 delay=std::max(std::max(8,schedulingQuanta),2*((setup.maxSamplesPerBlock+quantum-1)/quantum)+2);
 trace(("setup rate="+std::to_string(setup.sampleRate)+" maxBlock="+std::to_string(setup.maxSamplesPerBlock)+" mode="+std::to_string(setup.processMode)+" latency="+std::to_string(delay*quantum)).c_str());
 offline=setup.processMode==kOffline;return AudioEffect::setupProcessing(setup);
 }
 tresult PLUGIN_API setActive(TBool state) override {
 // Some hosts repeat activation without a matching deactivation. Preserve
 // the live runner, voices and editor instead of starting another Wine process.
 if(bool(state)==active&&!transport.failed&&(!state||authorization.permits()))return kResultOk;
 if(active){std::vector<uint8_t> captured;if(transport.control(GetStateWithProgram,{},captured,program))savedState=std::move(captured);}
 transport.stop();authorization.stop();active=false;seq=0;position=0;pending={};playing={};hasWaiting=false;midiRecovery.clear();
 if(!state)return kResultOk;
 if(!authorization.start()){trace("Zone Six login required");return kResultFalse;}
 bits=architecture();if(!bits)return kResultFalse;
 active=transport.start(path,bits,processSetup.sampleRate,offline);
 trace((std::string("transport=")+(transport.usesSharedAudio()?"shared":"pipe")+" pid="+std::to_string(getpid())+" scheduler="+(transport.usesRealtimeScheduler()?"1":"0")+" io_scheduler="+(transport.usesRealtimeIO()?"1":"0")).c_str());
 if(active){std::vector<uint8_t> ignored;if(!savedState.empty()&&!transport.control(SetState,savedState,ignored,program)){transport.stop();active=false;}else if(editorWanted&&!offline)transport.control(editorMapping.empty()?OpenEditor:OpenEmbeddedEditor,editorMapping,ignored);}
 return active?kResultOk:kResultFalse;
 }
 tresult PLUGIN_API setProcessing(TBool) override{return kResultOk;}
 tresult PLUGIN_API terminate() override{trace(("underrun quanta="+std::to_string(underruns)).c_str());trace(("overload dropped quanta="+std::to_string(overloadDrops)).c_str());authorization.stop();transport.stop();return AudioEffect::terminate();}
 tresult PLUGIN_API setState(IBStream* stream) override {
 IBStreamer in(stream,kLittleEndian);int32 length=0;int32 preset=0;
 if(!in.readInt32(length)||length<1||length>32768)return kResultFalse;
 std::string next(length,'\0');if(in.readRaw(next.data(),length)!=length||!in.readInt32(preset))return kResultFalse;
 path=bridgePluginPath().empty()?next:bridgePluginPath();program=std::clamp(preset,0,int(programSteps));savedState.clear();
 int32 marker=0,length2=0;if(in.readInt32(marker)){if(marker!=0x424c5354||!in.readInt32(length2)||length2<0||length2>int32(maxStateBytes))return kResultFalse;savedState.resize(length2);if(length2&&in.readRaw(savedState.data(),length2)!=length2)return kResultFalse;}
 if(active&&!savedState.empty()){std::vector<uint8_t> ignored;if(!transport.control(SetState,savedState,ignored,program))return kResultFalse;}
 return kResultOk;
 }
 tresult PLUGIN_API getState(IBStream* stream) override {
 if(active){std::vector<uint8_t> captured;if(!transport.control(GetStateWithProgram,{},captured,program))return kResultFalse;savedState=std::move(captured);}
 IBStreamer out(stream,kLittleEndian);
 bool ok=out.writeInt32(int32(path.size()))&&out.writeRaw(path.data(),int32(path.size()))==int32(path.size())&&out.writeInt32(program)&&out.writeInt32(0x424c5354)&&out.writeInt32(savedState.size());
 if(ok&&!savedState.empty())ok=out.writeRaw(savedState.data(),int32(savedState.size()))==int32(savedState.size());return ok?kResultOk:kResultFalse;
 }
 tresult PLUGIN_API notify(IMessage* message) override {
 if(message&&std::strcmp(message->getMessageID(),"BridgeLab.CloseEditor")&&!authorization.permits())return kResultFalse;
 if(message&&(!std::strcmp(message->getMessageID(),"BridgeLab.EmbedEditor")||!std::strcmp(message->getMessageID(),"BridgeLab.EditorInput"))){
 const void* bytes=nullptr;uint32 length=0;if(message->getAttributes()->getBinary("payload",bytes,length)!=kResultOk||length>32768)return kResultFalse;
 std::vector<uint8_t> payload((const uint8_t*)bytes,(const uint8_t*)bytes+length),ignored;
 bool embed=!std::strcmp(message->getMessageID(),"BridgeLab.EmbedEditor");if(embed){editorMapping=payload;editorWanted=true;}
 bool ok=active&&transport.control(embed?OpenEmbeddedEditor:EditorInput,payload,ignored);return ok?kResultOk:kResultFalse;
 }
 if(message&&!std::strcmp(message->getMessageID(),"BridgeLab.OpenEditor")){editorWanted=true;std::vector<uint8_t> ignored;bool ok=!active||transport.control(OpenEditor,{},ignored);trace(ok?"editor opened":"editor open FAILED");return ok?kResultOk:kResultFalse;}
 if(message&&!std::strcmp(message->getMessageID(),"BridgeLab.CloseEditor")){editorWanted=false;editorMapping.clear();std::vector<uint8_t> ignored;if(active)transport.control(CloseEditor,{},ignored);return kResultOk;}
 return AudioEffect::notify(message);
 }
 tresult PLUGIN_API process(ProcessData& data) override {
 // The audio thread only reads an atomic lease flag: no network, Keychain,
 // processes, locks or filesystem access occurs in this authorization check.
 if(!authorization.permits()){
  if(data.symbolicSampleSize==kSample32&&data.outputs)for(int bus=0;bus<data.numOutputs;++bus){
   auto& out=data.outputs[bus];if(out.channelBuffers32)for(int channel=0;channel<out.numChannels;++channel)if(out.channelBuffers32[channel])std::fill_n(out.channelBuffers32[channel],data.numSamples,0.f);
   out.silenceFlags=~uint64_t(0);
  }
  return kResultOk;
 }
 if(data.inputParameterChanges)for(int i=0;i<data.inputParameterChanges->getParameterCount();i++){auto* q=data.inputParameterChanges->getParameterData(i);int32 offset;ParamValue value;if(q&&(q->getParameterId()==0||q->getParameterId()==1)&&q->getPointCount()&&q->getPoint(q->getPointCount()-1,offset,value)==kResultOk)program=std::min(programSteps,uint32_t(std::clamp(value,0.0,1.0)*(q->getParameterId()==0?511:programSteps)+0.5));}
 // Preserve note flushes while a host has no audio buffer to process.
 // They belong at the next quantum position, not at an absent audio sample.
 if(data.numSamples==0||data.numOutputs==0||!data.outputs||data.outputs[0].numChannels==0){
 if(active&&data.inputEvents)for(int i=0;i<data.inputEvents->getEventCount();++i){Event e{};if(data.inputEvents->getEvent(i,e)==kResultOk&&(e.type==Event::kNoteOnEvent||e.type==Event::kNoteOffEvent)&&pending.request.count<maxMidi){auto& m=pending.request.midi[pending.request.count++];bool on=e.type==Event::kNoteOnEvent;m.offset=position;m.status=(on?0x90:0x80)|(on?e.noteOn.channel:e.noteOff.channel);m.note=on?e.noteOn.pitch:e.noteOff.pitch;m.velocity=uint8_t(std::clamp(on?e.noteOn.velocity:e.noteOff.velocity,0.f,1.f)*127);midiRecovery.observe(m.status,m.note,m.velocity);}}
 return kResultOk;
 }
 if(data.outputs[0].numChannels<2||data.symbolicSampleSize!=kSample32)return kResultFalse;
 float* left=data.outputs[0].channelBuffers32[0];float* right=data.outputs[0].channelBuffers32[1];
 if(!active||transport.failed){std::fill_n(left,data.numSamples,0);std::fill_n(right,data.numSamples,0);data.outputs[0].silenceFlags=3;return kResultOk;}
 data.outputs[0].silenceFlags=0;

 std::array<Event,128> events;int count=0;
 if(data.inputEvents)for(int i=0;i<data.inputEvents->getEventCount()&&count<128;i++){Event e;if(data.inputEvents->getEvent(i,e)==kResultOk&&(e.type==Event::kNoteOnEvent||e.type==Event::kNoteOffEvent))events[count++]=e;}
 std::sort(events.begin(),events.begin()+count,[](auto& a,auto& b){return a.sampleOffset<b.sampleOffset;});int next=0;
 for(int i=0;i<data.numSamples;i++){
 if(position==0){playing.response={};
 if(seq>=delay){auto wanted=seq-delay;bool received=false;
 while(true){if(!hasWaiting)hasWaiting=transport.receive(waiting);if(!hasWaiting)break;if(waiting.seq<wanted){hasWaiting=false;continue;}if(waiting.seq==wanted){playing=waiting;hasWaiting=false;received=true;}break;}
 if(!received)++underruns;
 if(received&&transport.timing.enabled()){float peak=0;for(float sample:playing.response.left)peak=std::max(peak,std::abs(sample));transport.timing.record('A',playing.seq,TimingTrace::now(),0,uint64_t(std::min(peak,1000.f)*1000000),0);}
 }}
 while(next<count&&events[next].sampleOffset<=i){auto& e=events[next++];if(pending.request.count<maxMidi){auto& m=pending.request.midi[pending.request.count++];m.offset=position;bool on=e.type==Event::kNoteOnEvent;m.status=(on?0x90:0x80)|(on?e.noteOn.channel:e.noteOff.channel);m.note=on?e.noteOn.pitch:e.noteOff.pitch;m.velocity=uint8_t(std::clamp(on?e.noteOn.velocity:e.noteOff.velocity,0.f,1.f)*127);midiRecovery.observe(m.status,m.note,m.velocity);}}
 left[i]=playing.response.left[position];right[i]=playing.response.right[position];
 if(++position==quantum){pending.seq=seq++;pending.request.program=program;
 if(offline){if(transport.exchange(pending))transport.responses.push(pending);}
 else{uint32_t next=midiRecovery.prepare(pending.request);bool submitted=transport.submit(pending,data.processMode==kPrefetch);
 if(transport.timing.enabled()){auto stamp=TimingTrace::now();transport.timing.record(submitted?'Q':'D',pending.seq,stamp,0,pending.request.count,next);if(submitted)for(uint32_t n=0;n<pending.request.count;++n){auto& m=pending.request.midi[n];transport.timing.record('M',pending.seq,stamp,0,(uint64_t(m.status)<<16)|(uint64_t(m.note)<<8)|m.velocity,m.offset);}}
 if(submitted)midiRecovery.sent(next);else if(!transport.failed){++overloadDrops;if(pending.request.count)midiRecovery.dropped();}}
 pending={};position=0;}
 }
 return kResultOk;
 }
};
class Controller final:public EditControllerEx1 {
public:
 IPlugView* PLUGIN_API createView(FIDString name) override {
  if(!name||std::strcmp(name,ViewType::kEditor))return nullptr;
  return zoneSixOnlineCheck()?makeBridgeEditor(this):makeZoneSixLockedView();
 }
 static FUnknown* create(void*){trace("create controller");return static_cast<IEditController*>(new Controller);}
 tresult PLUGIN_API initialize(FUnknown* context) override {
 trace("controller initialize");auto result=EditControllerEx1::initialize(context);
 if(result==kResultOk){
  // Keep ID 0 and its scaling for automation saved by previous versions.
  parameters.addParameter(STR16("Preset (legacy)"),nullptr,511,0,ParameterInfo::kCanAutomate|ParameterInfo::kIsHidden,0);
  if(!bridgePrograms().empty()){
   auto* list=new ProgramList(STR16("Preset"),1,kRootUnitId);
   for(const auto& name:bridgePrograms()){String128 title{};String(name.c_str(),kCP_Utf8).copyTo16(title,0,127);list->addProgram(title);}
   addUnit(new Unit(STR16("Instrument"),kRootUnitId,kNoParentUnitId,1));
   addProgramList(list);parameters.addParameter(list->getParameter());
  }
 }
 return result;
 }
 tresult PLUGIN_API getUnitByBus(MediaType type,BusDirection direction,int32 bus,int32 channel,UnitID& unit) override {
  if(!bridgePrograms().empty()&&type==kEvent&&direction==kInput&&bus==0&&channel>=0&&channel<16){unit=kRootUnitId;return kResultTrue;}
  return kResultFalse;
 }
 tresult PLUGIN_API setComponentState(IBStream* stream) override {
 IBStreamer in(stream,kLittleEndian);int32 length=0,preset=0;
 if(!in.readInt32(length)||length<1||length>32768)return kResultFalse;
 std::string path(length,'\0');if(in.readRaw(path.data(),length)!=length||!in.readInt32(preset))return kResultFalse;
 setParamNormalized(0,std::clamp(preset,0,511)/511.0);
 if(!bridgePrograms().empty())setParamNormalized(1,bridgeProgramSteps()?std::clamp(preset,0,int(bridgeProgramSteps()))/double(bridgeProgramSteps()):0);
 return kResultOk;
 }
};
bool InitModule(){trace("bundle entry");return true;}bool DeinitModule(){trace("bundle exit");return true;}
BEGIN_FACTORY_DEF("BridgeLab","https://example.invalid","")
DEF_CLASS2(INLINE_UID_FROM_FUID(uid),PClassInfo::kManyInstances,kVstAudioEffectClass,bridgeDisplayName(),0,"Instrument|Synth","0.10.3",kVstVersionString,Bridge::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(controllerUid),PClassInfo::kManyInstances,kVstComponentControllerClass,"BridgeLab Controller",0,"","0.10.3",kVstVersionString,Controller::create)
END_FACTORY
