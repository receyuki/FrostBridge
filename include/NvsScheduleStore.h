#pragma once
#ifdef FROSTBRIDGE_GATEWAY
#include "ScheduleStorage.h"
#include <Preferences.h>
namespace frost {
enum class StoreStatus { Empty,Loaded,Invalid,Unavailable };
struct StoreLoad { StoreStatus status;std::optional<ScheduleState> state; };
class NvsScheduleStore {
 public:
  StoreLoad load();
  bool save(const ScheduleState& state);
  bool verifyCurrent(const ScheduleState& state);
  StoreStatus status() const { return !open_?StoreStatus::Unavailable:invalid_?StoreStatus::Invalid:hasState_?StoreStatus::Loaded:StoreStatus::Empty; }
 private:
  Preferences prefs_;
  bool open_=false,invalid_=false,hasState_=false;
};
}
#endif
