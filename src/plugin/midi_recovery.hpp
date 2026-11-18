#pragma once
#include "../wire.hpp"
#include <cstring>
// Bounded MIDI recovery after a temporary audio-queue stall. A full queue must
// not permanently mute an otherwise healthy runner or lose held-key state.
class MidiRecovery {
 uint8_t held[16][128]{};bool touched[16][128]{};bool recovering=false,releasing=true;uint32_t cursor=0;
public:
 static constexpr uint32_t complete=4096;
 void clear(){std::memset(held,0,sizeof(held));std::memset(touched,0,sizeof(touched));recovering=false;releasing=true;cursor=0;}
 void observe(uint8_t status,uint8_t note,uint8_t velocity){
  if(note>=128||((status&0xf0)!=0x90&&(status&0xf0)!=0x80))return;
  touched[status&15][note]=true;
  auto value=(status&0xf0)==0x90?velocity:uint8_t(0);auto& previous=held[status&15][note];
  if(previous==value)return;previous=value;if(recovering){cursor=0;releasing=true;}
 }
 // A rejected recovery packet was never sent: retry its current phase rather
 // than replaying already accepted notes again. Empty audio loss needs no MIDI
 // recovery at all; the caller only invokes this for a packet containing MIDI.
 void dropped(){if(!recovering){recovering=true;releasing=true;cursor=0;}}
 uint32_t prepare(WireRequest& request)const{
  if(!recovering)return complete;request.count=0;
  if(releasing&&cursor==0)for(uint8_t channel=0;channel<16;channel++)request.midi[request.count++]={0,uint8_t(0xb0|channel),120,0,0};
  uint32_t next=cursor;
  // Some instruments ignore CC120. Release every observed key explicitly,
  // including a key whose note-off was dropped, before replaying held notes in
  // a later processing quantum. Never depend on panic/replay event ordering.
  while(next<2048&&request.count<maxMidi){uint8_t channel=next/128,note=next%128;auto velocity=held[channel][note];++next;if(releasing?touched[channel][note]:velocity!=0)request.midi[request.count++]={0,uint8_t((releasing?0x80:0x90)|channel),note,releasing?uint8_t(0):velocity,0};}
  return (releasing?0:2048)+next;
 }
 void sent(uint32_t next){if(!recovering)return;if(releasing){cursor=next;if(cursor==2048){releasing=false;cursor=0;}}else{cursor=next-2048;if(next==complete)recovering=false;}}
};
