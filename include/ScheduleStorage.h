#pragma once
#include "WeeklySchedule.h"
namespace frost {
// FBS1, bounded explicit little-endian fields + CRC32; no wall clock/commands.
std::optional<std::vector<uint8_t>> encodeScheduleState(const ScheduleState& state);
std::optional<ScheduleState> decodeScheduleState(const std::vector<uint8_t>& bytes);
}
