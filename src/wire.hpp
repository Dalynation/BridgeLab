#pragma once
#include <cstdint>
#include <cstddef>
constexpr uint32_t wireMagic=0x424c3032;
constexpr int quantum=256, maxMidi=128;
constexpr uint32_t maxStateBytes=16*1024*1024;
enum WireCommand:uint32_t {Audio=0,OpenEditor=1,CloseEditor=2,GetState=3,SetState=4,SetParameter=5,OpenEmbeddedEditor=6,EditorInput=7,GetStateWithProgram=8,SelectProgram=9,EditorStatus=10,EditorLifecycle=11,EditorResources=12,EditorGraphics=13};
struct WireMidi {uint32_t offset;uint8_t status,note,velocity,reserved;};
struct WireRequest {uint32_t magic=wireMagic,frames=quantum,count=0,program=0,command=Audio,payloadBytes=0;WireMidi midi[maxMidi]{};};
struct WireResponse {uint32_t magic=wireMagic,frames=quantum,status=1,payloadBytes=0;float left[quantum]{},right[quantum]{};};
static_assert(sizeof(WireMidi)==8,"wire MIDI layout");

constexpr uint32_t editorFrameMagic=0x424c4631,editorMaxWidth=2048,editorMaxHeight=2048;
constexpr size_t editorMapBytes=16+size_t(editorMaxWidth)*editorMaxHeight*4;
struct EditorInputEvent {uint32_t message;int32_t x,y;uint32_t value;};
