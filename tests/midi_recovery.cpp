#include "../src/plugin/midi_recovery.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{
 MidiRecovery recovery;WireRequest request;request.count=1;request.midi[0]={3,0x90,60,100,0};recovery.observe(0x90,60,100);
 recovery.prepare(request);if(request.count!=1||request.midi[0].offset!=3)throw std::runtime_error("normal MIDI changed");
 recovery.dropped();auto next=recovery.prepare(request);
 if(request.count!=17||request.midi[16].status!=0x80||request.midi[16].note!=60)throw std::runtime_error("explicit note-off missing for a CC120-ignoring synth");
 recovery.sent(next);next=recovery.prepare(request);
 if(request.count!=1||request.midi[0].status!=0x90||request.midi[0].note!=60)throw std::runtime_error("held note was not replayed in a later quantum");
 recovery.dropped();next=recovery.prepare(request);
 if(request.count!=1||request.midi[0].status!=0x90)throw std::runtime_error("rejected replay restarted already accepted release phase");
 recovery.observe(0x80,60,0);next=recovery.prepare(request);
 if(request.count!=17||request.midi[16].status!=0x80)throw std::runtime_error("released key not reconciled");
 recovery.sent(next);next=recovery.prepare(request);
 if(request.count)throw std::runtime_error("released key replayed");recovery.sent(next);
 recovery.clear();for(int c=0;c<16;c++)for(int n=0;n<128;n++)recovery.observe(0x90|c,n,100);recovery.dropped();
 bool seenOn[16][128]{},seenOff[16][128]{};int ons=0,offs=0,packets=0;bool replayStarted=false;
 do{
  next=recovery.prepare(request);if(request.count>128)throw std::runtime_error("packet overflow");bool on=false,off=false;
  for(uint32_t i=0;i<request.count;i++){auto m=request.midi[i];auto kind=m.status&0xf0;
   if(kind==0x90){on=true;if(seenOn[m.status&15][m.note])throw std::runtime_error("duplicate recovery note-on");seenOn[m.status&15][m.note]=true;++ons;}
   if(kind==0x80){off=true;if(replayStarted||seenOff[m.status&15][m.note])throw std::runtime_error("note-off order/duplication");seenOff[m.status&15][m.note]=true;++offs;}
  }
  if(on&&off)throw std::runtime_error("release and replay share a processing quantum");if(on)replayStarted=true;
  recovery.sent(next);++packets;
 }while(next<MidiRecovery::complete&&packets<40);
 if(ons!=2048||offs!=2048||packets!=33)throw std::runtime_error("bounded all-channel release/replay incomplete");
 std::cout<<"MIDI recovery: normal events unchanged, explicit note-offs, separate replay, released keys, rejected-send retry, all channels in bounded packets: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
