#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include <utility>
#include "FridgeController.h"
#include <functional>
#include <optional>
namespace frost {
enum class AutomationHealth : uint8_t { Unknown,Healthy,Expired };
enum class DayGap : uint8_t { None,PartialStart,TimeUnknown,ClockChanged,InputGap,LinkUnavailable,Persistence,Rejected,Failed,Overflow,PendingAtBoundary,DeviceUnavailable };
struct DailyInputs { uint32_t nowMs=0;std::optional<int64_t> wallMs;AutomationHealth link=AutomationHealth::Unknown;bool networkReady=false,deviceReady=true; };
enum class DailyField:uint8_t { Temperature,Power,Mode };
enum class DailyOutcome:uint8_t { Pending,Confirmed,Failed };
struct DailyAction {uint32_t second=0;DailyField field=DailyField::Temperature;int8_t value=0;uint16_t order=0;};
static_assert(sizeof(DailyAction)==8,"Explicit daily action RAM budget");
struct DailyRecord {DailyAction action;DailyOutcome outcome=DailyOutcome::Pending;};
struct LearningToken {uint32_t candidate=0;uint8_t index=0;uint32_t admittedMs=0;};
struct LearningCompletion {uint32_t processedMs=0;std::optional<int64_t> confirmedWallMs;};
struct DailySample { bool valid=false;int32_t day=0;uint8_t count=0;std::array<DailyAction,64> actions{}; };
struct DailyPersistedState { bool enabled=false,manual=false;uint32_t generation=0;std::optional<int64_t> overrideUntilMs;DailySample sample; };
enum class ReplayMode:uint8_t { Disabled,WaitingForTime,HomeKitPrimary,Learning,CheckingLink,ManualOverride,Replaying };
struct ReplayDecision {ReplayMode mode=ReplayMode::Disabled;uint8_t mask=0;std::array<int8_t,3> values{};std::optional<int64_t> nextActionMs;};
inline bool operator==(const ReplayDecision& a,const ReplayDecision& b){return a.mode==b.mode && a.mask==b.mask && a.values==b.values && a.nextActionMs==b.nextActionMs;}
struct DailyCandidate { int32_t day=0;DayGap gap=DayGap::PartialStart;uint8_t count=0; };
// Single BLE-worker owner. Inputs carry trusted wall time, or null after a cold
// boot. Save is bounded, non-reentrant and never called from an HAP callback.
// A successful save is required before replacing the persistent sample/config.
class DailyReplay {
 public:
  using Save=std::function<bool(const DailyPersistedState&)>;
  explicit DailyReplay(Save save,uint32_t bootMs=0):save_(std::move(save)),bootMs_(bootMs){}
  bool restore(const DailyPersistedState& state);
  bool restoreEncoded(const uint8_t* bytes,size_t size);
  static bool validState(const DailyPersistedState& state);
  // An explicit confirmation saves synchronously even if the value matches
  // RAM, so unavailable/uncertain storage cannot be acknowledged as saved.
  // A false result leaves the
  // effective RAM configuration unchanged and sets storageHealthy() false.
  // A failed readback may still have committed to flash; report it as unconfirmed.
  bool configure(bool enabled,const DailyInputs& inputs,std::optional<uint32_t> processedMs=std::nullopt);
  // Feed chronological, drained admission/fence inputs at least every 5 s.
  // Larger gaps invalidate coverage. Never advance to owner wall time before
  // older mailbox inputs have drained. A day seal may synchronously call Save.
  void advance(const DailyInputs& inputs);
  // Adapter-reported known loss. Does not advance/seal time or clear a gap.
  void invalidateCurrent(DayGap gap){if(state_.enabled && gap!=DayGap::None)today_.gap=gap;}
  std::optional<LearningToken> capture(Intent intent,const DailyInputs& inputs);
  // true consumes a pending token, not necessarily a successful NVS write.
  // confirmed is a Controller fresh-readback outcome, never queue acceptance.
  // May synchronously persist the end of an override; check storageHealthy().
  // Supply completion time for asynchronous readback: a post-midnight or
  // undatable confirmation cannot certify the previous day as complete.
  bool complete(LearningToken token,bool confirmed,std::optional<LearningCompletion> completion=std::nullopt);
  // Real confirmed automation is still a boundary when time/record capacity
  // prevents learning. Does not invent a record or certify day coverage.
  bool confirmAutomation(uint32_t admittedMs,uint32_t processedMs);
  // Save must succeed before an unapplied manual command is admitted. With
  // alreadyApplied=true a failed save still establishes a RAM override, returns
  // false and retries at most every 5 s; this respects observed panel changes.
  // A delayed panel observation keeps its ingress time in inputs; processedMs
  // is the actual save-attempt time used for retry backoff.
  bool manualOverride(const DailyInputs& inputs,bool alreadyApplied=false,std::optional<uint32_t> processedMs=std::nullopt);
  // Pure read: no sealing, time advancement, persistence or device operations.
  ReplayDecision decision(const DailyInputs& inputs)const;
  ReplayDecision preview(bool enabled,const DailyInputs& inputs)const;
  std::optional<DailyRecord> record(uint8_t index,bool sample) const;
  const DailyPersistedState& state() const{return state_;}
  DailyCandidate today() const{return today_;}
  std::optional<DailyCandidate> lastClosed()const{return lastClosed_;}
  bool storageHealthy()const{return !storageFault_;}
 private:
  Save save_;DailyPersistedState state_;DailyCandidate today_;std::optional<DailyCandidate> lastClosed_;
  std::array<DailyRecord,64> records_{};uint32_t candidateSerial_=0;
  uint32_t manualCandidate_=0;uint8_t manualAfter_=0;
  std::optional<uint32_t> manualAtMs_;
  uint32_t bootMs_=0;
  bool haveTime_=false,previousReady_=false;uint32_t lastNowMs_=0;int64_t lastWallMs_=0;
  bool storageFault_=false,retryPending_=false,dirty_=false;uint32_t retryAtMs_=0;
  bool commit(const DailyPersistedState& next,uint32_t nowMs);
  bool endOverride(uint32_t nowMs);
  void resetAfterRestore();
  ReplayDecision evaluate(bool enabled,bool manual,const DailyInputs& inputs)const;
};
}
