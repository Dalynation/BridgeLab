#include "../src/audio_health.hpp"
#include <iostream>
#include <limits>
#include <cassert>
static void run(AudioHealth& health,WireRequest& request,float* left,float* right,unsigned blocks){for(unsigned i=0;i<blocks;++i){health.observe(request,left,right);request.count=0;}}
static void note(WireRequest& request,uint8_t status,uint8_t velocity=100){request.count=1;request.midi[0]={0,status,60,velocity,0};}
int main(){
 for(unsigned rate:{44100u,48000u,192000u}){
  AudioHealth health(rate);WireRequest request;float left[quantum]{},right[quantum]{};
  unsigned second=(rate+quantum-1)/quantum;
  run(health,request,left,right,second);assert(!health.snapshot()[AudioHealth::Quiet]);
  note(request,0x90);run(health,request,left,right,second);
  auto quiet=health.snapshot();assert(quiet[AudioHealth::HeldKeys]==1&&quiet[AudioHealth::Quiet]&&quiet[AudioHealth::NoteOns]==1);
  for(int i=0;i<quantum;++i)left[i]=right[i]=.1f;
  run(health,request,left,right,second);auto audible=health.snapshot();assert(!audible[AudioHealth::Quiet]&&AudioHealth::number(audible[AudioHealth::MeanSquare])>.009f);
  note(request,0x90);run(health,request,left,right,1);assert(health.snapshot()[AudioHealth::HeldKeys]==1);
  note(request,0x80,0);run(health,request,left,right,1);assert(!health.snapshot()[AudioHealth::HeldKeys]&&!health.snapshot()[AudioHealth::Quiet]);
  // A release tail remains valid audio even though no keys are held.
  run(health,request,left,right,second);assert(!health.snapshot()[AudioHealth::Quiet]);
  note(request,0x91);run(health,request,left,right,1);note(request,0xb1,0);request.midi[0].note=123;run(health,request,left,right,1);assert(!health.snapshot()[AudioHealth::HeldKeys]);
  left[0]=std::numeric_limits<float>::quiet_NaN();right[0]=std::numeric_limits<float>::infinity();left[1]=1.2f;
  run(health,request,left,right,second);auto bad=health.snapshot();assert(bad[AudioHealth::Invalid]>0&&bad[AudioHealth::AboveRecent]>0);
  assert(std::isnan(left[0])&&std::isinf(right[0])&&left[1]==1.2f); // Observer never edits output.
  for(int i=0;i<quantum;++i)left[i]=right[i]=.1f;
  run(health,request,left,right,second*2);assert(!health.snapshot()[AudioHealth::AboveRecent]);
  std::cout<<"PASS rate="<<rate<<": quiet held notes, recovery, idle/tail distinction, channels, invalid/over-range audio, unchanged samples\n";
 }
}
