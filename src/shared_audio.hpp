#pragma once
#include "wire.hpp"
#include <atomic>
#include <cstddef>
constexpr uint32_t sharedAudioMagic=0x424c4131,sharedAudioSlots=128;
struct alignas(8) SharedAudioRequest {uint64_t seq=0;WireRequest request;};
struct alignas(8) SharedAudioResponse {uint64_t seq=0;WireResponse response;};
struct alignas(64) SharedAudio {
 uint32_t magic=sharedAudioMagic,version=1,frames=quantum,slots=sharedAudioSlots;
 std::atomic<uint32_t> requestRead{0},requestWrite{0},responseRead{0},responseWrite{0};
 std::atomic<uint32_t> completed{0},stop{0},ready{0},error{0},pause{0},paused{0};std::atomic<uint32_t> scheduler{0};uint32_t reserved=0;
 SharedAudioRequest requests[sharedAudioSlots];SharedAudioResponse responses[sharedAudioSlots];
};
static_assert(std::atomic<uint32_t>::is_always_lock_free,"Shared audio requires lock-free indices");
static_assert(sizeof(SharedAudioRequest)==1056&&sizeof(SharedAudioResponse)==2072,"Shared audio packet ABI");
static_assert(offsetof(SharedAudio,requests)==64&&sizeof(SharedAudio)==400448,"Shared audio cross-architecture ABI");
