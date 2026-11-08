#ifdef FROSTBRIDGE_GATEWAY
#include "NvsScheduleStore.h"
namespace frost {
StoreLoad NvsScheduleStore::load() {
  open_=prefs_.begin("fb-plan",false);
  if(!open_) return {StoreStatus::Unavailable,{}};
  if(!prefs_.isKey("state")) return {StoreStatus::Empty,{}};
  const auto length=prefs_.getBytesLength("state");
  if(length<22 || length>134) { invalid_=true;return {StoreStatus::Invalid,{}}; }
  std::vector<uint8_t> bytes(length);
  if(prefs_.getBytes("state",bytes.data(),bytes.size())!=bytes.size()) { invalid_=true;return {StoreStatus::Invalid,{}}; }
  auto state=decodeScheduleState(bytes);invalid_=!state.has_value();hasState_=state.has_value();
  return {state?StoreStatus::Loaded:StoreStatus::Invalid,std::move(state)};
}
bool NvsScheduleStore::save(const ScheduleState& state) {
  const auto bytes=encodeScheduleState(state);
  if(!open_ || !bytes || prefs_.putBytes("state",bytes->data(),bytes->size())!=bytes->size()) return false;
  invalid_=false;hasState_=true;return true;
}
bool NvsScheduleStore::verifyCurrent(const ScheduleState& state) {
  // A diagnostic must not replace an unrecognized saved configuration.
  if(invalid_ || !save(state)) return false;
  const auto expected=encodeScheduleState(state);
  const auto length=prefs_.getBytesLength("state");
  if(!expected || length!=expected->size()) return false;
  std::vector<uint8_t> actual(length);
  return prefs_.getBytes("state",actual.data(),length)==length && actual==*expected && decodeScheduleState(actual).has_value();
}
}
#endif
