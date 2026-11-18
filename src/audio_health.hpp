#pragma once
#include "wire.hpp"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

// Observation only: no plugin calls, output writes, allocations, locks or I/O.
// One DSP writer publishes bounded 32-bit snapshots for the non-realtime UI.
class AudioHealth {
public:
 enum Field {Blocks,NoteOns,HeldKeys,MeanSquare,Peak,Quiet,Invalid,AboveRecent,AboveTotal,Rate,Count};
 using Snapshot=std::array<uint32_t,Count>;
 static float number(uint32_t bits)noexcept{float value;std::memcpy(&value,&bits,4);return value;}
private:
 static_assert(std::atomic<uint32_t>::is_always_lock_free,"Audio health requires lock-free counters");
 unsigned rate;std::array<std::array<bool,128>,16> held{};
 uint32_t blocks=0,noteOns=0,heldKeys=0,invalid=0,aboveTotal=0,aboveRecent=0;
 uint32_t windowFrames=0,windowAbove=0,quietFrames=0,idleFrames=0;
 double energy=0;float windowPeak=0,meanSquare=0,peak=0;bool quiet=false;
 std::atomic<uint32_t> generation{0};std::array<std::atomic<uint32_t>,Count> published;
 static uint32_t add(uint32_t a,uint32_t b)noexcept{return a+std::min(b,UINT32_MAX-a);}
 static uint32_t bits(float value)noexcept{uint32_t result;std::memcpy(&result,&value,4);return result;}
 void publish()noexcept{
  Snapshot values{blocks,noteOns,heldKeys,bits(meanSquare),bits(peak),uint32_t(quiet),invalid,aboveRecent,aboveTotal,rate};
  generation.fetch_add(1,std::memory_order_acq_rel);
  for(size_t i=0;i<Count;++i)published[i].store(values[i],std::memory_order_relaxed);
  generation.fetch_add(1,std::memory_order_release);
 }
public:
 explicit AudioHealth(unsigned sampleRate=48000):rate(std::max(1u,sampleRate)){
  for(auto& value:published)value.store(0,std::memory_order_relaxed);publish();
 }
 void observe(const WireRequest& request,const float* left,const float* right)noexcept{
  for(uint32_t i=0;i<std::min(request.count,uint32_t(maxMidi));++i){
   auto m=request.midi[i];unsigned channel=m.status&15,kind=m.status&0xf0;
   if((kind==0x90||kind==0x80)&&m.note<128){
    bool on=kind==0x90&&m.velocity;
    if(on)noteOns=add(noteOns,1);
    if(held[channel][m.note]!=on){held[channel][m.note]=on;if(on)++heldKeys;else --heldKeys;}
   }else if(kind==0xb0&&(m.note==120||m.note==123)){
    for(auto& key:held[channel])if(key){key=false;--heldKeys;}
   }
  }
  const uint32_t frames=std::min(request.frames,uint32_t(quantum));
  double blockEnergy=0;uint32_t blockInvalid=0;
  for(uint32_t i=0;i<frames;++i)for(float sample:{left[i],right[i]}){
   if(!std::isfinite(sample)){++blockInvalid;continue;}
   blockEnergy+=double(sample)*sample;windowPeak=std::max(windowPeak,std::abs(sample));
   if(std::abs(sample)>1.0f){++windowAbove;aboveTotal=add(aboveTotal,1);}
  }
  invalid=add(invalid,blockInvalid);energy+=blockEnergy;windowFrames+=frames;blocks=add(blocks,1);
  // -55 dBFS for at least 0.75 seconds of held-note audio. This is advice,
  // not failure: filters, low gain and decaying envelopes can be intentional.
  constexpr double quietPower=3.162277660168379e-6;
  if(heldKeys){idleFrames=0;if(!blockInvalid&&blockEnergy<std::max(1u,frames)*2*quietPower)quietFrames=std::min(rate,quietFrames+frames);else quietFrames=0;}
  else {idleFrames=std::min(rate,idleFrames+frames);if(idleFrames>rate/2)quietFrames=0;}
  quiet=heldKeys&&quietFrames>=rate*3/4;
  if(windowFrames>=std::max(1u,rate/2)){
   meanSquare=float(std::min(energy/(windowFrames*2.0),double(std::numeric_limits<float>::max())));
   peak=windowPeak;aboveRecent=windowAbove;energy=0;windowFrames=windowAbove=0;windowPeak=0;
  }
  publish();
 }
 Snapshot snapshot()const noexcept{
  Snapshot result{};
  for(unsigned attempt=0;attempt<3;++attempt){
   auto before=generation.load(std::memory_order_acquire);
   for(size_t i=0;i<Count;++i)result[i]=published[i].load(std::memory_order_relaxed);
   if(!(before&1)&&generation.load(std::memory_order_acquire)==before)break;
  }
  return result;
 }
};
