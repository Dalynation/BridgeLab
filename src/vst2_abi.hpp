#pragma once
#include <cstdint>
// Minimal ABI declarations, not a bundled vendor SDK. No pointers cross processes.
#ifdef _WIN32
#define BCALL __cdecl
#else
#define BCALL
#endif
struct Effect;
using Callback = intptr_t (BCALL *)(Effect*,int32_t,int32_t,intptr_t,void*,float);
using Process = void (BCALL *)(Effect*,float**,float**,int32_t);
struct Effect {
 int32_t magic; Callback dispatch; Process process;
 void (BCALL *setParameter)(Effect*,int32_t,float);
 float (BCALL *getParameter)(Effect*,int32_t);
 int32_t programs,parameters,inputs,outputs,flags;
 intptr_t reserved1,reserved2; int32_t delay,quality1,quality2; float ratio;
 void *object,*user; int32_t id,version; Process replacing;
 void (BCALL *doubleReplacing)(Effect*,double**,double**,int32_t); char future[56];
};
struct Midi {int32_t type=1,size=32,delta=0,flags=0,length=0,offset=0; unsigned char data[4]{}; char detune=0,off=0,reserved1=0,reserved2=0;};
struct Events {int32_t count=1; intptr_t reserved=0; Midi* items[2]{};};
static_assert(sizeof(Midi)==32,"MIDI ABI");
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(Effect)==144,"32-bit effect ABI");
#else
static_assert(sizeof(Effect)==192,"64-bit effect ABI");
#endif
