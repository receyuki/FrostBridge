#pragma once
#ifdef FROSTBRIDGE_GATEWAY
#include "DailyJournal.h"
#include <Preferences.h>
namespace frost {
enum class DailyStoreStatus:uint8_t {Empty,Loaded,Invalid,Unavailable};
// BLE-worker only, synchronous Preferences/NVS. Uses only fb-daily/slot0/slot1.
// An unrecognized pair remains untouched. A valid older slot remains loadable
// when a newer slot is torn. No raw credential/config values leave this owner.
class NvsDailyStore {
 public:
  NvsDailyStore();
  NvsDailyStore(const NvsDailyStore&)=delete;
  NvsDailyStore& operator=(const NvsDailyStore&)=delete;
  // Physical maximum-size diagnostic uses only fb-dtest keys, never a sample.
  static bool verifyCapacity();
  DailyStoreStatus load(DailyReplay& owner);
  bool save(const DailyPersistedState& state);
  DailyStoreStatus status()const{return status_;}
 private:
  Preferences prefs_;DailyJournal journal_;
  bool open_=false,writable_=false;DailyStoreStatus status_=DailyStoreStatus::Unavailable;
};
}
#endif
