#include "../vst2_abi.hpp"
#include "transport.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>
struct Instance {
 Effect effect{};Callback host;Transport transport;std::string path;std::vector<char> chunk;
 double rate=48000;uint32_t program=0;bool active=false,offline=false;uint64_t seq=0;int position=0;
 Packet pending,playing,waiting;bool hasWaiting=false;
 explicit Instance(Callback cb):host(cb){
 std::ifstream config(BRIDGE_ROOT "/selected-plugin.txt");std::getline(config,path);
 effect.magic=0x56737450;effect.dispatch=dispatch;effect.process=process;effect.replacing=process;effect.programs=512;effect.outputs=2;effect.flags=(1<<4)|(1<<5)|(1<<8);effect.delay=1024;effect.id=0x424c4142;effect.version=402;effect.object=this;effect.ratio=1;
 }
 static Instance& self(Effect* e){return *static_cast<Instance*>(e->object);}
 static intptr_t BCALL dispatch(Effect* e,int32_t op,int32_t index,intptr_t value,void* ptr,float opt){
 auto& s=self(e);
 switch(op){
 case 0:return 1;case 1:delete &s;return 1;
 case 2:s.program=std::clamp<intptr_t>(value,0,511);return 1;case 3:return s.program;
 case 5:if(ptr)std::snprintf((char*)ptr,24,"Program %u",s.program+1);return 1;
 case 10:s.rate=opt;return 1;case 11:return value>0;
 case 12:
 s.transport.stop();s.active=false;s.seq=0;s.position=0;s.pending={};s.playing={};s.hasWaiting=false;
 if(value){int bits=32;std::ifstream dll(s.path,std::ios::binary);uint32_t offset=0;uint16_t machine=0;dll.seekg(60);dll.read((char*)&offset,4);if(offset>16*1024*1024)return 0;dll.seekg(offset+4);dll.read((char*)&machine,2);bits=machine==0x8664?64:32;
 s.offline=s.host(e,23,0,0,nullptr,0)==4;s.active=s.transport.start(s.path,bits,s.rate,s.offline);}
 return value?s.active:1;
 case 23:s.chunk.assign(s.path.begin(),s.path.end());s.chunk.push_back('\0');for(int i=0;i<4;i++)s.chunk.push_back(char(s.program>>(8*i)));if(ptr)*static_cast<void**>(ptr)=s.chunk.data();return s.chunk.size();
 case 24:if(ptr&&value>5&&value<32768){const char* p=static_cast<const char*>(ptr);auto* end=(const char*)std::memchr(p,0,value-4);if(!end)return 0;s.path.assign(p,end);s.program=0;for(int i=0;i<4;i++)s.program|=uint32_t(uint8_t(end[1+i]))<<(8*i);s.program=std::min(s.program,511u);return 1;}return 0;
 case 25:if(ptr){auto* events=static_cast<Events*>(ptr);auto** list=events->items;for(int i=0;i<std::min(events->count,128);i++){auto* midi=list[i];if(midi&&midi->type==1&&s.pending.request.count<maxMidi){auto& m=s.pending.request.midi[s.pending.request.count++];m.offset=std::clamp(midi->delta,0,255);m.status=midi->data[0];m.note=midi->data[1];m.velocity=midi->data[2];}}}return 1;
 case 29:if(ptr)std::snprintf((char*)ptr,24,"Program %d",index+1);return 1;
 case 35:return 2;
 case 45:if(ptr)std::strcpy((char*)ptr,"BridgeLab Instrument");return 1;
 case 47:if(ptr)std::strcpy((char*)ptr,"BridgeLab");return 1;
 case 48:if(ptr)std::strcpy((char*)ptr,"BridgeLab Instrument");return 1;
 case 49:return 402;
 case 51:return ptr&&(!std::strcmp((char*)ptr,"receiveVstEvents")||!std::strcmp((char*)ptr,"receiveVstMidiEvent"));
 case 58:return 2400;case 71:case 72:return 1;default:return 0;}
 }
 static void BCALL process(Effect* e,float**,float** out,int32_t frames){
 auto& s=self(e);if(!out||!out[0]||!out[1]||frames<=0)return;
 if(!s.active||s.transport.failed){std::fill_n(out[0],frames,0);std::fill_n(out[1],frames,0);return;}
 // Preserve block MIDI until it is sent to the runner. VST2 timing is quantized
 // to the internal 256-frame quantum in this initial wrapper.
 for(int i=0;i<frames;i++){
 if(s.position==0){s.playing.response={};if(s.seq>=4){auto wanted=s.seq-4;while(true){if(!s.hasWaiting)s.hasWaiting=s.transport.responses.pop(s.waiting);if(!s.hasWaiting)break;if(s.waiting.seq<wanted){s.hasWaiting=false;continue;}if(s.waiting.seq==wanted){s.playing=s.waiting;s.hasWaiting=false;}break;}}}
 out[0][i]=s.playing.response.left[s.position];out[1][i]=s.playing.response.right[s.position];
 if(++s.position==256){s.pending.seq=s.seq++;s.pending.request.program=s.program;if(s.offline){if(s.transport.exchange(s.pending))s.transport.responses.push(s.pending);}else if(!s.transport.requests.push(s.pending))s.transport.failed=true;s.pending={};s.position=0;}
 }
 }
};
extern "C" __attribute__((visibility("default"))) Effect* VSTPluginMain(Callback cb){if(!cb||cb(nullptr,1,0,0,nullptr,0)==0)return nullptr;return &(new Instance(cb))->effect;}
extern "C" __attribute__((visibility("default"))) Effect* main_macho(Callback cb){return VSTPluginMain(cb);}
