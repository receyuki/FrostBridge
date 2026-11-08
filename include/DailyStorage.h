#pragma once
#include "DailyReplay.h"
#include <array>
#include <cstddef>
namespace frost {
inline constexpr size_t dailyStateCapacity=580;
using DailyBytes=std::array<uint8_t,dailyStateCapacity>;
// FBD1: fixed 64-byte header, up to64 explicit 8-byte actions, CRC32.
// Neither function allocates. Invalid input leaves the output unchanged.
struct DailyStateStamp {uint32_t generation=0,checksum=0;};
// Envelope/CRC validation only; restoring a sample also validates domain fields.
std::optional<DailyStateStamp> dailyStateStamp(const uint8_t* bytes,size_t size);
std::optional<size_t> encodeDailyState(const DailyPersistedState& state,DailyBytes& output);
bool decodeDailyState(const uint8_t* bytes,size_t size,DailyPersistedState& output);
}
